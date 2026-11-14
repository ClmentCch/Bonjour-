/*
 * NXF Pro - Nintendo Switch entry point.
 *
 * Owns the framebuffer, the pad/touch input, the software keyboard and the
 * settings file. Everything else (UI, filesystem, FTP) lives in portable code.
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
#include "nxf/image.hpp"
#include "nxf/platform_switch.hpp"

#include "font_light_bin.h"
#include "font_medium_bin.h"
#include "font_regular_bin.h"
#include "logo_bin.h"

namespace {

constexpr const char* kConfigDir = "sdmc:/switch/NXF Pro";
constexpr const char* kConfigPath = "sdmc:/switch/NXF Pro/config.ini";
constexpr float kMoveDelay = 0.36f;
constexpr float kMoveRepeat = 0.075f;
constexpr float kStickDeadzone = 0.55f;

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
        nxf::saveSettings(kConfigPath, settings);
    }
};

} // namespace

namespace nxf {

bool loadSettings(const std::string& path, SettingsState& out) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }

    char line[512];
    while (std::fgets(line, sizeof(line), file) != nullptr) {
        const std::string text = trimmed(line);
        if (text.empty() || text[0] == '#') {
            continue;
        }
        const std::size_t separator = text.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = trimmed(text.substr(0, separator));
        const std::string value = trimmed(text.substr(separator + 1));

        if (key == "port") {
            const int port = std::atoi(value.c_str());
            if (port > 0 && port < 65536) {
                out.ftpPort = port;
            }
        } else if (key == "user") {
            if (!value.empty()) {
                out.ftpUser = value;
            }
        } else if (key == "password") {
            out.ftpPassword = value;
        } else if (key == "animations") {
            out.animations = value != "0";
        } else if (key == "grid") {
            out.gridDefault = value != "0";
        }
    }
    std::fclose(file);
    return true;
}

bool saveSettings(const std::string& path, const SettingsState& settings) {
    mkdir("sdmc:/switch", 0777);
    mkdir(kConfigDir, 0777);

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        return false;
    }
    std::fprintf(file,
                 "# NXF Pro\n"
                 "port=%d\n"
                 "user=%s\n"
                 "password=%s\n"
                 "animations=%d\n"
                 "grid=%d\n",
                 settings.ftpPort, settings.ftpUser.c_str(), settings.ftpPassword.c_str(),
                 settings.animations ? 1 : 0, settings.gridDefault ? 1 : 0);
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
    hidInitializeTouchScreen();

    nxf::FontSet fonts;
    fonts.load(nxf::Weight::Light, font_light_bin, font_light_bin_size);
    fonts.load(nxf::Weight::Regular, font_regular_bin, font_regular_bin_size);
    fonts.load(nxf::Weight::Medium, font_medium_bin, font_medium_bin_size);

    nxf::Image logo;
    nxf::loadImageFromMemory(logo_bin, logo_bin_size, logo);

    nxf::SettingsState settings;
    nxf::loadSettings(kConfigPath, settings);

    std::unique_ptr<nxf::StorageApi> sd = nxf::openSdStorage();
    std::unique_ptr<nxf::StorageApi> internal = nxf::openInternalStorage();
    std::unique_ptr<nxf::FtpController> ftp = nxf::createFtpServer(*sd, *internal);

    SwitchPlatform platform;
    nxf::App app(fonts, platform, *ftp, *sd, *internal);
    app.applySettings(settings);
    app.setDefaultView(settings.gridDefault ? nxf::ViewMode::Grid : nxf::ViewMode::List);
    if (logo.valid()) {
        app.setLogo(&logo);
    }
    app.setInitialPath(sd->root());

    Framebuffer framebuffer;
    if (R_FAILED(framebufferCreate(&framebuffer, nwindowGetDefault(), nxf::layout::kScreenWidth,
                                   nxf::layout::kScreenHeight, PIXEL_FORMAT_RGBA_8888, 2))) {
        ftp->stop();
        nifmExit();
        socketExit();
        return 1;
    }
    framebufferMakeLinear(&framebuffer);

    PadState pad;
    padInitializeDefault(&pad);

    nxf::InputState input;
    input.dt = 1.0f / 60.0f;

    int lastDirection = -1;
    float repeatTimer = 0.0f;

    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 pressed = padGetButtonsDown(&pad);
        const u64 held = padGetButtons(&pad);
        const HidAnalogStickState stick = padGetStickPos(&pad, 0);

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
        input.touchPressed = false;
        if (hidGetTouchScreenStates(&touches, 1) > 0 && touches.count > 0) {
            input.touchPressed = true;
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
            nxf::saveSettings(kConfigPath, app.settings());
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
