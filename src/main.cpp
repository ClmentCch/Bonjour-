/*
 * Bonjour! - native Nintendo Switch homebrew.
 *
 * A black screen, the word "Bonjour !" typed out letter by letter in Times New
 * Roman with a continuously flowing rainbow gradient.
 *
 * MIT licensed, see LICENSE.
 */
#include <switch.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "animation.hpp"
#include "renderer.hpp"

// Generated at build time from the selected TrueType file by bin2s.
extern "C" {
extern const unsigned char font_bin[];
extern const unsigned char font_bin_end[];
}

namespace {

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;
constexpr float kFontPixelHeight = 160.0f;
constexpr double kGradientCyclesPerSecond = 0.24; // one full rainbow every ~4 s
constexpr const char* kText = "Bonjour !";
constexpr const char* kUserFontPath = "sdmc:/switch/Bonjour/times.ttf";

#if BONJOUR_FONT_IS_TIMES
constexpr const char* kEmbeddedFontName = "Times New Roman (embedded)";
#else
constexpr const char* kEmbeddedFontName = "Liberation Serif (embedded fallback)";
#endif

/// Monotonic seconds since boot.
double nowSeconds() {
    return static_cast<double>(svcGetSystemTick()) /
           static_cast<double>(armGetSystemTickFreq());
}

bool readFile(const char* path, std::vector<std::uint8_t>& out) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) {
        return false;
    }

    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (size <= 0) {
        std::fclose(file);
        return false;
    }

    out.resize(static_cast<std::size_t>(size));
    const std::size_t read = std::fread(out.data(), 1, out.size(), file);
    std::fclose(file);
    out.resize(read);
    return read > 0;
}

/// Last-resort diagnostics screen: the application already failed, so show the
/// reason instead of exiting silently and let the user quit with +.
void showFatalError(const char* message) {
    consoleInit(nullptr);
    std::printf("\nBonjour! could not start.\n\n%s\n\nPress + to exit.\n", message);
    consoleUpdate(nullptr);

    PadState pad;
    padInitializeDefault(&pad);

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            break;
        }
        consoleUpdate(nullptr);
    }

    consoleExit(nullptr);
}

} // namespace

int main(int, char**) {
    // All shutdown work happens after the main loop, so exit is locked until
    // the framebuffer is released.
    appletLockExit();

    PadState pad;
    padInitializeDefault(&pad);

    Framebuffer framebuffer{};
    const Result rc = framebufferCreate(&framebuffer, nwindowGetDefault(), kScreenWidth,
                                        kScreenHeight, PIXEL_FORMAT_RGBX_8888, 2);
    if (R_FAILED(rc)) {
        char message[96];
        std::snprintf(message, sizeof(message),
                      "framebufferCreate failed (0x%08X).", static_cast<unsigned>(rc));
        appletUnlockExit();
        showFatalError(message);
        return EXIT_FAILURE;
    }
    framebufferMakeLinear(&framebuffer);

    // A Times New Roman placed by the user on the SD card takes precedence over
    // whatever was baked into the binary at build time.
    std::vector<std::uint8_t> fontBytes;
    const char* fontSource = nullptr;
    if (readFile(kUserFontPath, fontBytes)) {
        fontSource = kUserFontPath;
    } else {
        fontBytes.assign(font_bin, font_bin_end);
        fontSource = kEmbeddedFontName;
    }

    bonjour::Renderer renderer;
    if (!renderer.loadFontFromMemory(fontBytes.data(), fontBytes.size(), kFontPixelHeight,
                                     fontSource)) {
        framebufferClose(&framebuffer);
        appletUnlockExit();
        showFatalError("Could not load the font.");
        return EXIT_FAILURE;
    }

    if (!renderer.setText(kText)) {
        framebufferClose(&framebuffer);
        appletUnlockExit();
        showFatalError("Could not lay out the text.");
        return EXIT_FAILURE;
    }

    bonjour::Typewriter typewriter(static_cast<std::size_t>(renderer.letterCount()));
    std::vector<float> opacities(static_cast<std::size_t>(renderer.letterCount()), 0.0f);

    const double startTime = nowSeconds();

    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) {
            break;
        }

        const double elapsed = nowSeconds() - startTime;

        typewriter.update(elapsed, opacities.data());
        renderer.setReveal(opacities.data(), opacities.size());

        u32 stride = 0;
        void* base = framebufferBegin(&framebuffer, &stride);
        if (base == nullptr) {
            break;
        }

        // Pure black every frame; the gradient is drawn on top of it.
        std::memset(base, 0, static_cast<std::size_t>(stride) * kScreenHeight);

        renderer.render(static_cast<std::uint32_t*>(base), kScreenWidth, kScreenHeight,
                        static_cast<std::ptrdiff_t>(stride / sizeof(std::uint32_t)),
                        elapsed * kGradientCyclesPerSecond);

        framebufferEnd(&framebuffer);
    }

    framebufferClose(&framebuffer);
    appletUnlockExit();
    return EXIT_SUCCESS;
}
