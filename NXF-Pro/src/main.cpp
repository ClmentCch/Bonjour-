/*
 * NXF Pro - Nintendo Switch entry point.
 *
 * Owns the framebuffer, the pad/touch input, the software keyboard and the
 * NXF_Save/pref.json preferences file.
 *
 * MIT licensed, see LICENSE.
 */
#include <switch.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "nxf/app.hpp"
#include "nxf/font.hpp"
#include "nxf/i18n.hpp"
#include "nxf/image.hpp"
#include "nxf/platform_switch.hpp"

#include "font_light_bin.h"
#include "font_medium_bin.h"
#include "font_regular_bin.h"
#include "logo_bin.h"

namespace {

constexpr const char* kSaveDir = "sdmc:/NXF_Save";
constexpr const char* kPrefsPath = "sdmc:/NXF_Save/pref.json";
constexpr float kMoveDelay = 0.36f;
constexpr float kMoveRepeat = 0.075f;
constexpr float kStickDeadzone = 0.55f;

/// Append-only diagnostic log, useful when a console misbehaves.
void debugLog(const std::string& message) {
    static std::FILE* file = nullptr;
    static bool opened = false;
    if (!opened) {
        opened = true;
        mkdir("sdmc:/NXF_Save", 0777);
        file = std::fopen("sdmc:/NXF_Save/debug.log", "wb");
    }
    if (file != nullptr) {
        std::fprintf(file, "%s\n", message.c_str());
        std::fflush(file);
    }
}

std::string trimmed(const std::string& text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }
    while (end > begin && (text[end - 1] == '\r' || text[end - 1] == '\n' || text[end - 1] == ' ' ||
                           text[end - 1] == '\t')) {
        --end;
    }
    return text.substr(begin, end - begin);
}

/// Minimal JSON reader: finds "key": <string|number|true|false>.
bool jsonValue(const std::string& json, const std::string& key, std::string& out) {
    const std::string pattern = "\"" + key + "\"";
    std::size_t pos = json.find(pattern);
    if (pos == std::string::npos) {
        return false;
    }
    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) {
        return false;
    }
    ++pos;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n')) {
        ++pos;
    }
    if (pos >= json.size()) {
        return false;
    }
    if (json[pos] == '"') {
        const std::size_t end = json.find('"', pos + 1);
        if (end == std::string::npos) {
            return false;
        }
        out = json.substr(pos + 1, end - pos - 1);
        return true;
    }
    const std::size_t end = json.find_first_of(",}\n", pos);
    out = trimmed(json.substr(pos, end - pos));
    return !out.empty();
}

class SwitchPlatform final : public nxf::Platform {
public:
    bool promptText(const std::string& header, const std::string& initial,
                    std::string& out) override {
        SwkbdConfig config;
        if (R_FAILED(swkbdCreate(&config, 0))) {
            return false;
        }
        swkbdConfigMakePresetDefault(&config);
        swkbdConfigSetHeaderText(&config, header.c_str());
        swkbdConfigSetGuideText(&config, header.c_str());
        swkbdConfigSetStringLenMax(&config, 128);
        if (!initial.empty()) {
            swkbdConfigSetInitialText(&config, initial.c_str());
        }

        std::vector<char> buffer(512, '\0');
        const Result rc = swkbdShow(&config, buffer.data(), buffer.size());
        swkbdClose(&config);
        if (R_FAILED(rc)) {
            return false;
        }
        out = buffer.data();
        return true;
    }

    void settingsChanged(const nxf::SettingsState& settings) override {
        nxf::savePrefs(kPrefsPath, settings);
    }

    int autoRcmState() override {
        FsStorage storage;
        if (R_FAILED(fsOpenBisStorage(&storage, FsBisPartitionId_BootPartition1Root))) {
            return -1;
        }
        std::uint8_t sector[0x200] = {};
        const bool read = R_SUCCEEDED(fsStorageRead(&storage, 0, sector, sizeof(sector)));
        fsStorageClose(&storage);
        if (!read) {
            return -1;
        }
        return (sector[0] == 'B' && sector[1] == 'C' && sector[2] == 'T' && sector[3] == '0') ? 0 : 1;
    }

    bool savePreferences(const nxf::SettingsState& settings) override {
        return nxf::savePrefs(kPrefsPath, settings);
    }
};

} // namespace

namespace nxf {

bool prefsExist(const std::string& path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0;
}

bool loadPrefs(const std::string& path, SettingsState& out) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }
    std::string text;
    char buffer[512];
    std::size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        text.append(buffer, read);
    }
    std::fclose(file);

    std::string value;
    if (jsonValue(text, "language", value)) {
        out.language = langFromCode(trimmed(value));
    }
    if (jsonValue(text, "ftpPort", value)) {
        const int port = std::atoi(value.c_str());
        if (port > 0 && port < 65536) {
            out.ftpPort = port;
        }
    }
    if (jsonValue(text, "ftpUser", value) && !value.empty()) {
        out.ftpUser = value;
    }
    if (jsonValue(text, "ftpPassword", value)) {
        out.ftpPassword = value;
    }
    if (jsonValue(text, "animations", value)) {
        out.animations = value != "false";
    }
    if (jsonValue(text, "grid", value)) {
        out.gridDefault = value == "true";
    }
    return true;
}

bool savePrefs(const std::string& path, const SettingsState& settings) {
    mkdir("sdmc:/NXF_Save", 0777);
    (void)kSaveDir;

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        return false;
    }
    std::fprintf(file,
                 "{\n"
                 "  \"language\": \"%s\",\n"
                 "  \"ftpPort\": %d,\n"
                 "  \"ftpUser\": \"%s\",\n"
                 "  \"ftpPassword\": \"%s\",\n"
                 "  \"animations\": %s,\n"
                 "  \"grid\": %s\n"
                 "}\n",
                 langCode(settings.language), settings.ftpPort, settings.ftpUser.c_str(),
                 settings.ftpPassword.c_str(), settings.animations ? "true" : "false",
                 settings.gridDefault ? "true" : "false");
    std::fclose(file);
    return true;
}

} // namespace nxf

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // The user NAND partition is only reachable from a full-memory application;
    // in applet mode the mount fails and the explorer reports it as unavailable.
    FsFileSystem bis;
    if (R_SUCCEEDED(fsOpenBisFileSystem(&bis, FsBisPartitionId_User, ""))) {
        fsdevMountDevice("nand", bis);
    }

    socketInitializeDefault();
    nifmInitialize(NifmServiceType_User);

    // libnx v4 requires the supported styles to be declared before any
    // PadState is created, otherwise padUpdate() reports nothing.
    hidInitialize();
    hidInitializeTouchScreen();
    debugLog("hid initialized");

    nxf::FontSet fonts;
    fonts.load(nxf::Weight::Light, font_light_bin, font_light_bin_size);
    fonts.load(nxf::Weight::Regular, font_regular_bin, font_regular_bin_size);
    fonts.load(nxf::Weight::Medium, font_medium_bin, font_medium_bin_size);

    nxf::Image logo;
    nxf::loadImageFromMemory(logo_bin, logo_bin_size, logo);

    nxf::SettingsState settings;
    const bool hasPrefs = nxf::prefsExist(kPrefsPath) && nxf::loadPrefs(kPrefsPath, settings);
    nxf::setLang(settings.language);

    std::unique_ptr<nxf::StorageApi> sd = nxf::openSdStorage();
    std::unique_ptr<nxf::StorageApi> internal = nxf::openInternalStorage();
    std::unique_ptr<nxf::FtpController> ftp = nxf::createFtpServer(*sd, *internal);
    std::unique_ptr<nxf::TitleApi> titles = nxf::createTitleApi();

    SwitchPlatform platform;
    nxf::App app(fonts, platform, *ftp, *sd, *internal, *titles);
    app.applySettings(settings);
    app.setDefaultView(settings.gridDefault ? nxf::ViewMode::Grid : nxf::ViewMode::List);
    if (logo.valid()) {
        app.setLogo(&logo);
    }
    app.setInitialPath(sd->root());
    if (!hasPrefs) {
        app.showLanguageSetup();
    }

    Framebuffer framebuffer;
    if (R_FAILED(framebufferCreate(&framebuffer, nwindowGetDefault(), nxf::layout::kScreenWidth,
                                   nxf::layout::kScreenHeight, PIXEL_FORMAT_RGBA_8888, 2))) {
        ftp->stop();
        nifmExit();
        socketExit();
        return 1;
    }
    framebufferMakeLinear(&framebuffer);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    debugLog("pad ready");

    nxf::InputState input;
    input.dt = 1.0f / 60.0f;

    int lastDirection = -1;
    float repeatTimer = 0.0f;
    u64 previousButtons = 0;
    u64 loggedButtons = 0;
    int loggedPulses = 0;
    bool loggedFirstFrame = false;
    bool touchWasDown = false;

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 held = padGetButtons(&pad);
        const u64 pressed = held & ~previousButtons;
        previousButtons = held;
        const HidAnalogStickState stick = padGetStickPos(&pad, 0);

        if (!loggedFirstFrame) {
            loggedFirstFrame = true;
            char line[96];
            std::snprintf(line, sizeof(line), "first frame: held=0x%llX",
                          static_cast<unsigned long long>(held));
            debugLog(line);
        }
        if (held != loggedButtons && loggedPulses < 40) {
            loggedButtons = held;
            ++loggedPulses;
            char line[64];
            std::snprintf(line, sizeof(line), "input 0x%llX",
                          static_cast<unsigned long long>(held));
            debugLog(line);
        }

        int direction = -1;
        if (held & HidNpadButton_Up) {
            direction = 0;
        } else if (held & HidNpadButton_Down) {
            direction = 1;
        } else if (held & HidNpadButton_Left) {
            direction = 2;
        } else if (held & HidNpadButton_Right) {
            direction = 3;
        } else if (stick.y > kStickDeadzone * 32767.0f) {
            direction = 0;
        } else if (stick.y < -kStickDeadzone * 32767.0f) {
            direction = 1;
        } else if (stick.x < -kStickDeadzone * 32767.0f) {
            direction = 2;
        } else if (stick.x > kStickDeadzone * 32767.0f) {
            direction = 3;
        }

        bool step[4] = {false, false, false, false};
        if (direction != lastDirection) {
            repeatTimer = kMoveDelay;
            if (direction >= 0) {
                step[direction] = true;
            }
            lastDirection = direction;
        } else if (direction >= 0) {
            repeatTimer -= input.dt;
            if (repeatTimer <= 0.0f) {
                step[direction] = true;
                repeatTimer = kMoveRepeat;
            }
        }
        if (direction < 0) {
            repeatTimer = kMoveDelay;
        }

        input.up = step[0];
        input.down = step[1];
        input.left = step[2];
        input.right = step[3];
        input.accept = (pressed & HidNpadButton_A) != 0;
        input.back = (pressed & HidNpadButton_B) != 0;
        input.select = (pressed & HidNpadButton_X) != 0;
        input.actions = (pressed & HidNpadButton_Y) != 0;
        input.toggleView = (pressed & HidNpadButton_L) != 0;
        input.cycleSort = (pressed & HidNpadButton_R) != 0;
        input.search = (pressed & HidNpadButton_Minus) != 0;
        input.prevSection = (pressed & HidNpadButton_ZL) != 0;
        input.nextSection = (pressed & HidNpadButton_ZR) != 0;
        input.quit = (pressed & HidNpadButton_Plus) != 0;

        HidTouchScreenState touches {};
        const bool touchDown = hidGetTouchScreenStates(&touches, 1) > 0 && touches.count > 0;
        input.touchPressed = touchDown && !touchWasDown;
        touchWasDown = touchDown;
        if (touchDown) {
            input.touchX = static_cast<float>(touches.touches[0].x);
            input.touchY = static_cast<float>(touches.touches[0].y);
        }

        app.update(input);

        u32 stride = 0;
        auto* pixels = static_cast<std::uint32_t*>(framebufferBegin(&framebuffer, &stride));
        nxf::Canvas canvas(pixels, nxf::layout::kScreenWidth, nxf::layout::kScreenHeight,
                           stride / sizeof(std::uint32_t));
        app.render(canvas);
        framebufferEnd(&framebuffer);
        input.time += input.dt;

        if (app.settingsDirty()) {
            nxf::savePrefs(kPrefsPath, app.settings());
            app.clearSettingsDirty();
        }
        if (app.quitRequested()) {
            break;
        }
    }

    ftp->stop();
    framebufferClose(&framebuffer);
    nifmExit();
    socketExit();
    fsdevUnmountDevice("nand");
    return 0;
}
