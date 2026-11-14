/*
 * NXF Pro - design tokens and colour helpers.
 * MIT licensed, see LICENSE.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nxf {

using u8 = std::uint8_t;
using u32 = std::uint32_t;

struct Color {
    u8 r = 0;
    u8 g = 0;
    u8 b = 0;
    u8 a = 255;
};

constexpr Color rgba(u8 r, u8 g, u8 b, u8 a = 255) { return Color{r, g, b, a}; }

/// 0xRRGGBB literal to Color.
constexpr Color hex(u32 value, u8 alpha = 255) {
    return Color{static_cast<u8>((value >> 16) & 0xFF), static_cast<u8>((value >> 8) & 0xFF),
                 static_cast<u8>(value & 0xFF), alpha};
}

/// Packs into the framebuffer's RGBX8888 layout (byte order R,G,B,X).
constexpr u32 pack(Color c) {
    return static_cast<u32>(c.r) | (static_cast<u32>(c.g) << 8) |
           (static_cast<u32>(c.b) << 16) | (static_cast<u32>(c.a) << 24);
}

inline u8 mixChannel(u8 a, u8 b, float t) {
    return static_cast<u8>(std::lround(static_cast<float>(a) + (static_cast<float>(b) - a) * t));
}

inline Color mix(Color a, Color b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{mixChannel(a.r, b.r, t), mixChannel(a.g, b.g, t), mixChannel(a.b, b.b, t),
                 mixChannel(a.a, b.a, t)};
}

inline Color withAlpha(Color c, float alpha) {
    c.a = static_cast<u8>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
    return c;
}

inline Color lighten(Color c, float amount) { return mix(c, hex(0xFFFFFF), amount); }
inline Color darken(Color c, float amount) { return mix(c, hex(0x000000), amount); }

// ---------------------------------------------------------------------------
// Design tokens
// ---------------------------------------------------------------------------
namespace theme {

constexpr int kScreenWidth = 1280;
constexpr int kScreenHeight = 720;

constexpr int kSidebarWidth = 300;
constexpr int kSidebarPadding = 20;
constexpr int kContentLeft = kSidebarWidth + 48;
constexpr int kContentRight = kScreenWidth - 48;

// Surfaces
inline const Color kBackground = hex(0x000000);
inline const Color kSidebar = hex(0x0C0C0F);
inline const Color kSidebarEdge = hex(0x1A1A20);
inline const Color kPanel = hex(0x101015);
inline const Color kPanelHover = hex(0x17171D);
inline const Color kPanelActive = hex(0x1D1D25);
inline const Color kBorder = hex(0x232329);
inline const Color kBorderStrong = hex(0x30303A);
inline const Color kOverlay = hex(0x000000, 0x00);

// Type
inline const Color kText = hex(0xFFFFFF);
inline const Color kTextSecondary = hex(0xA2A2B0);
inline const Color kTextMuted = hex(0x6A6A78);
inline const Color kTextDisabled = hex(0x4A4A56);

// Signals
inline const Color kDanger = hex(0xF25454);
inline const Color kSuccess = hex(0x35D07F);
inline const Color kWarning = hex(0xF5B93F);
inline const Color kAccent = hex(0x7C8CF8);

} // namespace theme

// ---------------------------------------------------------------------------
// Rainbow ramp: blue, cyan, green, yellow, orange, red, pink, violet
// ---------------------------------------------------------------------------
inline Color rainbowStops[8] = {
    hex(0x3B82F6), // blue
    hex(0x22D3EE), // cyan
    hex(0x34D399), // green
    hex(0xFDE047), // yellow
    hex(0xFB923C), // orange
    hex(0xEF4444), // red
    hex(0xF472B6), // pink
    hex(0xA855F7), // violet
};

/// Continuous rainbow, `t` in cycles (values outside [0,1) wrap).
inline Color rainbow(float t) {
    t = t - std::floor(t);
    const float scaled = t * 8.0f;
    const int index = static_cast<int>(scaled) & 7;
    const float frac = scaled - std::floor(scaled);
    return mix(rainbowStops[index], rainbowStops[(index + 1) & 7], frac);
}

// ---------------------------------------------------------------------------
// Easing
// ---------------------------------------------------------------------------
inline float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

inline float easeOutCubic(float t) {
    t = clamp01(t);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

inline float easeInOutCubic(float t) {
    t = clamp01(t);
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

inline float easeOutQuad(float t) {
    t = clamp01(t);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

/// Exponential smoothing that behaves the same at any frame rate.
inline float approach(float current, float target, float rate, float dt) {
    const float factor = 1.0f - std::exp(-rate * dt);
    return current + (target - current) * factor;
}

inline float lerp(float a, float b, float t) { return a + (b - a) * clamp01(t); }

} // namespace nxf
