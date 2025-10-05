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

enum class FloatingType : uint8_t {
	Physical,
	Fire,
	Lightning,
	Magic,
	Acid,
	Exp,
};

void AddFloatingNumber(FloatingType type, const Monster &monster, int value);
void AddFloatingNumber(FloatingType type, const Player &player, int value);
void AddFloatingNumber(DamageType damageType, const Monster &monster, int damage);
void AddFloatingNumber(DamageType damageType, const Player &player, int damage);
void DrawFloatingNumbers(const Surface &out, Point viewPosition, Displacement offset);
void ClearFloatingNumbers();

} // namespace devilution
