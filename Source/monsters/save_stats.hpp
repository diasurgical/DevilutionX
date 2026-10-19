#pragma once

#include <cstdint>

#include "monster.h"

namespace devilution {

/**
 * Version 1 of the wide monster stat extension in the legacy 216-byte record.
 * Magic: offset 24; damage: 164/166/184/186; armor: 206. These bytes were
 * unused or alignment in both Diablo and DevilutionX. Legacy stat bytes remain
 * readable, capped at 255. Records that fit in those bytes have no extension.
 */
struct MonsterStatSaveExtension {
	static constexpr uint32_t MagicVersion1 = 0x31534D58; // "XMS1"

	uint32_t magic = 0;
	uint16_t minDamage = 0;
	uint16_t maxDamage = 0;
	uint16_t minDamageSpecial = 0;
	uint16_t maxDamageSpecial = 0;
	uint16_t armorClass = 0;

	static MonsterStatSaveExtension fromMonster(const Monster &monster)
	{
		if (monster.minDamage <= 255 && monster.maxDamage <= 255
		    && monster.minDamageSpecial <= 255 && monster.maxDamageSpecial <= 255
		    && monster.armorClass <= 255)
			return {};

		return { MagicVersion1, monster.minDamage, monster.maxDamage,
			monster.minDamageSpecial, monster.maxDamageSpecial, monster.armorClass };
	}

	void applyTo(Monster &monster) const
	{
		if (magic != MagicVersion1)
			return;

		monster.minDamage = minDamage;
		monster.maxDamage = maxDamage;
		monster.minDamageSpecial = minDamageSpecial;
		monster.maxDamageSpecial = maxDamageSpecial;
		monster.armorClass = armorClass;
	}
};

} // namespace devilution
