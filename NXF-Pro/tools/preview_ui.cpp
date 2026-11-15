/*
 * NXF Pro - off-device interface preview.
 *
 * Renders the real App with fake storage/FTP backends and writes one PPM per
 * screen, so the interface can be verified without a Nintendo Switch.
 *
 *   g++ -std=c++17 -O2 -Iinclude -Ivendor \
 *       src/canvas.cpp src/font.cpp src/icons.cpp src/model.cpp src/render.cpp \
 *       src/app.cpp tools/preview_ui.cpp -o nxfpreview
 *   ./nxfpreview assets out/
 */
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "nxf/app.hpp"
#include "nxf/image.hpp"

namespace {

constexpr int kWidth = nxf::layout::kScreenWidth;
constexpr int kHeight = nxf::layout::kScreenHeight;

bool readFile(const char* path, std::vector<std::uint8_t>& out) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    out.resize(static_cast<std::size_t>(size));
    const std::size_t read = std::fread(out.data(), 1, out.size(), file);
    std::fclose(file);
    out.resize(read);
    return read > 0;
}

bool loadImage(const char* path, nxf::Image& image) {
    std::vector<std::uint8_t> bytes;
    if (!readFile(path, bytes)) {
        return false;
    }
    return nxf::loadImageFromMemory(bytes.data(), bytes.size(), image);
}

void writePpm(const std::string& path, const std::vector<std::uint32_t>& pixels) {
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        std::printf("cannot write %s\n", path.c_str());
        return;
    }
    std::fprintf(file, "P6\n%d %d\n255\n", kWidth, kHeight);
    std::vector<std::uint8_t> row(static_cast<std::size_t>(kWidth) * 3);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::uint32_t p = pixels[static_cast<std::size_t>(y) * kWidth + x];
            row[x * 3 + 0] = static_cast<std::uint8_t>(p & 0xFF);
            row[x * 3 + 1] = static_cast<std::uint8_t>((p >> 8) & 0xFF);
            row[x * 3 + 2] = static_cast<std::uint8_t>((p >> 16) & 0xFF);
        }
        std::fwrite(row.data(), 1, row.size(), file);
    }
    std::fclose(file);
    std::printf("wrote %s\n", path.c_str());
}

struct FakeStorage : nxf::StorageApi {
    bool available_ = true;
    std::string root_ = "sdmc:/";
    std::string name_ = "Switch SD";

    bool available() const override { return available_; }
    std::string displayName() const override { return name_; }
    std::string root() const override { return root_; }

    bool list(const std::string& path, std::vector<nxf::FileEntry>& out,
              std::string& error) override {
        (void)error;
        out.clear();
        const bool root = path == root_;
        const bool switchDir = path == root_ + "switch";

        auto add = [&](const char* name, bool dir, std::uint64_t size, std::int64_t date) {
            out.push_back(nxf::FileEntry{name, dir, size, date});
        };

        const std::int64_t now = 1763190000;
        if (root) {
            add("atmosphere", true, 0, now - 90000);
            add("bootloader", true, 0, now - 400000);
            add("Nintendo", true, 0, now - 800000);
            add("switch", true, 0, now - 1200);
            add("config", true, 0, now - 2400000);
            add("hbmenu.nro", false, 6220011, now - 3000);
            add("payload.bin", false, 294912, now - 6000000);
            add("README.txt", false, 1840, now - 120000);
        } else if (switchDir) {
            add("Bonjour.nro", false, 703069, now - 600);
            add("NXF-Pro.nro", false, 1834022, now - 240);
            add("hbmenu.nro", false, 6220011, now - 90000);
            add("sys-clk", true, 0, now - 50000);
            add("screenshots", true, 0, now - 1000);
            add("notes.txt", false, 640, now - 4000);
        } else {
            add("config.ini", false, 220, now - 50000);
            add("cache", true, 0, now - 7000);
        }
        return true;
    }

    bool makeDirectory(const std::string&, std::string&) override { return true; }
    bool createFile(const std::string&, std::string&) override { return true; }
    bool renamePath(const std::string&, const std::string&, std::string&) override { return true; }
    bool removePath(const std::string&, std::string&) override { return true; }
    bool copyPath(const std::string&, const std::string&, std::string&) override { return true; }

    nxf::FileStats stats(const std::string& path) override {
        nxf::FileStats stats;
        stats.exists = true;
        stats.isDirectory = path.find('.') == std::string::npos;
        stats.size = 703069;
        stats.modified = 1763190000;
        return stats;
    }

    void space(std::uint64_t& total, std::uint64_t& free) override {
        total = 128ull * 1024 * 1024 * 1024;
        free = 47ull * 1024 * 1024 * 1024;
    }
};

struct FakeFtp : nxf::FtpController {
    bool running = false;
    int clients = 0;
    std::vector<std::string> log;

    bool start(int, const std::string&, const std::string&, std::string&) override {
        running = true;
        log.push_back("Serveur démarré sur le port 5000");
        log.push_back("192.168.1.42:5000 · connexion en attente");
        return true;
    }
    void stop() override {
        running = false;
        log.push_back("Serveur arrêté");
    }
    nxf::FtpSnapshot snapshot() const override {
        nxf::FtpSnapshot s;
        s.running = running;
        s.clients = clients;
        s.address = "192.168.1.42:5000";
        s.log = log;
        return s;
    }
};

struct FakeTitles : nxf::TitleApi {
    std::vector<nxf::TitleEntry> entries;

    bool available() const override { return true; }

    bool list(std::vector<nxf::TitleEntry>& out, std::string&) override {
        out = entries;
        return true;
    }

    bool remove(std::uint64_t id, nxf::TitleRemovalReport& report, std::string&) override {
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            if (it->id == id) {
                entries.erase(it);
                break;
            }
        }
        report.contentRemoved = true;
        report.savesRemoved = 1;
        report.leftoversRemoved = 1;
        return true;
    }
};

struct FakePlatform : nxf::Platform {
    bool promptText(const std::string&, const std::string&, std::string&) override { return false; }
    void settingsChanged(const nxf::SettingsState&) override {}
    bool savePreferences(const nxf::SettingsState&) override { return true; }
};

} // namespace

int main(int argc, char** argv) {
    const std::string assets = argc > 1 ? argv[1] : "assets";
    const std::string out = argc > 2 ? argv[2] : "build/preview";

    nxf::FontSet fonts;
    const char* weights[3] = {"Montserrat-Light.ttf", "Montserrat-Regular.ttf",
                              "Montserrat-Medium.ttf"};
    for (int i = 0; i < 3; ++i) {
        std::vector<std::uint8_t> data;
        const std::string path = assets + "/fonts/" + weights[i];
        if (!readFile(path.c_str(), data)) {
            std::printf("missing font %s\n", path.c_str());
            return 2;
        }
        if (!fonts.load(static_cast<nxf::Weight>(i), data.data(), data.size())) {
            std::printf("cannot parse %s\n", path.c_str());
            return 2;
        }
    }

    nxf::Image logo;
    if (!loadImage((assets + "/logonxf.png").c_str(), logo)) {
        std::printf("cannot load logo\n");
    }

    FakeStorage sd;
    FakeStorage internal;
    internal.available_ = false;
    internal.root_ = "nand:/";
    internal.name_ = "Mémoire interne";

    FakeFtp ftp;
    FakePlatform platform;

    std::vector<std::uint8_t> iconBytes;
    readFile((assets + "/logonxf.png").c_str(), iconBytes);

    FakeTitles titles;
    auto addTitle = [&](std::uint64_t id, const char* name, const char* author, const char* version,
                        int updates, int dlc, bool withIcon) {
        nxf::TitleEntry entry;
        entry.id = id;
        entry.name = name;
        entry.author = author;
        entry.version = version;
        entry.updates = updates;
        entry.dlc = dlc;
        if (withIcon) {
            entry.iconJpeg = iconBytes;
        }
        titles.entries.push_back(std::move(entry));
    };
    addTitle(0x0100000000010000ull, "The Legend of Zelda: Tears of the Kingdom", "Nintendo",
             "v1.2.1", 1, 2, true);
    addTitle(0x0100152000022000ull, "Mario Kart 8 Deluxe", "Nintendo", "v3.0.1", 1, 6, true);
    addTitle(0x0100E0F011200000ull, "NXF Pro", "ClmentCch", "v1.1.0", 0, 0, true);
    addTitle(0x01006A800016E000ull, "Super Smash Bros. Ultimate", "Nintendo", "v13.0.1", 1, 12,
             false);
    addTitle(0x0100000000001000ull, "sdmc:/switch/hbmenu.nro", "devkitPro", "", 0, 0, false);
    addTitle(0x01002F0011CA0000ull, "Animal Crossing: New Horizons", "Nintendo", "v2.0.7", 1, 0,
             false);
    addTitle(0x0100B0C011200000ull, "RetroArch", "Libretro", "v1.19.0", 0, 0, false);
    addTitle(0x0100DCA0064A6000ull, "Moonlight", "Moonlight Streaming", "v1.1.0", 0, 0, false);

    // Sparse .nsp files so the DBI screen shows realistic sizes.
    const std::string nspDir = "/tmp/nxf-preview-nsp";
    mkdir(nspDir.c_str(), 0777);
    auto makeNsp = [&](const char* name, off_t size) {
        const std::string path = nspDir + "/" + name;
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << "NSP0";
        stream.close();
        truncate(path.c_str(), size);
    };
    makeNsp("Mario Kart 8 Deluxe [0100152000022000][v0].nsp", 7ll * 1024 * 1024 * 1024);
    makeNsp("New Game [0100ABCDEF123000][v0].nsp", 2ll * 1024 * 1024 * 1024);
    makeNsp("RetroArch [0100B0C011200000][v0].nsp", 380ll * 1024 * 1024);
    makeNsp("The Legend of Zelda - TotK [0100000000010000][v0].nsp",
            14ll * 1024 * 1024 * 1024);

    nxf::App app(fonts, platform, ftp, sd, internal, titles);
    app.setNspRoots({nspDir});
    if (logo.valid()) {
        app.setLogo(&logo);
    }

    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kWidth) * kHeight);
    nxf::Canvas canvas(pixels.data(), kWidth, kHeight, kWidth);

    float clock = 0.0f;
    auto step = [&](int frames, const std::function<void(nxf::InputState&)>& setup = nullptr) {
        for (int i = 0; i < frames; ++i) {
            nxf::InputState input;
            input.time = clock;
            input.dt = 1.0f / 60.0f;
            if (setup) {
                setup(input);
            }
            app.update(input);
            clock += 1.0f / 60.0f;
        }
    };

    auto shot = [&](const std::string& name) {
        std::fill(pixels.begin(), pixels.end(), 0u);
        app.render(canvas);
        writePpm(out + "/" + name + ".ppm", pixels);
    };

    // Edge-triggered events must fire exactly once.
    auto press = [&](void (*setter)(nxf::InputState&)) {
        step(1, setter);
        step(1);
    };

    // First run: language setup.
    app.showLanguageSetup();
    step(40);
    shot("00_setup");
    press([](nxf::InputState& in) { in.accept = true; });
    step(60);

    // Intro + first listing.
    step(60);
    shot("01_sd_list");

    // Grid view.
    press([](nxf::InputState& in) { in.toggleView = true; });
    step(40);
    shot("02_sd_grid");

    // Actions menu on the selected folder.
    press([](nxf::InputState& in) { in.actions = true; });
    step(20);
    shot("03_actions");
    press([](nxf::InputState& in) { in.back = true; });
    step(10);

    // Internal memory (unavailable in applet mode).
    press([](nxf::InputState& in) { in.nextSection = true; });
    step(30);
    shot("04_internal_unavailable");

    // Installed applications.
    press([](nxf::InputState& in) { in.nextSection = true; });
    step(50);
    shot("05_apps");

    press([](nxf::InputState& in) { in.down = true; });
    press([](nxf::InputState& in) { in.down = true; });
    press([](nxf::InputState& in) { in.down = true; });
    step(25);
    shot("06_apps_selected");

    // Complete removal confirmation.
    press([](nxf::InputState& in) { in.accept = true; });
    step(25);
    shot("07_app_delete");
    press([](nxf::InputState& in) { in.back = true; });
    step(10);

    // DBI: cards, NSP list, conflict dialog.
    press([](nxf::InputState& in) { in.nextSection = true; });
    step(40);
    shot("08_dbi_cards");
    press([](nxf::InputState& in) { in.right = true; });
    step(15);
    press([](nxf::InputState& in) { in.accept = true; });
    step(40);
    shot("09_dbi_nsp");
    press([](nxf::InputState& in) { in.accept = true; });
    step(30);
    shot("10_dbi_conflict");
    press([](nxf::InputState& in) { in.back = true; });
    step(15);
    press([](nxf::InputState& in) { in.back = true; });
    step(15);

    // FTP stopped.
    press([](nxf::InputState& in) { in.nextSection = true; });
    step(30);
    shot("11_ftp_stopped");

    // FTP running with a client.
    press([](nxf::InputState& in) { in.accept = true; });
    ftp.running = true;
    ftp.clients = 1;
    ftp.log.push_back("Connexion de 192.168.1.77");
    ftp.log.push_back("PWD /sd/switch");
    ftp.log.push_back("LIST 200 OK");
    step(40);
    shot("12_ftp_running");

    // Settings.
    press([](nxf::InputState& in) { in.nextSection = true; });
    step(30);
    shot("13_settings");

    // About.
    press([](nxf::InputState& in) { in.nextSection = true; });
    step(30);
    shot("14_about");

    // Back to SD in list view.
    for (int i = 0; i < 6; ++i) {
        press([](nxf::InputState& in) { in.prevSection = true; });
    }
    step(20);
    press([](nxf::InputState& in) { in.toggleView = true; });
    step(20);

    // Delete confirmation: navigate the action menu to "Supprimer".
    press([](nxf::InputState& in) { in.actions = true; });
    step(10);
    for (int i = 0; i < 6; ++i) {
        press([](nxf::InputState& in) { in.down = true; });
    }
    shot("15_actions_delete");
    press([](nxf::InputState& in) { in.accept = true; });
    step(20);
    shot("16_confirm_delete");

    // Properties.
    press([](nxf::InputState& in) { in.back = true; });
    step(10);
    press([](nxf::InputState& in) { in.actions = true; });
    step(10);
    for (int i = 0; i < 7; ++i) {
        press([](nxf::InputState& in) { in.down = true; });
    }
    press([](nxf::InputState& in) { in.accept = true; });
    step(20);
    shot("17_properties");
    press([](nxf::InputState& in) { in.back = true; });
    step(20);

    for (int i = 0; i < 4; ++i) {
        press([](nxf::InputState& in) { in.nextSection = true; });
    }
    step(40);
    shot("18_copy");
    press([](nxf::InputState& in) { in.nextSection = true; });
    step(40);
    shot("19_modes");

    std::printf("\nall screens rendered\n");
    return 0;
}
