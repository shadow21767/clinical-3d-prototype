// A plain CPU RGBA8 image with just enough drawing to build the vitals monitor
// screen and the teaching panel, which are then uploaded as Vulkan textures.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) { return Color{r, g, b, a}; }

class Canvas {
public:
    Canvas(int width, int height);

    int width() const { return w_; }
    int height() const { return h_; }
    const uint8_t* data() const { return pixels_.data(); }
    size_t sizeBytes() const { return pixels_.size(); }

    void clear(Color c);
    void blend(int x, int y, Color c);
    void fillRect(int x, int y, int w, int h, Color c);
    void strokeRect(int x, int y, int w, int h, int thickness, Color c);
    void fillCircle(int cx, int cy, int r, Color c);
    void line(float x0, float y0, float x1, float y1, float thickness, Color c);
    // Connects consecutive points; used for the ECG and pulse traces.
    void polyline(const std::vector<float>& xs, const std::vector<float>& ys, float thickness, Color c);

    // Text. `scale` multiplies the 5x7 glyph size. Returns the advance width.
    int text(int x, int y, const std::string& s, Color c, int scale = 1, int tracking = 1);
    int textWidth(const std::string& s, int scale = 1, int tracking = 1) const;
    // Word-wraps within maxWidth and returns the y below the last line. Pass
    // draw = false to measure the block before painting anything behind it.
    int textWrapped(int x, int y, int maxWidth, const std::string& s, Color c, int scale, int lineHeight,
                    bool draw = true);

    bool writePng(const std::string& path) const;

private:
    int w_, h_;
    std::vector<uint8_t> pixels_;
};
