/**
 * @file floatingnumbers.h
 *
 * Adds floating numbers QoL feature
 */
#pragma once

#include <string>

#include "DiabloUI/ui_flags.hpp"
#include "engine/displacement.hpp"
#include "engine/point.hpp"
#include "engine/surface.hpp"

namespace devilution {

void AddFloatingNumber(Point pos, Displacement offset, std::string text, UiFlags style, int id = 0, bool reverseDirection = false);
void DrawFloatingNumbers(const Surface &out, Point viewPosition, Displacement offset);
void ClearFloatingNumbers();

} // namespace devilution
