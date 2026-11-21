/**
 * @file shrinedat.hpp
 *
 * Interface for loading shrine data from TSV files.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "engine/random.hpp"
#include "player.h"
#include "utils/attributes.h"

namespace devilution {

#define MAX_SHRINE_EFFECTS 3

enum class ShrineGametype : uint8_t {
	ShrineTypeAny,
	ShrineTypeSingle,
	ShrineTypeMulti,
};

enum class ShrineAffectsPlayer : uint8_t {
	Activator,
	Others,
	All,
};

enum class ShrineMessage : uint8_t {
	ShrineMessageDefault,
	ShrineMessage1,
	ShrineMessage2,
	ShrineMessage3,
	ShrineMessage4,
	ShrineMessage5,
};

enum class ShrineOperateFlag : uint8_t {
	// clang-format off
	None = 0,
	Crippling = 1 << 0,
	UnavailableRandom = 1 << 1,

	Last = UnavailableRandom
	// clang-format on
};
use_enum_as_flags(ShrineOperateFlag);

struct ShrineEffect {
	using TypeFn = ShrineMessage (*)(DiabloGenerator &, Player &, const Point, const ShrineEffect &);

	TypeFn typeFn;
	int param1;
	int param2;
	SpellID spellId;
	ShrineAffectsPlayer affectsPlayer;
};

struct ShrineData {

	std::string name;
	ShrineEffect shrineEffect[MAX_SHRINE_EFFECTS];
	ShrineGametype gameType;
	dungeon_type minLevelType;
	dungeon_type maxLevelType;
	ShrineOperateFlag shrineOperateFlags;
	std::string message;
	std::string message2;
	std::string message3;
	std::string message4;
	std::string message5;
};

extern DVL_API_FOR_TEST std::vector<ShrineData> ShrinesData;

void LoadShrineData();
const ShrineData &GetShrineData(uint8_t type);

} // namespace devilution

template <>
struct magic_enum::customize::enum_range<devilution::ShrineOperateFlag> {
	static constexpr uint8_t min = static_cast<uint64_t>(devilution::ShrineOperateFlag::None);
	static constexpr uint8_t max = static_cast<uint64_t>(devilution::ShrineOperateFlag::Last);
};
