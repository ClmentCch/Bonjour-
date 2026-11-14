/*
 * NXF Pro - vector icons.
 * MIT licensed, see LICENSE.
 */
#include "nxf/icons.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace nxf {

namespace {

struct Pen {
    Canvas& canvas;
    float x;
    float y;
    float s;
    Color color;
    float opacity;
    float thickness;

    float px(float v) const { return x + v * s; }
    float py(float v) const { return y + v * s; }

    Color tint() const { return withAlpha(color, (static_cast<float>(color.a) / 255.0f) * opacity); }

    void line(float x0, float y0, float x1, float y1, float width = 2.0f) const {
        canvas.drawLine(px(x0), py(y0), px(x1), py(y1), tint(), width * s);
    }
    void rect(float x0, float y0, float w, float h, float radius = 0.0f) const {
        canvas.fillRoundRect(Rect{px(x0), py(y0), w * s, h * s}, radius * s, tint());
    }
    void strokeRect(float x0, float y0, float w, float h, float radius = 0.0f,
                    float width = 1.6f) const {
        canvas.strokeRoundRect(Rect{px(x0), py(y0), w * s, h * s}, radius * s, tint(), width * s);
    }
    void circle(float cx, float cy, float radius) const {
        canvas.fillCircle(px(cx), py(cy), radius * s, tint());
    }
    void ring(float cx, float cy, float radius, float width = 1.8f) const {
        canvas.strokeCircle(px(cx), py(cy), radius * s, tint(), width * s);
    }
    void poly(std::initializer_list<float> points) const {
        std::vector<float> mapped;
        mapped.reserve(points.size());
        bool flip = false;
        for (float value : points) {
            mapped.push_back(flip ? py(value) : px(value));
            flip = !flip;
        }
        canvas.fillPolygon(mapped.data(), static_cast<int>(mapped.size() / 2), tint());
    }
    void arc(float cx, float cy, float radius, float fromDeg, float toDeg, float width = 1.8f) const {
        const int steps = 18;
        float prevX = 0, prevY = 0;
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / steps;
            const float angle = (fromDeg + (toDeg - fromDeg) * t) * 3.14159265f / 180.0f;
            const float ax = cx + std::cos(angle) * radius;
            const float ay = cy + std::sin(angle) * radius;
            if (i > 0) {
                line(prevX, prevY, ax, ay, width);
            }
            prevX = ax;
            prevY = ay;
        }
    }
};

const char* extensionOf(const char* name) {
    const char* dot = nullptr;
    for (const char* p = name; *p; ++p) {
        if (*p == '.') {
            dot = p;
        }
    }
    return dot != nullptr ? dot + 1 : "";
}

bool equalsIgnoreCase(const char* a, const char* b) {
    while (*a && *b) {
        const char ca = (*a >= 'A' && *a <= 'Z') ? static_cast<char>(*a + 32) : *a;
        const char cb = (*b >= 'A' && *b <= 'Z') ? static_cast<char>(*b + 32) : *b;
        if (ca != cb) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

void drawDocument(const Pen& pen, Icon kind) {
    // Page with a folded top-right corner.
    pen.strokeRect(5.0f, 3.2f, 14.0f, 17.6f, 2.2f, 1.5f);
    pen.line(14.6f, 3.2f, 19.0f, 7.8f, 1.4f);
    pen.line(14.6f, 3.2f, 14.6f, 7.8f, 1.2f);
    pen.line(14.6f, 7.8f, 19.0f, 7.8f, 1.2f);

    if (kind == Icon::DocText || kind == Icon::Doc) {
        pen.line(8.2f, 11.4f, 15.4f, 11.4f, 1.35f);
        pen.line(8.2f, 14.4f, 15.4f, 14.4f, 1.35f);
        pen.line(8.2f, 17.4f, 12.6f, 17.4f, 1.35f);
    } else if (kind == Icon::DocImage) {
        pen.poly({7.6f, 17.6f, 10.8f, 13.2f, 13.0f, 15.8f, 14.6f, 14.0f, 16.4f, 17.6f});
        pen.circle(14.8f, 10.6f, 1.15f);
    } else if (kind == Icon::DocAudio) {
        pen.circle(10.0f, 16.6f, 1.9f);
        pen.line(11.9f, 16.6f, 11.9f, 10.0f, 1.5f);
        pen.line(11.9f, 10.0f, 15.4f, 11.4f, 1.5f);
    } else if (kind == Icon::DocVideo) {
        pen.poly({9.8f, 10.4f, 15.8f, 13.6f, 9.8f, 16.8f});
    } else if (kind == Icon::DocArchive) {
        for (int i = 0; i < 4; ++i) {
            pen.rect(10.6f, 9.4f + static_cast<float>(i) * 2.5f, 2.4f, 1.5f, 0.5f);
        }
    } else if (kind == Icon::DocCode) {
        pen.line(10.6f, 10.4f, 8.2f, 13.4f, 1.5f);
        pen.line(8.2f, 13.4f, 10.6f, 16.4f, 1.5f);
        pen.line(13.4f, 10.4f, 15.8f, 13.4f, 1.5f);
        pen.line(15.8f, 13.4f, 13.4f, 16.4f, 1.5f);
    }
}

} // namespace

Icon iconForFile(const char* name, bool isDirectory) {
    if (isDirectory) {
        return Icon::Folder;
    }
    const char* ext = extensionOf(name);
    if (equalsIgnoreCase(ext, "png") || equalsIgnoreCase(ext, "jpg") ||
        equalsIgnoreCase(ext, "jpeg") || equalsIgnoreCase(ext, "bmp") ||
        equalsIgnoreCase(ext, "gif") || equalsIgnoreCase(ext, "webp")) {
        return Icon::DocImage;
    }
    if (equalsIgnoreCase(ext, "mp3") || equalsIgnoreCase(ext, "wav") ||
        equalsIgnoreCase(ext, "flac") || equalsIgnoreCase(ext, "ogg") ||
        equalsIgnoreCase(ext, "m4a")) {
        return Icon::DocAudio;
    }
    if (equalsIgnoreCase(ext, "mp4") || equalsIgnoreCase(ext, "mkv") ||
        equalsIgnoreCase(ext, "avi") || equalsIgnoreCase(ext, "mov") ||
        equalsIgnoreCase(ext, "webm")) {
        return Icon::DocVideo;
    }
    if (equalsIgnoreCase(ext, "zip") || equalsIgnoreCase(ext, "rar") ||
        equalsIgnoreCase(ext, "7z") || equalsIgnoreCase(ext, "nro") ||
        equalsIgnoreCase(ext, "nsp") || equalsIgnoreCase(ext, "xci")) {
        return Icon::DocArchive;
    }
    if (equalsIgnoreCase(ext, "cpp") || equalsIgnoreCase(ext, "h") ||
        equalsIgnoreCase(ext, "hpp") || equalsIgnoreCase(ext, "c") ||
        equalsIgnoreCase(ext, "py") || equalsIgnoreCase(ext, "js") ||
        equalsIgnoreCase(ext, "json") || equalsIgnoreCase(ext, "xml") ||
        equalsIgnoreCase(ext, "ini") || equalsIgnoreCase(ext, "sh")) {
        return Icon::DocCode;
    }
    if (equalsIgnoreCase(ext, "txt") || equalsIgnoreCase(ext, "md") ||
        equalsIgnoreCase(ext, "log")) {
        return Icon::DocText;
    }
    return Icon::Doc;
}

void drawIcon(Canvas& canvas, Icon icon, const Rect& box, Color color, float opacity) {
    const float s = box.w / 24.0f;
    Pen pen{canvas, box.x, box.y, s, color, opacity, 1.0f};

    switch (icon) {
        case Icon::Folder:
            pen.rect(3.0f, 5.0f, 8.4f, 4.8f, 1.7f);
            pen.rect(3.0f, 8.2f, 18.0f, 11.4f, 2.6f);
            break;
        case Icon::FolderOpen:
            pen.rect(3.0f, 5.0f, 8.4f, 4.8f, 1.7f);
            pen.poly({3.2f, 9.2f, 21.2f, 9.2f, 18.4f, 19.6f, 3.2f, 19.6f});
            break;
        case Icon::Doc:
        case Icon::DocText:
        case Icon::DocImage:
        case Icon::DocAudio:
        case Icon::DocVideo:
        case Icon::DocArchive:
        case Icon::DocCode:
            drawDocument(pen, icon);
            break;
        case Icon::Settings: {
            for (int i = 0; i < 8; ++i) {
                const float angle = static_cast<float>(i) * 45.0f * 3.14159265f / 180.0f;
                const float cx = 12.0f + std::cos(angle) * 8.2f;
                const float cy = 12.0f + std::sin(angle) * 8.2f;
                pen.rect(cx - 1.3f, cy - 1.3f, 2.6f, 2.6f, 0.8f);
            }
            pen.ring(12.0f, 12.0f, 6.2f, 2.0f);
            pen.circle(12.0f, 12.0f, 2.0f);
            break;
        }
        case Icon::Info:
            pen.ring(12.0f, 12.0f, 8.6f, 1.8f);
            pen.circle(12.0f, 7.8f, 1.25f);
            pen.line(12.0f, 11.0f, 12.0f, 16.6f, 1.9f);
            break;
        case Icon::Network:
            pen.arc(12.0f, 12.0f, 8.0f, 200.0f, 340.0f, 1.7f);
            pen.line(12.0f, 3.6f, 12.0f, 9.4f, 1.8f);
            pen.poly({9.4f, 7.0f, 12.0f, 10.6f, 14.6f, 7.0f});
            pen.arc(12.0f, 12.0f, 8.0f, 20.0f, 160.0f, 1.7f);
            pen.line(12.0f, 20.4f, 12.0f, 14.6f, 1.8f);
            pen.poly({9.4f, 17.0f, 12.0f, 13.4f, 14.6f, 17.0f});
            break;
        case Icon::SdCard:
            pen.poly({6.0f, 3.0f, 15.0f, 3.0f, 19.0f, 7.0f, 19.0f, 21.0f, 6.0f, 21.0f});
            pen.rect(8.4f, 6.0f, 1.5f, 4.0f, 0.6f);
            pen.rect(11.0f, 6.0f, 1.5f, 4.0f, 0.6f);
            pen.rect(13.6f, 6.0f, 1.5f, 4.0f, 0.6f);
            break;
        case Icon::Memory:
            pen.rect(6.5f, 6.5f, 11.0f, 11.0f, 2.0f);
            for (int i = 0; i < 4; ++i) {
                pen.line(9.0f + static_cast<float>(i) * 2.2f, 2.4f, 9.0f + static_cast<float>(i) * 2.2f,
                         5.6f, 1.3f);
                pen.line(9.0f + static_cast<float>(i) * 2.2f, 18.4f,
                         9.0f + static_cast<float>(i) * 2.2f, 21.6f, 1.3f);
            }
            pen.circle(12.0f, 12.0f, 2.0f);
            break;
        case Icon::ChevronRight:
            pen.line(9.5f, 5.5f, 16.0f, 12.0f, 2.0f);
            pen.line(16.0f, 12.0f, 9.5f, 18.5f, 2.0f);
            break;
        case Icon::ChevronLeft:
            pen.line(14.5f, 5.5f, 8.0f, 12.0f, 2.0f);
            pen.line(8.0f, 12.0f, 14.5f, 18.5f, 2.0f);
            break;
        case Icon::ChevronDown:
            pen.line(5.5f, 9.5f, 12.0f, 16.0f, 2.0f);
            pen.line(12.0f, 16.0f, 18.5f, 9.5f, 2.0f);
            break;
        case Icon::Grid:
            pen.rect(4.0f, 4.0f, 6.6f, 6.6f, 1.6f);
            pen.rect(13.4f, 4.0f, 6.6f, 6.6f, 1.6f);
            pen.rect(4.0f, 13.4f, 6.6f, 6.6f, 1.6f);
            pen.rect(13.4f, 13.4f, 6.6f, 6.6f, 1.6f);
            break;
        case Icon::Rows:
            for (int i = 0; i < 3; ++i) {
                pen.rect(4.0f, 5.2f + static_cast<float>(i) * 5.4f, 2.4f, 2.4f, 0.8f);
                pen.rect(8.4f, 5.9f + static_cast<float>(i) * 5.4f, 11.6f, 1.5f, 0.7f);
            }
            break;
        case Icon::Search:
            pen.ring(10.6f, 10.6f, 6.2f, 1.9f);
            pen.line(15.4f, 15.4f, 20.2f, 20.2f, 2.2f);
            break;
        case Icon::Sort:
            pen.line(4.0f, 6.5f, 14.0f, 6.5f, 1.7f);
            pen.line(4.0f, 12.0f, 11.0f, 12.0f, 1.7f);
            pen.line(4.0f, 17.5f, 8.0f, 17.5f, 1.7f);
            pen.poly({16.5f, 8.5f, 21.0f, 8.5f, 18.75f, 15.5f});
            break;
        case Icon::Plus:
            pen.line(12.0f, 5.0f, 12.0f, 19.0f, 2.1f);
            pen.line(5.0f, 12.0f, 19.0f, 12.0f, 2.1f);
            break;
        case Icon::Trash:
            pen.rect(9.0f, 3.4f, 6.0f, 2.0f, 0.8f);
            pen.rect(4.6f, 6.2f, 14.8f, 1.8f, 0.7f);
            pen.strokeRect(6.6f, 8.6f, 10.8f, 11.6f, 1.6f, 1.6f);
            pen.line(10.0f, 11.5f, 10.0f, 17.2f, 1.3f);
            pen.line(14.0f, 11.5f, 14.0f, 17.2f, 1.3f);
            break;
        case Icon::Copy:
            pen.strokeRect(4.0f, 4.0f, 11.0f, 13.0f, 2.0f, 1.6f);
            pen.strokeRect(9.0f, 7.0f, 11.0f, 13.0f, 2.0f, 1.6f);
            break;
        case Icon::Cut:
            pen.ring(7.0f, 17.4f, 2.4f, 1.6f);
            pen.ring(17.0f, 17.4f, 2.4f, 1.6f);
            pen.line(8.2f, 15.4f, 16.6f, 4.6f, 1.7f);
            pen.line(15.8f, 15.4f, 7.4f, 4.6f, 1.7f);
            break;
        case Icon::Paste:
            pen.strokeRect(5.0f, 5.0f, 14.0f, 15.5f, 2.2f, 1.6f);
            pen.rect(9.0f, 2.8f, 6.0f, 3.6f, 1.2f);
            break;
        case Icon::Rename:
            pen.poly({5.0f, 18.6f, 6.2f, 14.0f, 16.2f, 4.2f, 19.4f, 7.4f, 9.4f, 17.2f});
            pen.line(15.0f, 5.4f, 18.2f, 8.6f, 1.4f);
            break;
        case Icon::Close:
            pen.line(6.0f, 6.0f, 18.0f, 18.0f, 2.0f);
            pen.line(18.0f, 6.0f, 6.0f, 18.0f, 2.0f);
            break;
        case Icon::Check:
            pen.line(5.0f, 12.4f, 10.0f, 17.4f, 2.2f);
            pen.line(10.0f, 17.4f, 19.0f, 6.6f, 2.2f);
            break;
        case Icon::Warning:
            pen.poly({12.0f, 3.6f, 21.4f, 20.2f, 2.6f, 20.2f});
            pen.line(12.0f, 9.6f, 12.0f, 14.6f, 1.8f);
            pen.circle(12.0f, 17.4f, 1.1f);
            break;
        case Icon::Power:
            pen.arc(12.0f, 12.6f, 7.4f, -60.0f, 240.0f, 2.0f);
            pen.line(12.0f, 3.2f, 12.0f, 11.4f, 2.0f);
            break;
        case Icon::Link:
            pen.ring(8.4f, 9.6f, 3.4f, 1.7f);
            pen.ring(15.6f, 14.4f, 3.4f, 1.7f);
            pen.line(10.8f, 12.0f, 13.2f, 12.0f, 1.7f);
            break;
    }
}

} // namespace nxf
