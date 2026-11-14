/*
 * NXF Pro - PNG decoder wrapper (stb_image).
 * MIT licensed, see LICENSE.
 */
#include "nxf/image.hpp"

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

} // namespace nxf
