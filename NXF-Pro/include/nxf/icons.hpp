/*
 * NXF Pro - vector icon set drawn with canvas primitives (no bitmaps).
 * MIT licensed, see LICENSE.
 */
#pragma once

#include "nxf/canvas.hpp"
#include "nxf/theme.hpp"

namespace nxf {

enum class Icon {
    Folder,
    FolderOpen,
    Doc,
    DocText,
    DocImage,
    DocAudio,
    DocVideo,
    DocArchive,
    DocCode,
    Settings,
    Info,
    Network,
    SdCard,
    Memory,
    ChevronRight,
    ChevronLeft,
    ChevronDown,
    Grid,
    Rows,
    Search,
    Sort,
    Plus,
    Trash,
    Copy,
    Cut,
    Paste,
    Rename,
    Close,
    Check,
    Warning,
    Power,
    Link,
    Apps,
    Install,
};

/// Maps a file name to the icon that represents it.
Icon iconForFile(const char* name, bool isDirectory);

/// Draws the icon inside `box` (square aspect expected).
void drawIcon(Canvas& canvas, Icon icon, const Rect& box, Color color, float opacity = 1.0f);

} // namespace nxf
