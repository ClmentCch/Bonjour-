/*
 * Bonjour! - text renderer implementation.
 *
 * MIT licensed, see LICENSE.
 */
#include "renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace bonjour {

struct Renderer::Font {
    // stb keeps raw pointers into this buffer, so the bytes are owned here and
    // never reallocated after stbtt_InitFont().
    std::vector<std::uint8_t> data;
    stbtt_fontinfo info{};
    std::string name;
    float pixelHeight = 0.0f;
};

namespace {

/// Decodes UTF-8 into code points, skipping malformed lead bytes.
std::vector<std::uint32_t> decodeUtf8(const std::string& text) {
    std::vector<std::uint32_t> codePoints;
    codePoints.reserve(text.size());

    std::size_t i = 0;
    while (i < text.size()) {
        const unsigned char lead = static_cast<unsigned char>(text[i]);
        std::uint32_t codePoint = 0;
        std::size_t continuation = 0;

        if (lead < 0x80) {
            codePoint = lead;
        } else if ((lead & 0xE0) == 0xC0) {
            codePoint = lead & 0x1Fu;
            continuation = 1;
        } else if ((lead & 0xF0) == 0xE0) {
            codePoint = lead & 0x0Fu;
            continuation = 2;
        } else if ((lead & 0xF8) == 0xF0) {
            codePoint = lead & 0x07u;
            continuation = 3;
        } else {
            ++i; // stray continuation byte
            continue;
        }

        if (i + continuation >= text.size()) {
            break; // truncated sequence at the end
        }

        bool valid = true;
        for (std::size_t k = 1; k <= continuation; ++k) {
            const unsigned char byte = static_cast<unsigned char>(text[i + k]);
            if ((byte & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codePoint = (codePoint << 6) | (byte & 0x3Fu);
        }

        i += continuation + 1;
        if (valid) {
            codePoints.push_back(codePoint);
        }
    }

    return codePoints;
}

/// Fully saturated HSV hue to RGB: the eight hues of the gradient
/// (red, orange, yellow, green, cyan, blue, violet, pink) are all on this wheel,
/// in that order, so a single continuous hue sweep covers them exactly.
Rgb hueToRgb(double hue) {
    hue -= std::floor(hue);

    const double scaled = hue * 6.0;
    const int sector = static_cast<int>(scaled) % 6;
    const double f = scaled - std::floor(scaled);
    const double falling = 1.0 - f;

    double r = 0.0, g = 0.0, b = 0.0;
    switch (sector) {
        case 0: r = 1.0; g = f;       b = 0.0;     break; // red -> orange
        case 1: r = falling; g = 1.0; b = 0.0;     break; // orange -> yellow
        case 2: r = 0.0; g = 1.0;     b = f;       break; // yellow -> green
        case 3: r = 0.0; g = falling; b = 1.0;     break; // green -> cyan
        case 4: r = f;   g = 0.0;     b = 1.0;     break; // cyan -> blue
        default: r = 1.0; g = 0.0;    b = falling; break; // blue -> violet -> pink
    }

    const auto toByte = [](double value) {
        return static_cast<std::uint8_t>(
            std::lround(std::clamp(value, 0.0, 1.0) * 255.0));
    };

    return Rgb{toByte(r), toByte(g), toByte(b)};
}

} // namespace

Renderer::Renderer() = default;

Renderer::~Renderer() = default;

float Renderer::pixelHeight() const {
    return font_ ? font_->pixelHeight : 0.0f;
}

bool Renderer::loadFontFromMemory(const std::uint8_t* data, std::size_t size,
                                  float pixelHeight, const char* sourceName) {
    ready_ = false;
    font_.reset();

    if (data == nullptr || size < 64 || pixelHeight < 1.0f) {
        return false;
    }

    auto font = std::make_unique<Font>();
    font->pixelHeight = pixelHeight;
    font->name = sourceName != nullptr ? sourceName : "unknown";
    font->data.assign(data, data + size);

    // Times New Roman ships as a plain TrueType file with a single face, but
    // accepting a TrueType collection index keeps the loader robust.
    const int offset = stbtt_GetFontOffsetForIndex(font->data.data(), 0);
    if (offset < 0) {
        return false;
    }

    if (!stbtt_InitFont(&font->info, font->data.data(), offset)) {
        return false;
    }

    font_ = std::move(font);
    ready_ = true;
    return true;
}

bool Renderer::setText(const std::string& utf8Text) {
    glyphs_.clear();
    atlas_.clear();
    letterCount_ = 0;
    inkMinX_ = inkMinY_ = inkMaxX_ = inkMaxY_ = 0;

    if (!ready_ || !font_) {
        return false;
    }

    const std::vector<std::uint32_t> codePoints = decodeUtf8(utf8Text);
    if (codePoints.empty()) {
        return false;
    }

    letterCount_ = static_cast<int>(codePoints.size());
    const float scale = stbtt_ScaleForPixelHeight(&font_->info, font_->pixelHeight);

    float penX = 0.0f;
    std::uint32_t previous = 0;
    bool haveInk = false;

    for (std::size_t index = 0; index < codePoints.size(); ++index) {
        const std::uint32_t codePoint = codePoints[index];

        const int kerning = previous != 0
            ? stbtt_GetCodepointKernAdvance(&font_->info, previous, codePoint)
            : 0;
        penX += static_cast<float>(kerning) * scale;

        int advance = 0;
        int leftSideBearing = 0;
        stbtt_GetCodepointHMetrics(&font_->info, codePoint, &advance, &leftSideBearing);
        (void)leftSideBearing; // only the advance is needed for layout

        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(&font_->info, codePoint, scale, scale, &x0, &y0, &x1, &y1);

        const int glyphWidth = x1 - x0;
        const int glyphHeight = y1 - y0;

        if (glyphWidth > 0 && glyphHeight > 0) {
            BakedGlyph glyph;
            glyph.charIndex = static_cast<int>(index);
            glyph.x = static_cast<int>(std::lround(penX)) + x0;
            glyph.y = y0;
            glyph.width = glyphWidth;
            glyph.height = glyphHeight;
            glyph.atlasOffset = atlas_.size();

            atlas_.resize(atlas_.size() +
                          static_cast<std::size_t>(glyphWidth) * static_cast<std::size_t>(glyphHeight));
            stbtt_MakeCodepointBitmap(&font_->info, atlas_.data() + glyph.atlasOffset,
                                      glyphWidth, glyphHeight, glyphWidth, scale, scale, codePoint);

            if (!haveInk) {
                inkMinX_ = glyph.x;
                inkMaxX_ = glyph.x + glyph.width;
                inkMinY_ = glyph.y;
                inkMaxY_ = glyph.y + glyph.height;
                haveInk = true;
            } else {
                inkMinX_ = std::min(inkMinX_, glyph.x);
                inkMaxX_ = std::max(inkMaxX_, glyph.x + glyph.width);
                inkMinY_ = std::min(inkMinY_, glyph.y);
                inkMaxY_ = std::max(inkMaxY_, glyph.y + glyph.height);
            }

            glyphs_.push_back(glyph);
        }

        penX += static_cast<float>(advance) * scale;
        previous = codePoint;
    }

    return haveInk;
}

void Renderer::setReveal(const float* letterOpacities, std::size_t count) {
    for (BakedGlyph& glyph : glyphs_) {
        float opacity = 1.0f;
        if (letterOpacities != nullptr && static_cast<std::size_t>(glyph.charIndex) < count) {
            opacity = letterOpacities[glyph.charIndex];
        }
        glyph.opacity = std::clamp(opacity, 0.0f, 1.0f);
    }
}

void Renderer::render(std::uint32_t* pixels, int width, int height,
                      std::ptrdiff_t rowPixels, double flow) const {
    const int textWidth = inkWidth();
    const int textHeight = inkHeight();
    if (pixels == nullptr || width <= 0 || height <= 0 || textWidth <= 0 || textHeight <= 0) {
        return;
    }

    // Optical centring: the ink box (not the advance box) is centred, so the
    // glyphs are visually centred both horizontally and vertically.
    const int textLeft = (width - textWidth) / 2;
    const int textTop = (height - textHeight) / 2;
    const int originX = textLeft - inkMinX_;
    const int originY = textTop - inkMinY_;

    // One full rainbow cycle across the text width, sliding right over time.
    gradient_.resize(static_cast<std::size_t>(textWidth));
    for (int i = 0; i < textWidth; ++i) {
        const double u = static_cast<double>(i) / static_cast<double>(textWidth);
        gradient_[static_cast<std::size_t>(i)] = hueToRgb(u - flow);
    }

    for (const BakedGlyph& glyph : glyphs_) {
        const int opacity = static_cast<int>(std::lround(glyph.opacity * 255.0f));
        if (opacity <= 0) {
            continue;
        }

        const int glyphX = originX + glyph.x;
        const int glyphY = originY + glyph.y;

        for (int row = 0; row < glyph.height; ++row) {
            const int y = glyphY + row;
            if (y < 0 || y >= height) {
                continue;
            }

            const std::uint8_t* source =
                atlas_.data() + glyph.atlasOffset + static_cast<std::size_t>(row) * glyph.width;
            std::uint32_t* destination =
                pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(rowPixels);

            for (int column = 0; column < glyph.width; ++column) {
                const int x = glyphX + column;
                if (x < 0 || x >= width) {
                    continue;
                }

                int coverage = source[column];
                if (coverage == 0) {
                    continue;
                }
                coverage = coverage * opacity / 255;
                if (coverage == 0) {
                    continue;
                }

                const int gradientIndex = std::clamp(x - textLeft, 0, textWidth - 1);
                const Rgb colour = gradient_[static_cast<std::size_t>(gradientIndex)];

                // The framebuffer is already pure black, so blending the colour
                // with its coverage is enough for a clean anti-aliased edge.
                const std::uint32_t r = static_cast<std::uint32_t>(colour.r) * coverage / 255u;
                const std::uint32_t g = static_cast<std::uint32_t>(colour.g) * coverage / 255u;
                const std::uint32_t b = static_cast<std::uint32_t>(colour.b) * coverage / 255u;

                destination[x] = (b << 16) | (g << 8) | r; // RGBX8888
            }
        }
    }
}

std::string Renderer::description() const {
    if (!font_ || !ready_) {
        return "no font";
    }
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer), "%s, %.0f px, ink %dx%d",
                  font_->name.c_str(), static_cast<double>(font_->pixelHeight),
                  inkWidth(), inkHeight());
    return std::string(buffer);
}

} // namespace bonjour
