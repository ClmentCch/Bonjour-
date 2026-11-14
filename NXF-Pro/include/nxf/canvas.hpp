/*
 * NXF Pro - software canvas.
 *
 * Draws into a linear 32-bit RGBX framebuffer. Free of any libnx dependency so
 * the whole interface can be rendered on a PC for off-device verification.
 *
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "nxf/theme.hpp"

namespace nxf {

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    bool contains(float px, float py) const {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

/// Decoded RGBA image.
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels; // 0xAABBGGRR

    bool valid() const { return width > 0 && height > 0 && !pixels.empty(); }
};

class Canvas {
public:
    Canvas(std::uint32_t* pixels, int width, int height, std::ptrdiff_t pitchPixels);

    int width() const { return width_; }
    int height() const { return height_; }

    void clear(Color color);

    // Clipping, intersect-only (save/restore pair).
    void pushClip(const Rect& rect);
    void popClip();

    void fillRect(const Rect& rect, Color color);
    void fillRect(int x, int y, int w, int h, Color color);

    /// Anti-aliased rounded rectangle (signed distance, ~1px feather).
    void fillRoundRect(const Rect& rect, float radius, Color color);

    /// Vertical gradient inside a rounded rectangle.
    void fillRoundRectGradient(const Rect& rect, float radius, Color top, Color bottom);

    /// 1px-ish border drawn inside the rect.
    void strokeRoundRect(const Rect& rect, float radius, Color color, float thickness = 1.0f);

    /// Soft drop shadow: a few nested rounded rects with low alpha.
    void dropShadow(const Rect& rect, float radius, float spread, float alpha);

    void fillCircle(float cx, float cy, float radius, Color color);
    void strokeCircle(float cx, float cy, float radius, Color color, float thickness);

    void drawLine(float x0, float y0, float x1, float y1, Color color, float thickness = 1.0f);

    /// Filled convex polygon (scanline, anti-aliased on the vertical edges).
    void fillPolygon(const float* points, int count, Color color);

    /// Bilinear image draw with a rounded-rect mask, useful for logos.
    void drawImageRounded(const Image& image, const Rect& rect, float radius, float opacity);

    /// Raw horizontal gradient fill (no rounding).
    void fillHorizontalGradient(const Rect& rect, Color left, Color right);

    /// Blends a translucent overlay over the whole canvas.
    void dim(float alpha);

    /// Text drawing is implemented by the FontSet; this blits an alpha bitmap.
    void blendAlpha(int x, int y, int w, int h, const std::uint8_t* alpha, Color color,
                    float opacity);

    /// Gradient text blit: colour sampled per column from `gradient`.
    void blendAlphaGradient(int x, int y, int w, int h, const std::uint8_t* alpha,
                            const Color* columns, float opacity);

private:
    void blendPixel(int x, int y, Color color, float coverage);
    void writePixel(int x, int y, Color color);
    bool clipped(int x, int y) const;

    std::uint32_t* pixels_;
    int width_;
    int height_;
    std::ptrdiff_t pitch_;
    std::vector<Rect> clips_;
};

} // namespace nxf
