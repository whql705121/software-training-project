#pragma once

#include "cafeteria_types.h"

namespace cafeteria {

void BuildLayout(SimulationState& state);
void ResetSimulation(SimulationState& state);
void UpdateSimulation(SimulationState& state);

COLORREF StudentColor(StudentType type);
double Distance(Vec2 a, Vec2 b);

}  // namespace cafeteria
