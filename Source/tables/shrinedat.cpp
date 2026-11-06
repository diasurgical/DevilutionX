/**
 * @file shrinedat.cpp
 *
 * Implementation of shrine data.
 */
#include "tables/shrinedat.hpp"

#include <cassert>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "data/file.hpp"
#include "data/record_reader.hpp"
#include "levels/parse_dungeon_type.hpp"
#include "objects.h"

namespace devilution {

std::vector<ShrineData> ShrinesData;

namespace {

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::expected<ShrineData::OperateFn, std::string>
ParseShrineOperateFn(std::string_view value)
{
	if (value.empty()) return std::unexpected("Empty ShrineData::OperateFn name");
	if (value == "OperateShrineMysterious") return OperateShrineMysterious;
	if (value == "OperateShrineHidden") return OperateShrineHidden;
	if (value == "OperateShrineGloomy") return OperateShrineGloomy;
	if (value == "OperateShrineWeird") return OperateShrineWeird;
	if (value == "OperateShrineMagical") return OperateShrineMagical;
	if (value == "OperateShrineStone") return OperateShrineStone;
	if (value == "OperateShrineReligious") return OperateShrineReligious;
	if (value == "OperateShrineEnchanted") return OperateShrineEnchanted;
	if (value == "OperateShrineThaumaturgic") return OperateShrineThaumaturgic;
	if (value == "OperateShrineFascinating") return OperateShrineFascinating;
	if (value == "OperateShrineCryptic") return OperateShrineCryptic;
	if (value == "OperateShrineEldritch") return OperateShrineEldritch;
	if (value == "OperateShrineEerie") return OperateShrineEerie;
	if (value == "OperateShrineDivine") return OperateShrineDivine;
	if (value == "OperateShrineHoly") return OperateShrineHoly;
	if (value == "OperateShrineSacred") return OperateShrineSacred;
	if (value == "OperateShrineSpiritual") return OperateShrineSpiritual;
	if (value == "OperateShrineSpooky") return OperateShrineSpooky;
	if (value == "OperateShrineAbandoned") return OperateShrineAbandoned;
	if (value == "OperateShrineCreepy") return OperateShrineCreepy;
	if (value == "OperateShrineQuiet") return OperateShrineQuiet;
	if (value == "OperateShrineSecluded") return OperateShrineSecluded;
	if (value == "OperateShrineOrnate") return OperateShrineOrnate;
	if (value == "OperateShrineGlimmering") return OperateShrineGlimmering;
	if (value == "OperateShrineTainted") return OperateShrineTainted;
	if (value == "OperateShrineOily") return OperateShrineOily;
	if (value == "OperateShrineGlowing") return OperateShrineGlowing;
	if (value == "OperateShrineMendicant") return OperateShrineMendicant;
	if (value == "OperateShrineSparkling") return OperateShrineSparkling;
	if (value == "OperateShrineTown") return OperateShrineTown;
	if (value == "OperateShrineShimmering") return OperateShrineShimmering;
	if (value == "OperateShrineSolar") return OperateShrineSolar;
	if (value == "OperateShrineMurphys") return OperateShrineMurphys;
	return std::unexpected("Unknown ShrineData::OperateFn name");
}

std::expected<ShrineGametype, std::string> ParseShrineGameType(std::string_view value)
{
	if (value == "Any") return ShrineGametype::ShrineTypeAny;
	if (value == "Single") return ShrineGametype::ShrineTypeSingle;
	if (value == "Multi") return ShrineGametype::ShrineTypeMulti;
	return std::unexpected("Unknown shrine game type");
}

} // namespace

void LoadShrineData()
{
	const std::string_view filename = "txtdata\\shrines\\shrines.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	dataFile.skipHeaderOrDie(filename);

	ShrinesData.clear();
	ShrinesData.reserve(dataFile.numRecords());

	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		ShrineData &entry = ShrinesData.emplace_back();

		reader.advance(); // Skip the first column (shrine ID).
		reader.readString("name", entry.name);
		reader.read("operateFn", entry.operateFn, ParseShrineOperateFn);
		reader.readInt("powerParam1", entry.powerParam1);
		reader.readInt("powerParam2", entry.powerParam2);
		reader.read("gameType", entry.gameType, ParseShrineGameType);
		reader.read("minLevelType", entry.minLevelType, ParseDungeonType);
		reader.read("maxLevelType", entry.maxLevelType, ParseDungeonType);
		reader.readBool("availableRandomEffectShrine", entry.availableRandomEffectShrine);
		reader.readBool("cripplingEffect", entry.cripplingEffect);
		reader.readString("message", entry.message);
		reader.readString("message2", entry.message2);
		reader.readString("message3", entry.message3);
		reader.readString("message4", entry.message4);
	}

	ShrinesData.shrink_to_fit();
}

const ShrineData &GetShrineData(uint8_t shrineType)
{
	const auto shrineIndex = static_cast<size_t>(shrineType);
	assert(shrineIndex < ShrinesData.size());
	return ShrinesData[shrineIndex];
}

} // namespace devilution
