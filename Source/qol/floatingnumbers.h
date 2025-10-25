/**
 * @file floatingnumbers.h
 *
 * Adds floating numbers QoL feature
 */
#pragma once

#include "engine/point.hpp"
#include "misdat.h"
#include "monster.h"
#include "player.h"

namespace devilution {

void AddFloatingNumber(FloatingNumberType floatingNumberType, const Monster &monster, int damage);
void AddFloatingNumber(FloatingNumberType floatingNumberType, const Player &player, int damage);
void DrawFloatingNumbers(const Surface &out, Point viewPosition, Displacement offset);
void ClearFloatingNumbers();

} // namespace devilution
