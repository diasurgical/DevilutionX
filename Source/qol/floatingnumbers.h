/**
 * @file floatingnumbers.h
 *
 * Adds floating numbers QoL feature
 */
#pragma once

#include "engine/point.hpp"
#include "monster.h"
#include "player.h"
#include "tables/misdat.h"

namespace devilution {

void AddFloatingNumber(DamageType damageType, const Monster &monster, int damage);
void AddFloatingNumber(DamageType damageType, const Player &player, int damage);
void DrawFloatingNumbers(const Surface &out, Point viewPosition, Displacement offset);
void ClearFloatingNumbers();

} // namespace devilution
