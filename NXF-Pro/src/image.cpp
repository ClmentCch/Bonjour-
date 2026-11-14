/*
 * NXF Pro - PNG decoder wrapper (stb_image).
 * MIT licensed, see LICENSE.
 */
#include "nxf/image.hpp"

#include <algorithm>
#include <utility>

#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_TGA
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace nxf {

bool loadImageFromMemory(const void* data, std::size_t size, Image& out) {
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* rgba = stbi_load_from_memory(static_cast<const stbi_uc*>(data),
                                                static_cast<int>(size), &width, &height, &channels, 4);
    if (rgba == nullptr) {
        return false;
    }

    out.width = width;
    out.height = height;
    out.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (std::size_t i = 0; i < out.pixels.size(); ++i) {
        out.pixels[i] = static_cast<std::uint32_t>(rgba[i * 4 + 0]) |
                        (static_cast<std::uint32_t>(rgba[i * 4 + 1]) << 8) |
                        (static_cast<std::uint32_t>(rgba[i * 4 + 2]) << 16) |
                        (static_cast<std::uint32_t>(rgba[i * 4 + 3]) << 24);
    }
    stbi_image_free(rgba);
    return true;
}

bool loadImageScaled(const void* data, std::size_t size, int maxSize, Image& out) {
    Image source;
    if (!loadImageFromMemory(data, size, source)) {
        return false;
    }

    const int longest = std::max(source.width, source.height);
    if (maxSize <= 0 || longest <= maxSize) {
        out = std::move(source);
        return true;
    }

    const int width = std::max(1, source.width * maxSize / longest);
    const int height = std::max(1, source.height * maxSize / longest);
    out.width = width;
    out.height = height;
    out.pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);

    for (int y = 0; y < height; ++y) {
        const int y0 = y * source.height / height;
        const int y1 = std::max(y0 + 1, (y + 1) * source.height / height);
        for (int x = 0; x < width; ++x) {
            const int x0 = x * source.width / width;
            const int x1 = std::max(x0 + 1, (x + 1) * source.width / width);
            unsigned r = 0, g = 0, b = 0, a = 0;
            int samples = 0;
            for (int sy = y0; sy < y1; ++sy) {
                const std::uint32_t* row = &source.pixels[static_cast<std::size_t>(sy) * source.width];
                for (int sx = x0; sx < x1; ++sx) {
                    const std::uint32_t pixel = row[sx];
                    r += pixel & 0xFFu;
                    g += (pixel >> 8) & 0xFFu;
                    b += (pixel >> 16) & 0xFFu;
                    a += (pixel >> 24) & 0xFFu;
                    ++samples;
                }
            }
            if (samples > 0) {
                const auto n = static_cast<std::uint32_t>(samples);
                out.pixels[static_cast<std::size_t>(y) * width + x] =
                    (r / n) | ((g / n) << 8) | ((b / n) << 16) | ((a / n) << 24);
            }
        }
    }
    return true;
}

} // namespace nxf
