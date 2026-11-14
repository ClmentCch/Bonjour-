/*
 * NXF Pro - software canvas implementation.
 * MIT licensed, see LICENSE.
 */
#include "nxf/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace nxf {

namespace {

inline float coverageOf(float a, float b, int pixel) {
    return std::clamp(std::min(b, static_cast<float>(pixel) + 1.0f) -
                          std::max(a, static_cast<float>(pixel)),
                      0.0f, 1.0f);
}

} // namespace

Canvas::Canvas(std::uint32_t* pixels, int width, int height, std::ptrdiff_t pitchPixels)
    : pixels_(pixels), width_(width), height_(height), pitch_(pitchPixels) {
    clips_.push_back(Rect{0, 0, static_cast<float>(width), static_cast<float>(height)});
}

void Canvas::clear(Color color) {
    if (color.r == 0 && color.g == 0 && color.b == 0 && color.a == 255) {
        for (int y = 0; y < height_; ++y) {
            std::memset(pixels_ + static_cast<std::ptrdiff_t>(y) * pitch_, 0,
                        static_cast<std::size_t>(width_) * sizeof(std::uint32_t));
        }
        return;
    }
    const std::uint32_t packed = pack(color);
    for (int y = 0; y < height_; ++y) {
        std::fill_n(pixels_ + static_cast<std::ptrdiff_t>(y) * pitch_, width_, packed);
    }
}

void Canvas::pushClip(const Rect& rect) {
    const Rect& current = clips_.back();
    Rect next;
    next.x = std::max(current.x, rect.x);
    next.y = std::max(current.y, rect.y);
    next.w = std::max(0.0f, std::min(current.right(), rect.right()) - next.x);
    next.h = std::max(0.0f, std::min(current.bottom(), rect.bottom()) - next.y);
    clips_.push_back(next);
}

void Canvas::popClip() {
    if (clips_.size() > 1) {
        clips_.pop_back();
    }
}

bool Canvas::clipped(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return true;
    }
    const Rect& clip = clips_.back();
    return static_cast<float>(x) < clip.x || static_cast<float>(y) < clip.y ||
           static_cast<float>(x) >= clip.right() || static_cast<float>(y) >= clip.bottom();
}

void Canvas::writePixel(int x, int y, Color color) {
    if (clipped(x, y)) {
        return;
    }
    pixels_[static_cast<std::ptrdiff_t>(y) * pitch_ + x] = pack(color) | 0xFF000000u;
}

void Canvas::blendPixel(int x, int y, Color color, float coverage) {
    if (clipped(x, y)) {
        return;
    }
    const float alpha = (static_cast<float>(color.a) / 255.0f) * std::clamp(coverage, 0.0f, 1.0f);
    if (alpha <= 0.0f) {
        return;
    }
    std::uint32_t& dst = pixels_[static_cast<std::ptrdiff_t>(y) * pitch_ + x];
    if (alpha >= 0.999f) {
        dst = pack(color) | 0xFF000000u;
        return;
    }
    const std::uint32_t dr = dst & 0xFF;
    const std::uint32_t dg = (dst >> 8) & 0xFF;
    const std::uint32_t db = (dst >> 16) & 0xFF;
    const std::uint32_t r = static_cast<std::uint32_t>(
        static_cast<float>(dr) + (static_cast<float>(color.r) - dr) * alpha);
    const std::uint32_t g = static_cast<std::uint32_t>(
        static_cast<float>(dg) + (static_cast<float>(color.g) - dg) * alpha);
    const std::uint32_t b = static_cast<std::uint32_t>(
        static_cast<float>(db) + (static_cast<float>(color.b) - db) * alpha);
    dst = r | (g << 8) | (b << 16) | 0xFF000000u;
}

void Canvas::fillRect(int x, int y, int w, int h, Color color) {
    fillRect(Rect{static_cast<float>(x), static_cast<float>(y), static_cast<float>(w),
                  static_cast<float>(h)},
             color);
}

void Canvas::fillRect(const Rect& rect, Color color) {
    if (rect.w <= 0.0f || rect.h <= 0.0f || color.a == 0) {
        return;
    }
    const int y0 = static_cast<int>(std::floor(rect.y));
    const int y1 = static_cast<int>(std::ceil(rect.bottom()));
    const int x0 = static_cast<int>(std::floor(rect.x));
    const int x1 = static_cast<int>(std::ceil(rect.right()));

    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const float cov = coverageOf(rect.x, rect.right(), x) *
                              coverageOf(rect.y, rect.bottom(), y);
            if (cov <= 0.0f) {
                continue;
            }
            if (cov >= 1.0f) {
                writePixel(x, y, color);
            } else {
                blendPixel(x, y, color, cov);
            }
        }
    }
}

void Canvas::fillRoundRect(const Rect& rect, float radius, Color color) {
    if (rect.w <= 0.0f || rect.h <= 0.0f || color.a == 0) {
        return;
    }
    radius = std::clamp(radius, 0.0f, std::min(rect.w, rect.h) * 0.5f);

    const int y0 = static_cast<int>(std::floor(rect.y));
    const int y1 = static_cast<int>(std::ceil(rect.bottom()));
    const std::uint32_t packed = pack(color) | 0xFF000000u;

    for (int y = y0; y < y1; ++y) {
        const float cy = static_cast<float>(y) + 0.5f;
        float dy = 0.0f;
        if (cy < rect.y + radius) {
            dy = (rect.y + radius) - cy;
        } else if (cy > rect.bottom() - radius) {
            dy = cy - (rect.bottom() - radius);
        }
        if (dy > radius) {
            continue;
        }

        const float inset = dy > 0.0f ? radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy))
                                      : 0.0f;
        const float left = rect.x + inset;
        const float right = rect.right() - inset;
        if (right <= left) {
            continue;
        }

        const float rowCoverage = coverageOf(rect.y, rect.bottom(), y);
        if (rowCoverage <= 0.0f) {
            continue;
        }

        // Fully covered interior: one tight fill.
        const int solidStart = static_cast<int>(std::ceil(left));
        const int solidEnd = static_cast<int>(std::floor(right));
        if (rowCoverage >= 0.999f && solidEnd > solidStart) {
            if (y >= 0 && y < height_) {
                const Rect& clip = clips_.back();
                const int clipStart = std::max(solidStart, static_cast<int>(clip.x));
                const int clipEnd = std::min(solidEnd, static_cast<int>(clip.right()));
                if (clipEnd > clipStart && static_cast<float>(y) >= clip.y &&
                    static_cast<float>(y) < clip.bottom()) {
                    std::fill_n(pixels_ + static_cast<std::ptrdiff_t>(y) * pitch_ + clipStart,
                                clipEnd - clipStart, packed);
                }
            }
        }

        // Anti-aliased ends (and everything when the row is sub-pixel thin).
        const int x0 = static_cast<int>(std::floor(left)) - 1;
        const int x1 = static_cast<int>(std::ceil(right)) + 1;
        for (int x = x0; x < x1; ++x) {
            const float cov = coverageOf(left, right, x) * rowCoverage;
            if (cov <= 0.0f) {
                continue;
            }
            if (cov >= 0.999f) {
                continue; // already written by the solid fill
            }
            blendPixel(x, y, color, cov);
        }
    }
}

void Canvas::fillRoundRectGradient(const Rect& rect, float radius, Color top, Color bottom) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }
    radius = std::clamp(radius, 0.0f, std::min(rect.w, rect.h) * 0.5f);
    const int y0 = static_cast<int>(std::floor(rect.y));
    const int y1 = static_cast<int>(std::ceil(rect.bottom()));

    for (int y = y0; y < y1; ++y) {
        const float cy = static_cast<float>(y) + 0.5f;
        float dy = 0.0f;
        if (cy < rect.y + radius) {
            dy = (rect.y + radius) - cy;
        } else if (cy > rect.bottom() - radius) {
            dy = cy - (rect.bottom() - radius);
        }
        if (dy > radius) {
            continue;
        }
        const float inset = dy > 0.0f ? radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy))
                                      : 0.0f;
        const float left = rect.x + inset;
        const float right = rect.right() - inset;
        const float t = rect.h > 0.0f ? std::clamp((cy - rect.y) / rect.h, 0.0f, 1.0f) : 0.0f;
        const Color rowColor = mix(top, bottom, t);

        for (int x = static_cast<int>(std::floor(left)); x < static_cast<int>(std::ceil(right));
             ++x) {
            const float cov = coverageOf(left, right, x) * coverageOf(rect.y, rect.bottom(), y);
            if (cov > 0.0f) {
                blendPixel(x, y, rowColor, cov);
            }
        }
    }
}

void Canvas::strokeRoundRect(const Rect& rect, float radius, Color color, float thickness) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }
    radius = std::clamp(radius, 0.0f, std::min(rect.w, rect.h) * 0.5f);
    thickness = std::max(0.5f, thickness);
    const int y0 = static_cast<int>(std::floor(rect.y - 1.0f));
    const int y1 = static_cast<int>(std::ceil(rect.bottom() + 1.0f));

    for (int y = y0; y < y1; ++y) {
        const float cy = static_cast<float>(y) + 0.5f;
        float dy = 0.0f;
        if (cy < rect.y + radius) {
            dy = (rect.y + radius) - cy;
        } else if (cy > rect.bottom() - radius) {
            dy = cy - (rect.bottom() - radius);
        }
        if (dy > radius) {
            continue;
        }
        const float inset = dy > 0.0f ? radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy))
                                      : 0.0f;
        const float left = rect.x + inset;
        const float right = rect.right() - inset;

        const int x0 = static_cast<int>(std::floor(left - thickness)) - 1;
        const int x1 = static_cast<int>(std::ceil(right + thickness)) + 1;
        for (int x = x0; x < x1; ++x) {
            const float outer = coverageOf(left, right, x);
            const float inner = coverageOf(left + thickness, right - thickness, x);
            float cov = outer - inner;
            // Vertical edges.
            if (x >= static_cast<int>(left) - 1 &&
                x <= static_cast<int>(std::ceil(left)) + 1) {
                cov = std::max(cov, coverageOf(left, left + thickness, x) *
                                        coverageOf(rect.y, rect.bottom(), y));
            }
            if (x >= static_cast<int>(right) - 1 &&
                x <= static_cast<int>(std::ceil(right)) + 1) {
                cov = std::max(cov, coverageOf(right - thickness, right, x) *
                                        coverageOf(rect.y, rect.bottom(), y));
            }
            if (cov > 0.0f) {
                blendPixel(x, y, color, std::clamp(cov, 0.0f, 1.0f));
            }
        }
    }
}

void Canvas::dropShadow(const Rect& rect, float radius, float spread, float alpha) {
    for (int layer = 3; layer >= 1; --layer) {
        const float grow = spread * static_cast<float>(layer) / 3.0f;
        const float a = alpha * (0.45f / static_cast<float>(layer));
        Rect shadow{rect.x - grow, rect.y - grow + 2.0f, rect.w + grow * 2.0f,
                    rect.h + grow * 2.0f};
        fillRoundRect(shadow, radius + grow, withAlpha(hex(0x000000), a));
    }
}

void Canvas::fillCircle(float cx, float cy, float radius, Color color) {
    if (radius <= 0.0f) {
        return;
    }
    const int y0 = static_cast<int>(std::floor(cy - radius));
    const int y1 = static_cast<int>(std::ceil(cy + radius));
    for (int y = y0; y < y1; ++y) {
        const float dy = (static_cast<float>(y) + 0.5f) - cy;
        const float span = radius * radius - dy * dy;
        if (span <= 0.0f) {
            continue;
        }
        const float half = std::sqrt(span);
        const float left = cx - half;
        const float right = cx + half;
        for (int x = static_cast<int>(std::floor(left)); x < static_cast<int>(std::ceil(right));
             ++x) {
            const float cov = coverageOf(left, right, x);
            if (cov > 0.0f) {
                blendPixel(x, y, color, cov);
            }
        }
    }
}

void Canvas::strokeCircle(float cx, float cy, float radius, Color color, float thickness) {
    const float half = std::max(0.5f, thickness * 0.5f);
    const int y0 = static_cast<int>(std::floor(cy - radius - half));
    const int y1 = static_cast<int>(std::ceil(cy + radius + half));
    for (int y = y0; y < y1; ++y) {
        for (int x = static_cast<int>(std::floor(cx - radius - half));
             x < static_cast<int>(std::ceil(cx + radius + half)); ++x) {
            const float dx = (static_cast<float>(x) + 0.5f) - cx;
            const float dy = (static_cast<float>(y) + 0.5f) - cy;
            const float distance = std::sqrt(dx * dx + dy * dy);
            // Distance-based feather: 1 at the stroke centre line, 0 at its edge.
            const float coverage =
                std::clamp(1.0f - std::fabs(distance - radius) / half, 0.0f, 1.0f);
            if (coverage > 0.0f) {
                blendPixel(x, y, color, coverage);
            }
        }
    }
}

void Canvas::drawLine(float x0, float y0, float x1, float y1, Color color, float thickness) {
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const int steps = static_cast<int>(std::max(std::fabs(dx), std::fabs(dy))) + 1;
    const float half = std::max(0.5f, thickness * 0.5f);
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        fillCircle(x0 + dx * t, y0 + dy * t, half, color);
    }
}

void Canvas::fillPolygon(const float* points, int count, Color color) {
    if (count < 3) {
        return;
    }
    float minY = points[1], maxY = points[1];
    for (int i = 1; i < count; ++i) {
        minY = std::min(minY, points[i * 2 + 1]);
        maxY = std::max(maxY, points[i * 2 + 1]);
    }
    const int y0 = static_cast<int>(std::floor(minY));
    const int y1 = static_cast<int>(std::ceil(maxY));

    for (int y = y0; y < y1; ++y) {
        const float scan = static_cast<float>(y) + 0.5f;
        float left = 1e9f;
        float right = -1e9f;
        for (int i = 0; i < count; ++i) {
            const int j = (i + 1) % count;
            const float ax = points[i * 2], ay = points[i * 2 + 1];
            const float bx = points[j * 2], by = points[j * 2 + 1];
            if ((ay <= scan && by > scan) || (by <= scan && ay > scan)) {
                const float t = (scan - ay) / (by - ay);
                const float x = ax + (bx - ax) * t;
                left = std::min(left, x);
                right = std::max(right, x);
            }
        }
        if (right <= left) {
            continue;
        }
        for (int x = static_cast<int>(std::floor(left)); x < static_cast<int>(std::ceil(right));
             ++x) {
            const float cov = coverageOf(left, right, x);
            if (cov > 0.0f) {
                blendPixel(x, y, color, cov);
            }
        }
    }
}

void Canvas::drawImageRounded(const Image& image, const Rect& rect, float radius, float opacity) {
    if (!image.valid() || rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }
    radius = std::clamp(radius, 0.0f, std::min(rect.w, rect.h) * 0.5f);

    for (int y = static_cast<int>(std::floor(rect.y)); y < static_cast<int>(std::ceil(rect.bottom()));
         ++y) {
        const float cy = static_cast<float>(y) + 0.5f;
        float dy = 0.0f;
        if (cy < rect.y + radius) {
            dy = (rect.y + radius) - cy;
        } else if (cy > rect.bottom() - radius) {
            dy = cy - (rect.bottom() - radius);
        }
        if (dy > radius) {
            continue;
        }
        const float inset = dy > 0.0f ? radius - std::sqrt(std::max(0.0f, radius * radius - dy * dy))
                                      : 0.0f;
        const float left = rect.x + inset;
        const float right = rect.right() - inset;

        const float v = (cy - rect.y) / rect.h;
        const int sy = std::clamp(static_cast<int>(v * image.height), 0, image.height - 1);

        for (int x = static_cast<int>(std::floor(left)); x < static_cast<int>(std::ceil(right));
             ++x) {
            const float cov = coverageOf(left, right, x) * coverageOf(rect.y, rect.bottom(), y);
            if (cov <= 0.0f) {
                continue;
            }
            const float u = ((static_cast<float>(x) + 0.5f) - rect.x) / rect.w;
            const int sx = std::clamp(static_cast<int>(u * image.width), 0, image.width - 1);
            const std::uint32_t texel = image.pixels[static_cast<std::size_t>(sy) * image.width + sx];
            const Color color{static_cast<u8>(texel & 0xFF), static_cast<u8>((texel >> 8) & 0xFF),
                              static_cast<u8>((texel >> 16) & 0xFF),
                              static_cast<u8>((texel >> 24) & 0xFF)};
            blendPixel(x, y, color, cov * opacity * (static_cast<float>(color.a) / 255.0f));
        }
    }
}

void Canvas::fillHorizontalGradient(const Rect& rect, Color left, Color right) {
    for (int y = static_cast<int>(std::floor(rect.y)); y < static_cast<int>(std::ceil(rect.bottom()));
         ++y) {
        for (int x = static_cast<int>(std::floor(rect.x)); x < static_cast<int>(std::ceil(rect.right()));
             ++x) {
            const float t = rect.w > 0.0f ? (static_cast<float>(x) - rect.x) / rect.w : 0.0f;
            const float cov = coverageOf(rect.x, rect.right(), x) *
                              coverageOf(rect.y, rect.bottom(), y);
            if (cov > 0.0f) {
                blendPixel(x, y, mix(left, right, t), cov);
            }
        }
    }
}

void Canvas::dim(float alpha) {
    if (alpha <= 0.0f) {
        return;
    }
    const std::uint32_t amount = static_cast<std::uint32_t>(std::clamp(alpha, 0.0f, 1.0f) * 256.0f);
    const std::uint32_t keep = 256u - amount;
    for (int y = 0; y < height_; ++y) {
        std::uint32_t* row = pixels_ + static_cast<std::ptrdiff_t>(y) * pitch_;
        for (int x = 0; x < width_; ++x) {
            const std::uint32_t p = row[x];
            const std::uint32_t r = ((p & 0xFF) * keep) >> 8;
            const std::uint32_t g = (((p >> 8) & 0xFF) * keep) >> 8;
            const std::uint32_t b = (((p >> 16) & 0xFF) * keep) >> 8;
            row[x] = r | (g << 8) | (b << 16) | 0xFF000000u;
        }
    }
}

void Canvas::blendAlpha(int x, int y, int w, int h, const std::uint8_t* alpha, Color color,
                        float opacity) {
    for (int row = 0; row < h; ++row) {
        const std::uint8_t* src = alpha + static_cast<std::size_t>(row) * w;
        for (int column = 0; column < w; ++column) {
            const float a = (static_cast<float>(src[column]) / 255.0f) * opacity;
            if (a > 0.0f) {
                blendPixel(x + column, y + row, color, a);
            }
        }
    }
}

void Canvas::blendAlphaGradient(int x, int y, int w, int h, const std::uint8_t* alpha,
                                const Color* columns, float opacity) {
    for (int row = 0; row < h; ++row) {
        const std::uint8_t* src = alpha + static_cast<std::size_t>(row) * w;
        for (int column = 0; column < w; ++column) {
            const float a = (static_cast<float>(src[column]) / 255.0f) * opacity;
            if (a > 0.0f) {
                blendPixel(x + column, y + row, columns[column], a);
            }
        }
    }
}

} // namespace nxf
