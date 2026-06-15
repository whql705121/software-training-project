#pragma once

#include "cafeteria_types.h"

namespace cafeteria {

void RenderSimulation(HWND hwnd, HDC targetDc, const SimulationState& state, bool paused);

}  // namespace cafeteria
