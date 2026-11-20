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
#include "shrines.hpp"

namespace devilution {

std::vector<ShrineData> ShrinesData;

namespace {

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::expected<ShrineEffect::TypeFn, std::string>
ParseShrineEffectType(std::string_view value)
{
	if (value.empty()) return nullptr;
	if (value == "RandomAttributeChange") return ShrineEffectRandomAttributeChange;
	if (value == "RandomItemMaxDurabilityChange") return ShrineEffectRandomItemMaxDurabilityChange;
	if (value == "ArmorClassChange") return ShrineEffectArmorClassChange;
	if (value == "MinDamageChange") return ShrineEffectMinDamageChange;
	if (value == "MaxDamageChange") return ShrineEffectMaxDamageChange;
	if (value == "OperatePlayerCastSpellFromPlayer") return ShrineEffectOperatePlayerCastSpellFromPlayer;
	if (value == "OtherPlayersCastSpellFromPlayer") return ShrineEffectOtherPlayersCastSpellFromPlayer;
	if (value == "AllPlayersCastSpellFromPlayer") return ShrineEffectAllPlayersCastSpellFromPlayer;
	if (value == "StavesChargesChange") return ShrineEffectStavesChargesChange;
	if (value == "ItemsDurabilityChange") return ShrineEffectItemsDurabilityChange;
	if (value == "RandomSpellLevelChange") return ShrineEffectRandomSpellLevelChange;
	if (value == "RefillsChests") return ShrineEffectRefillsChests;
	if (value == "ChangeSpellLevel") return ShrineEffectChangeSpellLevel;
	if (value == "ManaBaseReduction") return ShrineEffectManaBaseReduction;
	if (value == "RestoreLifeAndMana") return ShrineEffectRestoreLifeAndMana;
	if (value == "TransformPotionsToRejuvenation") return ShrineEffectTransformPotionsToRejuvenation;
	if (value == "MagicAttributeChange") return ShrineEffectMagicAttributeChange;
	if (value == "CreateManaOrRejuvPotions") return ShrineEffectCreateManaOrRejuvPotions;
	if (value == "CreateLifeOrRejuvPotions") return ShrineEffectCreateLifeOrRejuvPotions;
	if (value == "OperatePlayerCastSpellFromPlayerTowardZero") return ShrineEffectOperatePlayerCastSpellFromPlayerTowardZero;
	if (value == "OtherPlayersCastSpellFromPlayerTowardZero") return ShrineEffectOtherPlayersCastSpellFromPlayerTowardZero;
	if (value == "AllPlayersCastSpellFromPlayerTowardZero") return ShrineEffectAllPlayersCastSpellFromPlayerTowardZero;
	if (value == "FillEmptySlotsGold") return ShrineEffectFillEmptySlotsGold;
	if (value == "DexterityAttributeChange") return ShrineEffectDexterityAttributeChange;
	if (value == "StrengthAttributeChange") return ShrineEffectStrengthAttributeChange;
	if (value == "VitalityAttributeChange") return ShrineEffectVitalityAttributeChange;
	if (value == "RevealMap") return ShrineEffectRevealMap;
	if (value == "IdentifyItems") return ShrineEffectIdentifyItems;
	if (value == "PreferredAttributeChange") return ShrineEffectPreferredAttributeChange;
	if (value == "CastFirewallTrap") return ShrineEffectCastFirewallTrap;
	if (value == "ExperienceToMagicAttribute") return ShrineEffectExperienceToMagicAttribute;
	if (value == "GoldToExperience") return ShrineEffectGoldToExperience;
	if (value == "ExperiencePerDungeonLevel") return ShrineEffectExperiencePerDungeonLevel;
	if (value == "CastFlashBottomTrap") return ShrineEffectCastFlashBottomTrap;
	if (value == "CastSpellTrap") return ShrineEffectCastSpellTrap;
	if (value == "OperatePlayerCastSpellFromShrine") return ShrineEffectOperatePlayerCastSpellFromShrine;
	if (value == "OtherPlayersCastSpellFromShrine") return ShrineEffectOtherPlayersCastSpellFromShrine;
	if (value == "AllPlayersCastSpellFromShrine") return ShrineEffectAllPlayersCastSpellFromShrine;
	if (value == "LocalHourAttributeChange") return ShrineEffectLocalHourAttributeChange;
	if (value == "BreakItemOrTakeGold") return ShrineEffectBreakItemOrTakeGold;
	return std::unexpected("Unknown ShrineData::OperateFn name");
}

std::expected<ShrineAffectsPlayer, std::string> ParseShrineAffectsPlayer(std::string_view value)
{
	if (value == "Activator") return ShrineAffectsPlayer::Activator;
	if (value == "Others") return ShrineAffectsPlayer::Others;
	if (value == "All") return ShrineAffectsPlayer::All;
	return std::unexpected("Unknown shrine game type");
}

void ReadShrineEffect(RecordReader &reader, std::string_view fieldName, ShrineEffect &effect)
{
	reader.read(fieldName, effect.typeFn, ParseShrineEffectType);
	if (effect.typeFn != nullptr) {
		reader.readInt(StrCat(fieldName, ".value1"), effect.param1);
		reader.readInt(StrCat(fieldName, ".value2"), effect.param2);
		reader.read(StrCat(fieldName, ".spell"), effect.spellId, ParseSpellId);
		reader.read(StrCat(fieldName, ".affectsPlayer"), effect.affectsPlayer, ParseShrineAffectsPlayer);
	} else {
		reader.advance();
		effect.param1 = 0;
		reader.advance();
		effect.param2 = 0;
		reader.advance();
		effect.spellId = SpellID::Null;
		reader.advance();
		effect.affectsPlayer = ShrineAffectsPlayer::All;
	}
}

std::expected<ShrineGametype, std::string> ParseShrineGameType(std::string_view value)
{
	if (value == "Any") return ShrineGametype::ShrineTypeAny;
	if (value == "Single") return ShrineGametype::ShrineTypeSingle;
	if (value == "Multi") return ShrineGametype::ShrineTypeMulti;
	return std::unexpected("Unknown shrine game type");
}

std::expected<ShrineOperateFlag, std::string> ParseShrineOperateFlag(std::string_view value)
{
	const std::optional<ShrineOperateFlag> enumValueOpt = magic_enum::enum_cast<ShrineOperateFlag>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	return std::unexpected("Unknown enum value");
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
		for (size_t i = 0; i < MAX_SHRINE_EFFECTS; ++i) {
			ReadShrineEffect(reader, StrCat("effect", i), entry.shrineEffect[i]);
		}
		reader.read("gameType", entry.gameType, ParseShrineGameType);
		reader.read("minLevelType", entry.minLevelType, ParseDungeonType);
		reader.read("maxLevelType", entry.maxLevelType, ParseDungeonType);
		reader.readEnumList("shrineOperateFlags", entry.shrineOperateFlags, ParseShrineOperateFlag);
		reader.readString("message", entry.message);
		reader.readString("message2", entry.message2);
		reader.readString("message3", entry.message3);
		reader.readString("message4", entry.message4);
		reader.readString("message5", entry.message5);
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
