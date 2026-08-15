// Paints the two dynamic textures: the vitals monitor screen (every frame) and
// the teaching overlay (when the step changes).
#pragma once

#include "canvas.hpp"
#include "steps.hpp"

void paintMonitor(Canvas& c, const Vitals& v, float ecgPhase);

void paintPanel(Canvas& c, int stepIndex, int total, bool following, bool hotspots);
