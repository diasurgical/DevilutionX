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

enum class ShrineOperateFlag : uint8_t {
	// clang-format off
	None = 0,
	Crippling = 1 << 0,
	UnavailableRandom = 1 << 1,
	RestoreLife = 1 << 2,
	RestoreMana = 1 << 3,
	SpellTargetsMonsters = 1 << 4,
	SpellTargetsPlayers = 1 << 5,

	Last = SpellTargetsPlayers
	// clang-format on
};
use_enum_as_flags(ShrineOperateFlag);

struct ShrineData {
	using OperateFn = int (*)(DiabloGenerator &, Player &, Point, const ShrineData &);

	std::string name;
	std::string message;
	std::string message2;
	std::string message3;
	std::string message4;
	OperateFn operateFn;
	int powerParam1;
	int powerParam2;
	SpellID spell;
	ShrineGametype gameType;
	dungeon_type minLevelType;
	dungeon_type maxLevelType;
	ShrineOperateFlag shrineOperateFlags;
};

extern std::vector<ShrineData> ShrinesData;

void LoadShrineData();
const ShrineData &GetShrineData(uint8_t type);

} // namespace devilution

template <>
struct magic_enum::customize::enum_range<devilution::ShrineOperateFlag> {
	static constexpr uint8_t min = static_cast<uint64_t>(devilution::ShrineOperateFlag::None);
	static constexpr uint8_t max = static_cast<uint64_t>(devilution::ShrineOperateFlag::Last);
};
