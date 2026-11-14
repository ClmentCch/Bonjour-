/*
 * NXF Pro - PNG decoder wrapper (stb_image).
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <cstddef>

#include "nxf/canvas.hpp"

namespace nxf {

/// Decodes an in-memory PNG/JPEG into an RGBA image. Returns false when the
/// data cannot be decoded.
bool loadImageFromMemory(const void* data, std::size_t size, Image& out);

/// Decodes and box-downsamples so the longest side is at most `maxSize` pixels.
/// Used for application icons, which stay small enough to cache.
bool loadImageScaled(const void* data, std::size_t size, int maxSize, Image& out);

} // namespace nxf
