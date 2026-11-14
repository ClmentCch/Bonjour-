/*
 * NXF Pro - TrueType text engine (Montserrat).
 *
 * Glyphs are rasterized on demand with stb_truetype and cached per size, so the
 * Switch never pays for a glyph twice.
 *
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "nxf/canvas.hpp"
#include "nxf/theme.hpp"

namespace nxf {

enum class Weight { Light = 0, Regular = 1, Medium = 2 };

struct Glyph {
    int width = 0;
    int height = 0;
    int offsetX = 0;
    int offsetY = 0;
    float advance = 0.0f;
    std::size_t stride = 0;
    std::size_t offset = 0; // into the cache's alpha pool
};

struct TextStyle {
    Weight weight = Weight::Regular;
    float size = 16.0f;
    Color color = theme::kText;
    float opacity = 1.0f;
    float letterSpacing = 0.0f;
};

class FontSet {
public:
    FontSet();
    ~FontSet();
    FontSet(const FontSet&) = delete;
    FontSet& operator=(const FontSet&) = delete;

    bool load(Weight weight, const std::uint8_t* data, std::size_t size);
    bool ready(Weight weight) const;

    float ascent(Weight weight, float size) const;
    float lineHeight(Weight weight, float size) const;
    float measure(const TextStyle& style, const std::string& text) const;

    /// Draws text with `y` as the top of the line box. Returns the advance width.
    float draw(Canvas& canvas, const TextStyle& style, float x, float y, const std::string& text) const;

    /// Draws text with a per-column colour ramp (used by the NXF Pro title).
    float drawGradient(Canvas& canvas, const TextStyle& style, float x, float y,
                       const std::string& text, const std::vector<Color>& columns,
                       int columnsStartX) const;

    /// Truncates with an ellipsis so it fits in `maxWidth`.
    std::string ellipsize(const TextStyle& style, const std::string& text, float maxWidth) const;

private:
    struct Face;
    const Glyph* glyph(Weight weight, float size, std::uint32_t codePoint) const;

    std::vector<Face*> faces_;
    mutable std::map<std::uint64_t, Glyph> cache_;
    mutable std::map<std::uint32_t, std::vector<std::uint8_t>> pools_;
};

/// Decodes UTF-8 into code points.
std::vector<std::uint32_t> decodeUtf8(const std::string& text);

} // namespace nxf
