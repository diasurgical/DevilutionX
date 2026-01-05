/**
 * @file floatingnumbers.h
 *
 * Adds floating numbers QoL feature
 */
#pragma once

#include "engine/point.hpp"
#include "tables/misdat.h"
#include "monster.h"
#include "player.h"

namespace devilution {

void AddFloatingNumber(DamageType damageType, const Monster &monster, int damage);
void AddFloatingNumber(DamageType damageType, const Player &player, int damage);
void DrawFloatingNumbers(const Surface &out, Point viewPosition, Displacement offset);
void ClearFloatingNumbers();

} // namespace devilution
