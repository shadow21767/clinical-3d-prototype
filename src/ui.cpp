#include "ui.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

const Color TEXT = rgb(232, 238, 244);
const Color MUTED = rgb(143, 163, 181);
const Color ACCENT = rgb(95, 208, 255);
const Color WARN = rgb(255, 179, 64);
const Color GREEN = rgb(77, 251, 139);
const Color PANEL_BG = rgb(16, 23, 31, 224);
const Color EDGE = rgb(120, 160, 190, 60);

std::string fmt(float v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", (int)std::lround(v));
    return buf;
}

// One cardiac cycle, phase in [0,1). Rough PQRST morphology.
float ecg(float p) {
    if (p < 0.10f) return 0.0f;
    if (p < 0.18f) return std::sin((p - 0.10f) / 0.08f * 3.14159f) * 0.12f;
    if (p < 0.24f) return 0.0f;
    if (p < 0.27f) return -((p - 0.24f) / 0.03f) * 0.12f;
    if (p < 0.31f) return -0.12f + ((p - 0.27f) / 0.04f) * 1.12f;
    if (p < 0.35f) return 1.0f - ((p - 0.31f) / 0.04f) * 1.30f;
    if (p < 0.39f) return -0.30f + ((p - 0.35f) / 0.04f) * 0.30f;
    if (p < 0.52f) return 0.0f;
    if (p < 0.68f) return std::sin((p - 0.52f) / 0.16f * 3.14159f) * 0.26f;
    return 0.0f;
}

}  // namespace

void paintMonitor(Canvas& c, const Vitals& v, float ecgPhase) {
    const int W = c.width(), H = c.height();
    c.clear(rgb(6, 19, 26));

    const Color grid = rgb(80, 140, 160, 42);
    for (int x = 0; x <= W; x += W / 20) c.fillRect(x, 0, 1, H, grid);
    for (int y = 0; y <= H; y += H / 10) c.fillRect(0, y, W, 1, grid);

    // ECG trace across the upper half.
    const float cycles = 3.2f;
    std::vector<float> xs, ys;
    for (int px = 0; px <= W; px += 2) {
        const float p = std::fmod((float)px / W * cycles + ecgPhase, 1.0f);
        xs.push_back((float)px);
        ys.push_back(H * 0.30f - ecg(p) * H * 0.26f);
    }
    c.polyline(xs, ys, 3.0f, GREEN);

    // Plethysmograph below it.
    xs.clear();
    ys.clear();
    for (int px = 0; px <= W; px += 2) {
        const float p = std::fmod((float)px / W * cycles + ecgPhase, 1.0f);
        const float s = std::max(0.0f, std::sin(p * 6.28318f));
        xs.push_back((float)px);
        ys.push_back(H * 0.62f - std::pow(s, 1.6f) * H * 0.11f);
    }
    c.polyline(xs, ys, 2.5f, rgb(95, 208, 255));

    // Readouts along the bottom.
    const int labelY = (int)(H * 0.74f);
    const int valueY = (int)(H * 0.80f);
    auto cell = [&](int x, const char* label, const std::string& value, const char* unit, Color color) {
        c.text(x, labelY, label, MUTED, 2);
        const int w = c.text(x, valueY, value, color, 5);
        c.text(x + w + 4, valueY + 20, unit, MUTED, 2);
    };

    cell(16, "HR", fmt(v.hr), "bpm", GREEN);
    cell((int)(W * 0.36f), "NIBP", fmt(v.sys) + "/" + fmt(v.dia), "mmHg", v.sys < 100 ? WARN : TEXT);
    cell((int)(W * 0.75f), "SpO2", fmt(v.spo2), "%", v.spo2 < 92 ? rgb(255, 107, 94) : ACCENT);

    if (v.sys < 100.0f || v.spo2 < 92.0f) {
        c.text(W - 190, 14, "CHECK PATIENT", rgb(255, 120, 80), 2);
    }
}

void paintPanel(Canvas& c, int stepIndex, int total, bool following, bool hotspots) {
    const int W = c.width(), H = c.height();
    const Step& s = steps()[stepIndex];
    c.clear(rgb(0, 0, 0, 0));

    // ---- top bar ----
    c.fillRect(0, 0, W, 54, rgb(6, 10, 14, 150));
    c.fillCircle(26, 27, 5, GREEN);
    c.text(40, 21, "CLINICAL PROCEDURE WALKTHROUGH", TEXT, 2);
    const std::string caseLine = "Trauma primary survey - adult, single responder";
    c.text(W - c.textWidth(caseLine, 2) - 26, 21, caseLine, MUTED, 2);

    // ---- step rail ----
    int railY = 76;
    for (int i = 0; i < total; i++) {
        const bool active = i == stepIndex;
        const bool done = i < stepIndex;
        if (active) {
            c.fillRect(20, railY - 6, 266, 34, rgb(95, 208, 255, 36));
            c.strokeRect(20, railY - 6, 266, 34, 1, rgb(95, 208, 255, 110));
        }
        const Color numColor = active ? ACCENT : (done ? GREEN : MUTED);
        c.strokeRect(30, railY - 1, 22, 22, 1, numColor);
        c.text(38, railY + 5, std::to_string(i + 1), numColor, 2);
        c.text(64, railY + 5, steps()[i].title, active ? TEXT : MUTED, 2);
        railY += 40;
    }

    // ---- teaching panel ----
    const int pw = 430;
    const int px = W - pw - 24;
    const int py = 132;
    const int ph = H - py - 74;
    c.fillRect(px, py, pw, ph, PANEL_BG);
    c.strokeRect(px, py, pw, ph, 1, EDGE);

    int y = py + 22;
    const int pad = 22;
    const int inner = pw - pad * 2;

    // phase chip and step counter
    const std::string phase = s.phase;
    const int chipW = c.textWidth(phase, 2) + 18;
    c.fillRect(px + pad, y - 5, chipW, 24, rgb(95, 208, 255, 36));
    c.text(px + pad + 9, y + 2, phase, ACCENT, 2);
    const std::string counter = "Step " + std::to_string(stepIndex + 1) + " of " + std::to_string(total);
    c.text(px + pw - pad - c.textWidth(counter, 2), y + 2, counter, MUTED, 2);
    y += 44;

    // Longer titles step down a size rather than running off the panel.
    const int titleScale = c.textWidth(s.title, 4) <= inner ? 4 : 3;
    c.text(px + pad, y, s.title, TEXT, titleScale);
    y += titleScale == 4 ? 46 : 38;

    y = c.textWrapped(px + pad, y, inner, s.narration, rgb(195, 208, 220), 2, 21);
    y += 16;

    c.text(px + pad, y, "KEY ACTIONS", MUTED, 2);
    y += 25;
    for (const std::string& action : s.actions) {
        c.fillCircle(px + pad + 4, y + 6, 3, ACCENT);
        y = c.textWrapped(px + pad + 18, y, inner - 18, action, TEXT, 2, 19);
        y += 7;
    }
    y += 8;

    // "watch for" callout: measured first so the fill goes behind the text.
    const int watchTop = y;
    const int watchInner = inner - 16;
    const int watchBottom =
        c.textWrapped(px + pad + 16, y + 30, watchInner, s.watch, WARN, 2, 19, /*draw=*/false);
    const int watchH = watchBottom - watchTop + 10;
    c.fillRect(px + pad, watchTop, inner, watchH, rgb(255, 179, 64, 26));
    c.fillRect(px + pad, watchTop, 3, watchH, WARN);
    c.text(px + pad + 16, watchTop + 9, "WATCH FOR", WARN, 2);
    c.textWrapped(px + pad + 16, watchTop + 30, watchInner, s.watch, rgb(240, 228, 205), 2, 19);

    // progress bar, flush to the panel's bottom edge
    const int barY = py + ph - 14;
    c.fillRect(px + pad, barY, inner, 3, rgb(255, 255, 255, 30));
    c.fillRect(px + pad, barY, inner * (stepIndex + 1) / total, 3, ACCENT);

    // ---- view bar ----
    const int vy = H - 54;
    const std::string mode = following ? "GUIDED VIEW" : "FREE LOOK";
    const int modeW = c.textWidth(mode, 2) + 18;
    c.fillRect(24, vy, modeW + 470, 34, rgb(16, 23, 31, 210));
    c.strokeRect(24, vy, modeW + 470, 34, 1, EDGE);
    c.fillRect(34, vy + 6, modeW, 22, following ? rgb(120, 160, 190, 36) : rgb(255, 179, 64, 36));
    c.text(43, vy + 13, mode, following ? MUTED : WARN, 2);
    const std::string tips = std::string("Arrows step - drag orbit - scroll zoom - R resets - H markers") +
                             (hotspots ? "" : " (hidden)");
    c.text(44 + modeW, vy + 13, tips, MUTED, 2);
}
