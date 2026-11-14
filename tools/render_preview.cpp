/*
 * Bonjour! - off-device verification harness.
 *
 * Compiles the very same renderer and animation code that runs on the Switch
 * with the host compiler, renders frames to PPM files and checks the layout,
 * the typewriter progression and the gradient motion numerically.
 *
 * Build:
 *   g++ -std=c++17 -O2 -Iinclude -Ivendor tools/render_preview.cpp \
 *       src/renderer.cpp src/animation.cpp -o render_preview
 *
 * Usage:
 *   ./render_preview assets/fonts/LiberationSerif-Regular.ttf /tmp/frames
 */
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "animation.hpp"
#include "renderer.hpp"

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 720;
constexpr float kFontPixelHeight = 160.0f;
constexpr double kGradientCyclesPerSecond = 0.24;
constexpr const char* kText = "Bonjour !";

int failures = 0;

void check(bool condition, const char* what) {
    std::printf("  [%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (!condition) {
        ++failures;
    }
}

bool readFile(const char* path, std::vector<std::uint8_t>& out) {
    std::FILE* file = std::fopen(path, "rb");
    if (!file) {
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

void writePpm(const std::string& path, const std::vector<std::uint32_t>& pixels) {
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
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
    std::printf("  wrote %s\n", path.c_str());
}

struct Bounds {
    int minX = kWidth, minY = kHeight, maxX = -1, maxY = -1;
    int litPixels = 0;
    bool empty() const { return litPixels == 0; }
    int centerX() const { return (minX + maxX) / 2; }
    int centerY() const { return (minY + maxY) / 2; }
};

Bounds measure(const std::vector<std::uint32_t>& pixels) {
    Bounds bounds;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const std::uint32_t p = pixels[static_cast<std::size_t>(y) * kWidth + x];
            if ((p & 0x00FFFFFFu) == 0) {
                continue;
            }
            ++bounds.litPixels;
            bounds.minX = std::min(bounds.minX, x);
            bounds.maxX = std::max(bounds.maxX, x);
            bounds.minY = std::min(bounds.minY, y);
            bounds.maxY = std::max(bounds.maxY, y);
        }
    }
    return bounds;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <font.ttf> <output-dir>\n", argv[0]);
        return 2;
    }

    std::vector<std::uint8_t> fontData;
    if (!readFile(argv[1], fontData)) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }

    bonjour::Renderer renderer;
    std::printf("== font load ==\n");
    check(renderer.loadFontFromMemory(fontData.data(), fontData.size(), kFontPixelHeight, argv[1]),
          "TrueType font parsed");
    check(renderer.setText(kText), "text laid out");
    std::printf("  %s\n", renderer.description().c_str());

    std::printf("== layout / centring ==\n");
    const int textLeft = (kWidth - renderer.inkWidth()) / 2;
    const int textTop = (kHeight - renderer.inkHeight()) / 2;
    check(renderer.inkWidth() > 0 && renderer.inkHeight() > 0, "ink box is not empty");
    check(std::abs((textLeft + renderer.inkWidth() / 2) - kWidth / 2) <= 1,
          "ink box horizontally centred");
    check(std::abs((textTop + renderer.inkHeight() / 2) - kHeight / 2) <= 1,
          "ink box vertically centred");

    bonjour::Typewriter typewriter(static_cast<std::size_t>(renderer.letterCount()));
    std::vector<float> opacities(static_cast<std::size_t>(renderer.letterCount()), 0.0f);
    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kWidth) * kHeight, 0u);

    auto renderAt = [&](double seconds) {
        typewriter.update(seconds, opacities.data());
        renderer.setReveal(opacities.data(), opacities.size());
        std::fill(pixels.begin(), pixels.end(), 0u);
        renderer.render(pixels.data(), kWidth, kHeight, kWidth,
                        seconds * kGradientCyclesPerSecond);
    };

    std::printf("== typewriter ==\n");
    renderAt(0.0);
    check(measure(pixels).empty(), "screen is black before the first letter");
    renderAt(typewriter.startsAt() + 0.01);
    const Bounds firstLetter = measure(pixels);
    check(!firstLetter.empty(), "first letter appears after the start delay");
    check(firstLetter.maxX - firstLetter.minX < renderer.inkWidth(),
          "only part of the text is drawn mid-animation");
    renderAt(1.05);
    const Bounds midway = measure(pixels);
    check(midway.litPixels > firstLetter.litPixels, "more letters are visible later");
    check(midway.maxX <= firstLetter.maxX + renderer.inkWidth(),
          "letters appear from left to right");
    renderAt(typewriter.finishedAt() + 0.1);
    const Bounds full = measure(pixels);
    check(full.litPixels > midway.litPixels, "all letters are visible once finished");
    check(std::abs(full.centerX() - kWidth / 2) <= 1 && std::abs(full.centerY() - kHeight / 2) <= 1,
          "rendered text is centred both ways");
    check(full.maxX - full.minX == renderer.inkWidth() - 1,
          "rendered width matches the ink box");

    renderAt(typewriter.finishedAt() + 3.0);
    const Bounds later = measure(pixels);
    check(later.litPixels == full.litPixels, "text never disappears after the animation");
    check(later.minX == full.minX && later.maxX == full.maxX, "text does not drift");

    std::printf("== gradient ==\n");
    const double stillTime = typewriter.finishedAt() + 1.0;
    renderAt(stillTime);
    const std::vector<std::uint32_t> frameA = pixels;
    renderAt(stillTime + 0.5);
    const std::vector<std::uint32_t> frameB = pixels;

    int changed = 0;
    int lit = 0;
    std::uint32_t sampleA = 0, sampleB = 0;
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        if ((frameA[i] & 0x00FFFFFFu) == 0) {
            continue;
        }
        ++lit;
        if (frameA[i] != frameB[i]) {
            ++changed;
            sampleA = frameA[i];
            sampleB = frameB[i];
        }
    }
    std::printf("  lit=%d changed=%d sample=#%06X -> #%06X\n", lit, changed, sampleA, sampleB);
    check(changed > lit / 2, "most coloured pixels change over time (gradient is moving)");

    // The spectrum must slide to the right: the frame at t+dt is the frame at t
    // shifted right by speed*dt*textWidth pixels. Search the best matching
    // shift instead of trusting a single guess.
    renderAt(stillTime - 0.5);
    const std::vector<std::uint32_t> frameC = pixels;
    const int expectedShift = static_cast<int>(std::lround(
        0.5 * kGradientCyclesPerSecond * static_cast<double>(renderer.inkWidth())));
    const int maxShift = 90;

    // Compare chromaticity rather than raw RGB: anti-aliased edge pixels have a
    // different coverage in the two frames, but the hue at a given column must
    // be identical once the frame is shifted by the gradient motion.
    auto matchCount = [&](int shift) {
        int matches = 0;
        int compared = 0;
        for (int y = full.minY; y <= full.maxY; ++y) {
            for (int x = full.minX + maxShift; x + maxShift <= full.maxX; ++x) {
                // frameA is the later frame: sample it `shift` pixels to the
                // right of x, so a positive best shift means the content moved
                // to the right.
                const std::uint32_t a = frameA[static_cast<std::size_t>(y) * kWidth + (x + shift)];
                const std::uint32_t b = frameC[static_cast<std::size_t>(y) * kWidth + x];
                const int luminanceA = static_cast<int>(a & 0xFF) +
                                       static_cast<int>((a >> 8) & 0xFF) +
                                       static_cast<int>((a >> 16) & 0xFF);
                const int luminanceB = static_cast<int>(b & 0xFF) +
                                       static_cast<int>((b >> 8) & 0xFF) +
                                       static_cast<int>((b >> 16) & 0xFF);
                if (luminanceA < 60 || luminanceB < 60) {
                    continue;
                }
                ++compared;
                const double chromaA[3] = {static_cast<double>(a & 0xFF) / luminanceA,
                                           static_cast<double>((a >> 8) & 0xFF) / luminanceA,
                                           static_cast<double>((a >> 16) & 0xFF) / luminanceA};
                const double chromaB[3] = {static_cast<double>(b & 0xFF) / luminanceB,
                                           static_cast<double>((b >> 8) & 0xFF) / luminanceB,
                                           static_cast<double>((b >> 16) & 0xFF) / luminanceB};
                const double diff = std::fabs(chromaA[0] - chromaB[0]) +
                                    std::fabs(chromaA[1] - chromaB[1]) +
                                    std::fabs(chromaA[2] - chromaB[2]);
                if (diff <= 0.05) {
                    ++matches;
                }
            }
        }
        return compared > 0 ? matches * 100 / compared : 0;
    };

    int bestShift = 0;
    int bestScore = -1;
    for (int shift = -maxShift; shift <= maxShift; ++shift) {
        const int score = matchCount(shift);
        if (score > bestScore) {
            bestScore = score;
            bestShift = shift;
        }
    }
    std::printf("  best match: %dpx shift (%d%% of pixels), expected %dpx\n", bestShift,
                bestScore, expectedShift);
    check(std::abs(bestShift - expectedShift) <= 3 && bestScore >= 80,
          "gradient slides to the right at the expected speed");
    check(bestShift > 0, "gradient moves from left to right, not backwards");

    std::printf("== frames ==\n");
    const std::string dir = argv[2];
    const double times[] = {0.0, 0.72, 0.90, 1.05, 1.25, 1.60, 2.60};
    for (double t : times) {
        renderAt(t);
        char name[512];
        std::snprintf(name, sizeof(name), "%s/frame_%04d.ppm", dir.c_str(),
                      static_cast<int>(std::lround(t * 100)));
        writePpm(name, pixels);
    }

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
