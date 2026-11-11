/**
 * @file shrinedat.hpp
 *
 * Interface for loading shrine data from TSV files.
 */
#pragma once

#include <cstdint>
#include <engine/random.hpp>
#include <player.h>
#include <string>
#include <vector>

namespace devilution {

enum class ShrineGametype : uint8_t {
	ShrineTypeAny,
	ShrineTypeSingle,
	ShrineTypeMulti,
};

struct ShrineData {
	using OperateFn = int (*)(DiabloGenerator &, Player &, Point, int, int);

	std::string name;
	std::string message;
	std::string message2;
	std::string message3;
	std::string message4;
	OperateFn operateFn;
	int powerParam1;
	int powerParam2;
	ShrineGametype gameType;
	dungeon_type minLevelType;
	dungeon_type maxLevelType;
	bool availableRandomEffectShrine;
	bool cripplingEffect;
};

extern std::vector<ShrineData> ShrinesData;

void LoadShrineData();
const ShrineData &GetShrineData(uint8_t type);

} // namespace devilution
