/**
 * @file objdat.cpp
 *
 * Implementation of all object data.
 */
#include "objdat.h"

#include <string>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <expected.hpp>

#include "cursor.h"
#include "data/file.hpp"
#include "data/iterators.hpp"
#include "data/record_reader.hpp"

namespace devilution {

/** Maps from dun_object_id to object_id. */
const _object_id ObjTypeConv[] = {
	OBJ_NULL,
	OBJ_LEVER,
	OBJ_CRUX1,
	OBJ_CRUX2,
	OBJ_CRUX3,
	OBJ_ANGEL,
	OBJ_BANNERL,
	OBJ_BANNERM,
	OBJ_BANNERR,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_BOOK2L,
	OBJ_BOOK2R,
	OBJ_BCROSS,
	OBJ_NULL,
	OBJ_CANDLE1,
	OBJ_CANDLE2,
	OBJ_CANDLEO,
	OBJ_CAULDRON,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_FLAMEHOLE,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_MCIRCLE1,
	OBJ_MCIRCLE2,
	OBJ_SKFIRE,
	OBJ_SKPILE,
	OBJ_SKSTICK1,
	OBJ_SKSTICK2,
	OBJ_SKSTICK3,
	OBJ_SKSTICK4,
	OBJ_SKSTICK5,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_SWITCHSKL,
	OBJ_NULL,
	OBJ_TRAPL,
	OBJ_TRAPR,
	OBJ_TORTURE1,
	OBJ_TORTURE2,
	OBJ_TORTURE3,
	OBJ_TORTURE4,
	OBJ_TORTURE5,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NUDEW2R,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_TNUDEM1,
	OBJ_TNUDEM2,
	OBJ_TNUDEM3,
	OBJ_TNUDEM4,
	OBJ_TNUDEW1,
	OBJ_TNUDEW2,
	OBJ_TNUDEW3,
	OBJ_CHEST1,
	OBJ_CHEST1,
	OBJ_CHEST1,
	OBJ_CHEST2,
	OBJ_CHEST2,
	OBJ_CHEST2,
	OBJ_CHEST3,
	OBJ_CHEST3,
	OBJ_CHEST3,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_PEDESTAL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_ALTBOY,
	OBJ_NULL,
	OBJ_NULL,
	OBJ_WARARMOR,
	OBJ_WARWEAP,
	OBJ_TORCHR2,
	OBJ_TORCHL2,
	OBJ_MUSHPATCH,
	OBJ_STAND,
	OBJ_TORCHL,
	OBJ_TORCHR,
	OBJ_FLAMELVR,
	OBJ_SARC,
	OBJ_BARREL,
	OBJ_BARRELEX,
	OBJ_BOOKSHELF,
	OBJ_BOOKCASEL,
	OBJ_BOOKCASER,
	OBJ_ARMORSTANDN,
	OBJ_WEAPONRACKN,
	OBJ_BLOODFTN,
	OBJ_PURIFYINGFTN,
	OBJ_SHRINEL,
	OBJ_SHRINER,
	OBJ_GOATSHRINE,
	OBJ_MURKYFTN,
	OBJ_TEARFTN,
	OBJ_DECAP,
	OBJ_TCHEST1,
	OBJ_TCHEST2,
	OBJ_TCHEST3,
	OBJ_LAZSTAND,
	OBJ_BOOKSTAND,
	OBJ_BOOKSHELFR,
	OBJ_POD,
	OBJ_PODEX,
	OBJ_URN,
	OBJ_URNEX,
	OBJ_L5BOOKS,
	OBJ_L5CANDLE,
	OBJ_L5LEVER,
	OBJ_L5SARC,
};

/** Contains the data related to each object ID. */
std::vector<ObjectData> AllObjects;

/** Maps from object_graphic_id to object CEL name. */
std::vector<std::string> ObjMasterLoadList;

namespace {

tl::expected<theme_id, std::string> ParseTheme(std::string_view value)
{
	if (value.empty()) return THEME_NONE;
	if (value == "THEME_BARREL") return THEME_BARREL;
	if (value == "THEME_SHRINE") return THEME_SHRINE;
	if (value == "THEME_MONSTPIT") return THEME_MONSTPIT;
	if (value == "THEME_SKELROOM") return THEME_SKELROOM;
	if (value == "THEME_TREASURE") return THEME_TREASURE;
	if (value == "THEME_LIBRARY") return THEME_LIBRARY;
	if (value == "THEME_TORTURE") return THEME_TORTURE;
	if (value == "THEME_BLOODFOUNTAIN") return THEME_BLOODFOUNTAIN;
	if (value == "THEME_DECAPITATED") return THEME_DECAPITATED;
	if (value == "THEME_PURIFYINGFOUNTAIN") return THEME_PURIFYINGFOUNTAIN;
	if (value == "THEME_ARMORSTAND") return THEME_ARMORSTAND;
	if (value == "THEME_GOATSHRINE") return THEME_GOATSHRINE;
	if (value == "THEME_CAULDRON") return THEME_CAULDRON;
	if (value == "THEME_MURKYFOUNTAIN") return THEME_MURKYFOUNTAIN;
	if (value == "THEME_TEARFOUNTAIN") return THEME_TEARFOUNTAIN;
	if (value == "THEME_BRNCROSS") return THEME_BRNCROSS;
	if (value == "THEME_WEAPONRACK") return THEME_WEAPONRACK;
	return tl::make_unexpected("Unknown enum value");
}

tl::expected<quest_id, std::string> ParseQuest(std::string_view value)
{
	if (value.empty()) return Q_INVALID;
	if (value == "Q_ROCK") return Q_ROCK;
	if (value == "Q_MUSHROOM") return Q_MUSHROOM;
	if (value == "Q_GARBUD") return Q_GARBUD;
	if (value == "Q_ZHAR") return Q_ZHAR;
	if (value == "Q_VEIL") return Q_VEIL;
	if (value == "Q_DIABLO") return Q_DIABLO;
	if (value == "Q_BUTCHER") return Q_BUTCHER;
	if (value == "Q_LTBANNER") return Q_LTBANNER;
	if (value == "Q_BLIND") return Q_BLIND;
	if (value == "Q_BLOOD") return Q_BLOOD;
	if (value == "Q_ANVIL") return Q_ANVIL;
	if (value == "Q_WARLORD") return Q_WARLORD;
	if (value == "Q_SKELKING") return Q_SKELKING;
	if (value == "Q_PWATER") return Q_PWATER;
	if (value == "Q_SCHAMB") return Q_SCHAMB;
	if (value == "Q_BETRAYER") return Q_BETRAYER;
	if (value == "Q_GRAVE") return Q_GRAVE;
	if (value == "Q_FARMER") return Q_FARMER;
	if (value == "Q_GIRL") return Q_GIRL;
	if (value == "Q_TRADER") return Q_TRADER;
	if (value == "Q_DEFILER") return Q_DEFILER;
	if (value == "Q_NAKRUL") return Q_NAKRUL;
	if (value == "Q_CORNSTN") return Q_CORNSTN;
	if (value == "Q_JERSEY") return Q_JERSEY;
	return tl::make_unexpected("Unknown enum value");
}

tl::expected<ObjectDataFlags, std::string> ParseObjectDataFlags(std::string_view value)
{
	if (value.empty()) return ObjectDataFlags::None;
	if (value == "Animated") return ObjectDataFlags::Animated;
	if (value == "Solid") return ObjectDataFlags::Solid;
	if (value == "MissilesPassThrough") return ObjectDataFlags::MissilesPassThrough;
	if (value == "Light") return ObjectDataFlags::Light;
	if (value == "Trap") return ObjectDataFlags::Trap;
	if (value == "Breakable") return ObjectDataFlags::Breakable;
	return tl::make_unexpected("Unknown enum value");
}

tl::expected<SelectionRegion, std::string> ParseSelectionRegion(std::string_view value)
{
	if (value.empty()) return SelectionRegion::None;
	if (value == "Bottom") return SelectionRegion::Bottom;
	if (value == "Middle") return SelectionRegion::Middle;
	if (value == "Top") return SelectionRegion::Top;
	return tl::make_unexpected("Unknown enum value");
}

} // namespace

void LoadObjectData()
{
	const std::string_view filename = "txtdata\\objects\\objdat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	dataFile.skipHeaderOrDie(filename);

	AllObjects.clear();
	ObjMasterLoadList.clear();

	ankerl::unordered_dense::map<std::string, uint8_t> filenameToId;

	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		ObjectData &item = AllObjects.emplace_back();

		reader.advance(); // skip id

		std::string objFilename;
		reader.readString("file", objFilename);
		if (const auto it = filenameToId.find(objFilename); it != filenameToId.end()) {
			item.ofindex = it->second;
		} else {
			const auto id = static_cast<uint8_t>(ObjMasterLoadList.size());
			ObjMasterLoadList.push_back(objFilename);
			filenameToId.emplace(std::move(objFilename), id);
			item.ofindex = id;
		}

		reader.readInt("minLevel", item.minlvl);
		reader.readInt("maxLevel", item.maxlvl);
		reader.read("levelType", item.olvltype, ParseDungeonType);
		reader.read("theme", item.otheme, ParseTheme);
		reader.read("quest", item.oquest, ParseQuest);
		reader.readEnumList("flags", item.flags, ParseObjectDataFlags);
		reader.readInt("animDelay", item.animDelay);
		reader.readInt("animLen", item.animLen);
		reader.readInt("animWidth", item.animWidth);
		reader.readEnumList("selectionRegion", item.selectionRegion, ParseSelectionRegion);
	}

	// Sanity check because we do not actually parse the IDs yet.
	assert(static_cast<size_t>(OBJ_LAST) + 1 == AllObjects.size());

	AllObjects.shrink_to_fit();
	ObjMasterLoadList.shrink_to_fit();
}

} // namespace devilution
