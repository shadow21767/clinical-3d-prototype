#include "canvas.hpp"

#include "font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

Canvas::Canvas(int width, int height) : w_(width), h_(height), pixels_((size_t)width * height * 4, 0) {}

void Canvas::clear(Color c) {
    for (size_t i = 0; i < pixels_.size(); i += 4) {
        pixels_[i] = c.r;
        pixels_[i + 1] = c.g;
        pixels_[i + 2] = c.b;
        pixels_[i + 3] = c.a;
    }
}

void Canvas::blend(int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_ || c.a == 0) return;
    uint8_t* p = &pixels_[((size_t)y * w_ + x) * 4];
    if (c.a == 255) {
        p[0] = c.r;
        p[1] = c.g;
        p[2] = c.b;
        p[3] = 255;
        return;
    }
    const float a = c.a / 255.0f;
    p[0] = (uint8_t)(c.r * a + p[0] * (1 - a));
    p[1] = (uint8_t)(c.g * a + p[1] * (1 - a));
    p[2] = (uint8_t)(c.b * a + p[2] * (1 - a));
    p[3] = (uint8_t)std::min(255.0f, p[3] + c.a * a);
}

void Canvas::fillRect(int x, int y, int w, int h, Color c) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++) blend(i, j, c);
}

void Canvas::strokeRect(int x, int y, int w, int h, int t, Color c) {
    fillRect(x, y, w, t, c);
    fillRect(x, y + h - t, w, t, c);
    fillRect(x, y, t, h, c);
    fillRect(x + w - t, y, t, h, c);
}

void Canvas::fillCircle(int cx, int cy, int r, Color c) {
    for (int j = -r; j <= r; j++) {
        for (int i = -r; i <= r; i++) {
            if (i * i + j * j <= r * r) blend(cx + i, cy + j, c);
        }
    }
}

void Canvas::line(float x0, float y0, float x1, float y1, float thickness, Color c) {
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.0001f) return;
    const int steps = (int)std::ceil(len);
    const float half = thickness * 0.5f;
    for (int s = 0; s <= steps; s++) {
        const float t = (float)s / steps;
        const float x = x0 + dx * t, y = y0 + dy * t;
        // Stamp a small disc so thick lines stay connected around corners.
        for (int j = (int)std::floor(-half); j <= (int)std::ceil(half); j++) {
            for (int i = (int)std::floor(-half); i <= (int)std::ceil(half); i++) {
                if (i * i + j * j <= half * half + 0.25f) blend((int)std::round(x) + i, (int)std::round(y) + j, c);
            }
        }
    }
}

void Canvas::polyline(const std::vector<float>& xs, const std::vector<float>& ys, float thickness, Color c) {
    const size_t n = std::min(xs.size(), ys.size());
    for (size_t i = 1; i < n; i++) line(xs[i - 1], ys[i - 1], xs[i], ys[i], thickness, c);
}

int Canvas::text(int x, int y, const std::string& s, Color c, int scale, int tracking) {
    int cursor = x;
    for (char ch : s) {
        const uint8_t* glyph = fontGlyph(ch);
        for (int col = 0; col < FONT_W; col++) {
            for (int row = 0; row < FONT_H; row++) {
                if (!(glyph[col] & (1 << row))) continue;
                fillRect(cursor + col * scale, y + row * scale, scale, scale, c);
            }
        }
        cursor += (FONT_W + tracking) * scale;
    }
    return cursor - x;
}

int Canvas::textWidth(const std::string& s, int scale, int tracking) const {
    if (s.empty()) return 0;
    return (int)s.size() * (FONT_W + tracking) * scale - tracking * scale;
}

int Canvas::textWrapped(int x, int y, int maxWidth, const std::string& s, Color c, int scale, int lineHeight,
                        bool draw) {
    std::istringstream stream(s);
    std::string word, line;
    int cursorY = y;

    auto flush = [&]() {
        if (line.empty()) return;
        if (draw) text(x, cursorY, line, c, scale);
        cursorY += lineHeight;
        line.clear();
    };

    while (stream >> word) {
        const std::string candidate = line.empty() ? word : line + " " + word;
        if (textWidth(candidate, scale) > maxWidth && !line.empty()) {
            flush();
            line = word;
        } else {
            line = candidate;
        }
    }
    flush();
    return cursorY;
}

// ---- minimal PNG writer (store-mode deflate), for debug dumps only ----
namespace {

uint32_t crc32of(const uint8_t* data, size_t len, uint32_t crc = 0) {
    static uint32_t table[256];
    static bool built = false;
    if (!built) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        built = true;
    }
    crc = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

void be32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v >> 24));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)v);
}

void chunk(std::vector<uint8_t>& out, const char tag[5], const std::vector<uint8_t>& payload) {
    be32(out, (uint32_t)payload.size());
    std::vector<uint8_t> body(tag, tag + 4);
    body.insert(body.end(), payload.begin(), payload.end());
    out.insert(out.end(), body.begin(), body.end());
    be32(out, crc32of(body.data(), body.size()));
}

}  // namespace

bool Canvas::writePng(const std::string& path) const {
    // Raw scanlines with a zero filter byte per row.
    std::vector<uint8_t> raw;
    raw.reserve((size_t)h_ * (w_ * 4 + 1));
    for (int y = 0; y < h_; y++) {
        raw.push_back(0);
        const uint8_t* row = &pixels_[(size_t)y * w_ * 4];
        raw.insert(raw.end(), row, row + (size_t)w_ * 4);
    }

    // zlib stream using stored (uncompressed) deflate blocks.
    std::vector<uint8_t> z{0x78, 0x01};
    size_t offset = 0;
    while (offset < raw.size()) {
        const uint16_t block = (uint16_t)std::min<size_t>(65535, raw.size() - offset);
        const bool last = offset + block >= raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back((uint8_t)(block & 0xFF));
        z.push_back((uint8_t)(block >> 8));
        z.push_back((uint8_t)(~block & 0xFF));
        z.push_back((uint8_t)((~block >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + offset, raw.begin() + offset + block);
        offset += block;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    be32(z, (b << 16) | a);

    std::vector<uint8_t> png{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    be32(ihdr, (uint32_t)w_);
    be32(ihdr, (uint32_t)h_);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8-bit RGBA
    chunk(png, "IHDR", ihdr);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});

    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    std::fwrite(png.data(), 1, png.size(), f);
    std::fclose(f);
    return true;
}
