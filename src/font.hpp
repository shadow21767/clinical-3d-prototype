// Embedded 5x7 bitmap font covering printable ASCII (32-126).
//
// Each glyph is five column bytes; bit 0 is the top row, bit 6 the bottom. This
// is the only way to get text on screen in raw Vulkan without pulling in a
// font rasterizer.
#pragma once

#include <cstdint>

constexpr int FONT_W = 5;
// Eight rows so g, j, p, q, and y can hang below the baseline.
constexpr int FONT_H = 8;
constexpr char FONT_FIRST = 32;
constexpr char FONT_LAST = 126;

// Returns the five column bytes for a character, or the blank glyph.
const uint8_t* fontGlyph(char c);
