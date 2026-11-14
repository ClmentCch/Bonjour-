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

} // namespace nxf
