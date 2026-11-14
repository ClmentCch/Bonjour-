/*
 * NXF Pro - TrueType text engine implementation.
 * MIT licensed, see LICENSE.
 */
#include "nxf/font.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace nxf {

struct FontSet::Face {
    std::vector<std::uint8_t> data;
    stbtt_fontinfo info{};
    bool ok = false;

    float scale(float pixelSize) const {
        return stbtt_ScaleForPixelHeight(&info, pixelSize);
    }
    float ascent(float pixelSize) const {
        int a = 0, d = 0, g = 0;
        stbtt_GetFontVMetrics(&info, &a, &d, &g);
        return static_cast<float>(a) * scale(pixelSize);
    }
    float lineHeight(float pixelSize) const {
        int a = 0, d = 0, g = 0;
        stbtt_GetFontVMetrics(&info, &a, &d, &g);
        return static_cast<float>(a - d + g) * scale(pixelSize);
    }
};

namespace {

int quantize(float size) { return std::max(1, static_cast<int>(std::lround(size))); }

std::uint64_t cacheKey(Weight weight, int px, std::uint32_t codePoint) {
    return (static_cast<std::uint64_t>(weight) << 48) |
           (static_cast<std::uint64_t>(px) << 24) | codePoint;
}

std::uint32_t poolKey(Weight weight, int px) {
    return (static_cast<std::uint32_t>(weight) << 16) | static_cast<std::uint32_t>(px);
}

} // namespace

std::vector<std::uint32_t> decodeUtf8(const std::string& text) {
    std::vector<std::uint32_t> out;
    out.reserve(text.size());
    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        std::uint32_t cp = 0;
        std::size_t extra = 0;
        if (lead < 0x80) {
            cp = lead;
        } else if ((lead & 0xE0) == 0xC0) {
            cp = lead & 0x1Fu;
            extra = 1;
        } else if ((lead & 0xF0) == 0xE0) {
            cp = lead & 0x0Fu;
            extra = 2;
        } else if ((lead & 0xF8) == 0xF0) {
            cp = lead & 0x07u;
            extra = 3;
        } else {
            ++i;
            continue;
        }
        if (i + extra >= text.size()) {
            break;
        }
        bool ok = true;
        for (std::size_t k = 1; k <= extra; ++k) {
            const unsigned char byte = static_cast<unsigned char>(text[i + k]);
            if ((byte & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (byte & 0x3Fu);
        }
        i += extra + 1;
        if (ok) {
            out.push_back(cp);
        }
    }
    return out;
}

FontSet::FontSet() : faces_(3, nullptr) {}

FontSet::~FontSet() {
    for (Face* face : faces_) {
        delete face;
    }
}

bool FontSet::load(Weight weight, const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < 128) {
        return false;
    }
    auto* face = new Face();
    face->data.assign(data, data + size);
    const int offset = stbtt_GetFontOffsetForIndex(face->data.data(), 0);
    if (offset < 0 || !stbtt_InitFont(&face->info, face->data.data(), offset)) {
        delete face;
        return false;
    }
    face->ok = true;

    const std::size_t index = static_cast<std::size_t>(weight);
    delete faces_[index];
    faces_[index] = face;
    return true;
}

bool FontSet::ready(Weight weight) const {
    const std::size_t index = static_cast<std::size_t>(weight);
    return index < faces_.size() && faces_[index] != nullptr && faces_[index]->ok;
}

float FontSet::ascent(Weight weight, float size) const {
    const std::size_t index = static_cast<std::size_t>(weight);
    if (index >= faces_.size() || faces_[index] == nullptr) {
        return size * 0.8f;
    }
    return faces_[index]->ascent(static_cast<float>(quantize(size)));
}

float FontSet::lineHeight(Weight weight, float size) const {
    const std::size_t index = static_cast<std::size_t>(weight);
    if (index >= faces_.size() || faces_[index] == nullptr) {
        return size * 1.2f;
    }
    return faces_[index]->lineHeight(static_cast<float>(quantize(size)));
}

const Glyph* FontSet::glyph(Weight weight, float size, std::uint32_t codePoint) const {
    const std::size_t index = static_cast<std::size_t>(weight);
    if (index >= faces_.size() || faces_[index] == nullptr || !faces_[index]->ok) {
        return nullptr;
    }
    const int px = quantize(size);
    const std::uint64_t key = cacheKey(weight, px, codePoint);

    auto found = cache_.find(key);
    if (found != cache_.end()) {
        return &found->second;
    }

    const Face& face = *faces_[index];
    const float scale = face.scale(static_cast<float>(px));

    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&face.info, codePoint, &advance, &lsb);

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetCodepointBitmapBox(&face.info, codePoint, scale, scale, &x0, &y0, &x1, &y1);

    Glyph glyph;
    glyph.advance = static_cast<float>(advance) * scale;
    glyph.offsetX = x0;
    glyph.offsetY = y0;
    glyph.width = std::max(0, x1 - x0);
    glyph.height = std::max(0, y1 - y0);
    glyph.stride = static_cast<std::size_t>(glyph.width);

    if (glyph.width > 0 && glyph.height > 0) {
        std::vector<std::uint8_t>& pool = pools_[poolKey(weight, px)];
        glyph.offset = pool.size();
        pool.resize(pool.size() + static_cast<std::size_t>(glyph.width) * glyph.height);
        stbtt_MakeCodepointBitmap(&face.info, pool.data() + glyph.offset, glyph.width,
                                  glyph.height, glyph.width, scale, scale, codePoint);
    }

    auto inserted = cache_.emplace(key, glyph);
    return &inserted.first->second;
}

float FontSet::measure(const TextStyle& style, const std::string& text) const {
    const std::size_t index = static_cast<std::size_t>(style.weight);
    if (index >= faces_.size() || faces_[index] == nullptr) {
        return 0.0f;
    }
    const Face& face = *faces_[index];
    const float scale = face.scale(static_cast<float>(quantize(style.size)));

    float width = 0.0f;
    std::uint32_t previous = 0;
    for (std::uint32_t cp : decodeUtf8(text)) {
        if (previous != 0) {
            width += static_cast<float>(stbtt_GetCodepointKernAdvance(&face.info, previous, cp)) *
                     scale;
        }
        const Glyph* g = glyph(style.weight, style.size, cp);
        if (g != nullptr) {
            width += g->advance + style.letterSpacing;
        }
        previous = cp;
    }
    return width;
}

float FontSet::draw(Canvas& canvas, const TextStyle& style, float x, float y,
                    const std::string& text) const {
    const std::size_t index = static_cast<std::size_t>(style.weight);
    if (index >= faces_.size() || faces_[index] == nullptr || style.opacity <= 0.0f) {
        return 0.0f;
    }
    const Face& face = *faces_[index];
    const int px = quantize(style.size);
    const float scale = face.scale(static_cast<float>(px));
    const float baseline = y + face.ascent(static_cast<float>(px));

    float pen = x;
    std::uint32_t previous = 0;

    for (std::uint32_t cp : decodeUtf8(text)) {
        if (previous != 0) {
            pen += static_cast<float>(stbtt_GetCodepointKernAdvance(&face.info, previous, cp)) *
                   scale;
        }
        const Glyph* g = glyph(style.weight, style.size, cp);
        if (g == nullptr) {
            continue;
        }
        if (g->width > 0 && g->height > 0) {
            const std::vector<std::uint8_t>& pool = pools_.at(poolKey(style.weight, px));
            const int drawX = static_cast<int>(std::lround(pen)) + g->offsetX;
            const int drawY = static_cast<int>(std::lround(baseline)) + g->offsetY;
            canvas.blendAlpha(drawX, drawY, g->width, g->height, pool.data() + g->offset,
                              style.color, style.opacity);
        }
        pen += g->advance + style.letterSpacing;
        previous = cp;
    }

    return pen - x;
}

float FontSet::drawGradient(Canvas& canvas, const TextStyle& style, float x, float y,
                            const std::string& text, const std::vector<Color>& columns,
                            int columnsStartX) const {
    const std::size_t index = static_cast<std::size_t>(style.weight);
    if (index >= faces_.size() || faces_[index] == nullptr || style.opacity <= 0.0f ||
        columns.empty()) {
        return 0.0f;
    }
    const Face& face = *faces_[index];
    const int px = quantize(style.size);
    const float scale = face.scale(static_cast<float>(px));
    const float baseline = y + face.ascent(static_cast<float>(px));
    const int columnsEnd = columnsStartX + static_cast<int>(columns.size());

    float pen = x;
    std::uint32_t previous = 0;

    for (std::uint32_t cp : decodeUtf8(text)) {
        if (previous != 0) {
            pen += static_cast<float>(stbtt_GetCodepointKernAdvance(&face.info, previous, cp)) *
                   scale;
        }
        const Glyph* g = glyph(style.weight, style.size, cp);
        if (g == nullptr) {
            continue;
        }
        if (g->width > 0 && g->height > 0) {
            const std::vector<std::uint8_t>& pool = pools_.at(poolKey(style.weight, px));
            int drawX = static_cast<int>(std::lround(pen)) + g->offsetX;
            const int drawY = static_cast<int>(std::lround(baseline)) + g->offsetY;

            // Clamp to the gradient span so glyphs never read out of bounds.
            int sourceStart = std::max(columnsStartX, drawX);
            int sourceEnd = std::min(columnsEnd, drawX + g->width);
            if (sourceEnd > sourceStart) {
                const std::uint8_t* alpha = pool.data() + g->offset +
                                            static_cast<std::size_t>(drawX - (drawX)) +
                                            static_cast<std::size_t>(0);
                // Offset into the glyph bitmap for the first visible column.
                const int skip = sourceStart - drawX;
                alpha += static_cast<std::size_t>(skip);
                canvas.blendAlphaGradient(sourceStart, drawY, sourceEnd - sourceStart, g->height,
                                          alpha, columns.data() + (sourceStart - columnsStartX),
                                          style.opacity);
            }
            (void)drawX;
        }
        pen += g->advance + style.letterSpacing;
        previous = cp;
    }

    return pen - x;
}

std::string FontSet::ellipsize(const TextStyle& style, const std::string& text,
                               float maxWidth) const {
    if (measure(style, text) <= maxWidth) {
        return text;
    }
    const std::string dots = "...";
    const float dotsWidth = measure(style, dots);
    std::string result;
    float width = 0.0f;
    for (std::uint32_t cp : decodeUtf8(text)) {
        // Re-encode the code point (input is expected to be UTF-8 already).
        std::string piece;
        if (cp < 0x80) {
            piece.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            piece.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            piece.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            piece.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            piece.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            piece.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        const float pieceWidth = measure(style, piece);
        if (width + pieceWidth + dotsWidth > maxWidth) {
            break;
        }
        width += pieceWidth;
        result += piece;
    }
    return result + dots;
}

} // namespace nxf
