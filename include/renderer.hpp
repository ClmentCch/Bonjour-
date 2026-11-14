/*
 * Bonjour! - text renderer.
 *
 * Renders a static string with a horizontally animated rainbow gradient into a
 * linear 32-bit RGBX framebuffer. Deliberately free of any libnx dependency so
 * the exact same code can be compiled on a PC for off-device verification.
 *
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bonjour {

/// Straight 8-bit colour, no alpha.
struct Rgb {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

/// One glyph baked into the alpha atlas.
/// Coordinates are relative to the text origin: x grows to the right, y grows
/// downwards and y = 0 is the baseline.
struct BakedGlyph {
    int charIndex = 0;          ///< index of the source character
    int x = 0;                  ///< left edge relative to the text origin
    int y = 0;                  ///< top edge relative to the baseline
    int width = 0;
    int height = 0;
    std::size_t atlasOffset = 0;
    float opacity = 1.0f;       ///< reveal opacity driven by the typewriter
};

/// Lays out and draws one static line of text.
class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Parses a TrueType font held in memory and rasterizes glyphs at
    /// `pixelHeight` (the em size, i.e. the nominal rendering height).
    /// `sourceName` is only used for diagnostics.
    bool loadFontFromMemory(const std::uint8_t* data, std::size_t size,
                            float pixelHeight, const char* sourceName);

    /// Lays out `utf8Text` once: glyph advances, kerning, rasterization and the
    /// ink bounding box used for exact optical centring.
    bool setText(const std::string& utf8Text);

    /// Applies one opacity per character, in [0, 1]. Characters without a
    /// bitmap (such as the space) are simply skipped when drawing.
    void setReveal(const float* letterOpacities, std::size_t count);

    /// Draws the text into `pixels`, a `width` x `height` RGBX8888 buffer with
    /// `rowPixels` pixels per row. `flow` is the gradient phase in cycles: it
    /// increases over time and slides the spectrum to the right.
    void render(std::uint32_t* pixels, int width, int height,
                std::ptrdiff_t rowPixels, double flow) const;

    /// Visible ink size, in pixels.
    int inkWidth() const { return inkMaxX_ - inkMinX_; }
    int inkHeight() const { return inkMaxY_ - inkMinY_; }

    /// Number of characters of the laid out text (spaces included).
    int letterCount() const { return letterCount_; }

    bool ready() const { return ready_; }
    float pixelHeight() const;
    std::string description() const;

private:
    struct Font;

    std::unique_ptr<Font> font_;
    std::vector<BakedGlyph> glyphs_;
    std::vector<std::uint8_t> atlas_;
    mutable std::vector<Rgb> gradient_;

    int inkMinX_ = 0;
    int inkMinY_ = 0;
    int inkMaxX_ = 0;
    int inkMaxY_ = 0;
    int letterCount_ = 0;
    bool ready_ = false;
};

} // namespace bonjour
