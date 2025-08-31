/**
 * @file playerdat.cpp
 *
 * Implementation of all player data.
 */

#include "playerdat.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <charconv>
#include <cstdint>
#include <vector>

#include <expected.hpp>
#include <fmt/format.h>

#include "data/file.hpp"
#include "data/record_reader.hpp"
#include "items.h"
#include "player.h"
#include "textdat.h"
#include "utils/language.h"
#include "utils/static_vector.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

class ExperienceData {
	/** Specifies the experience point limit of each level. */
	std::vector<uint32_t> levelThresholds;

public:
	uint8_t getMaxLevel() const
	{
		return static_cast<uint8_t>(std::min<size_t>(levelThresholds.size(), std::numeric_limits<uint8_t>::max()));
	}

	DVL_REINITIALIZES void clear()
	{
		levelThresholds.clear();
	}

	[[nodiscard]] uint32_t getThresholdForLevel(unsigned level) const
	{
		if (level > 0)
			return levelThresholds[std::min<unsigned>(level - 1, getMaxLevel())];

		return 0;
	}

	void setThresholdForLevel(unsigned level, uint32_t experience)
	{
		if (level > 0) {
			if (level > levelThresholds.size()) {
				// To avoid ValidatePlayer() resetting players to 0 experience we need to use the maximum possible value here
				// As long as the file has no gaps it'll get initialised properly.
				levelThresholds.resize(level, std::numeric_limits<uint32_t>::max());
			}

			levelThresholds[static_cast<size_t>(level - 1)] = experience;
		}
	}
} ExperienceData;

enum class ExperienceColumn {
	Level,
	Experience,
	LAST = Experience
};

tl::expected<ExperienceColumn, ColumnDefinition::Error> mapExperienceColumnFromName(std::string_view name)
{
	if (name == "Level") {
		return ExperienceColumn::Level;
	}
	if (name == "Experience") {
		return ExperienceColumn::Experience;
	}
	return tl::unexpected { ColumnDefinition::Error::UnknownColumn };
}

void ReloadExperienceData()
{
	constexpr std::string_view filename = "txtdata\\Experience.tsv";
	auto dataFileResult = DataFile::load(filename);
	if (!dataFileResult.has_value()) {
		DataFile::reportFatalError(dataFileResult.error(), filename);
	}
	DataFile &dataFile = dataFileResult.value();

	constexpr unsigned ExpectedColumnCount = enum_size<ExperienceColumn>::value;

	std::array<ColumnDefinition, ExpectedColumnCount> columns;
	auto parseHeaderResult = dataFile.parseHeader<ExperienceColumn>(columns.data(), columns.data() + columns.size(), mapExperienceColumnFromName);

	if (!parseHeaderResult.has_value()) {
		DataFile::reportFatalError(parseHeaderResult.error(), filename);
	}

	ExperienceData.clear();
	for (DataFileRecord record : dataFile) {
		uint8_t level = 0;
		uint32_t experience = 0;
		bool skipRecord = false;

		FieldIterator fieldIt = record.begin();
		const FieldIterator endField = record.end();
		for (auto &column : columns) {
			fieldIt += column.skipLength;

			if (fieldIt == endField) {
				DataFile::reportFatalError(DataFile::Error::NotEnoughColumns, filename);
			}

			DataFileField field = *fieldIt;

			switch (static_cast<ExperienceColumn>(column)) {
			case ExperienceColumn::Level: {
				auto parseIntResult = field.parseInt(level);

				if (!parseIntResult.has_value()) {
					if (*field == "MaxLevel") {
						skipRecord = true;
					} else {
						DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Level", field);
					}
				}
			} break;

			case ExperienceColumn::Experience: {
				auto parseIntResult = field.parseInt(experience);

				if (!parseIntResult.has_value()) {
					DataFile::reportFatalFieldError(parseIntResult.error(), filename, "Experience", field);
				}
			} break;

			default:
				break;
			}

			if (skipRecord)
				break;

			++fieldIt;
		}

		if (!skipRecord)
			ExperienceData.setThresholdForLevel(level, experience);
	}
}

void LoadClassData(std::string_view classPath, ClassAttributes &attributes, PlayerCombatData &combat)
{
	const std::string filename = StrCat("txtdata\\classes\\", classPath, "\\attributes.tsv");
	tl::expected<DataFile, DataFile::Error> dataFileResult = DataFile::load(filename);
	if (!dataFileResult.has_value()) {
		DataFile::reportFatalError(dataFileResult.error(), filename);
	}
	DataFile &dataFile = dataFileResult.value();

	if (tl::expected<void, DataFile::Error> result = dataFile.skipHeader();
	    !result.has_value()) {
		DataFile::reportFatalError(result.error(), filename);
	}

	auto recordIt = dataFile.begin();
	const auto recordEnd = dataFile.end();

	const auto getValueField = [&](std::string_view expectedKey) {
		if (recordIt == recordEnd) {
			app_fatal(fmt::format("Missing field {} in {}", expectedKey, filename));
		}
		DataFileRecord record = *recordIt;
		FieldIterator fieldIt = record.begin();
		const FieldIterator endField = record.end();

		const std::string_view key = (*fieldIt).value();
		if (key != expectedKey) {
			app_fatal(fmt::format("Unexpected field in {}: got {}, expected {}", filename, key, expectedKey));
		}

		++fieldIt;
		if (fieldIt == endField) {
			DataFile::reportFatalError(DataFile::Error::NotEnoughColumns, filename);
		}
		return *fieldIt;
	};

	const auto valueReader = [&](auto &&readFn) {
		return [&](std::string_view expectedKey, auto &outValue) {
			DataFileField valueField = getValueField(expectedKey);
			if (const tl::expected<void, devilution::DataFileField::Error> result = readFn(valueField, outValue);
			    !result.has_value()) {
				DataFile::reportFatalFieldError(result.error(), filename, "Value", valueField);
			}
			++recordIt;
		};
	};

	const auto readInt = valueReader([](DataFileField &valueField, auto &outValue) {
		return valueField.parseInt(outValue);
	});
	const auto readDecimal = valueReader([](DataFileField &valueField, auto &outValue) {
		return valueField.parseFixed6(outValue);
	});

	readInt("baseStr", attributes.baseStr);
	readInt("baseMag", attributes.baseMag);
	readInt("baseDex", attributes.baseDex);
	readInt("baseVit", attributes.baseVit);
	readInt("maxStr", attributes.maxStr);
	readInt("maxMag", attributes.maxMag);
	readInt("maxDex", attributes.maxDex);
	readInt("maxVit", attributes.maxVit);
	readInt("blockBonus", combat.baseToBlock);
	readDecimal("adjLife", attributes.adjLife);
	readDecimal("adjMana", attributes.adjMana);
	readDecimal("lvlLife", attributes.lvlLife);
	readDecimal("lvlMana", attributes.lvlMana);
	readDecimal("chrLife", attributes.chrLife);
	readDecimal("chrMana", attributes.chrMana);
	readDecimal("itmLife", attributes.itmLife);
	readDecimal("itmMana", attributes.itmMana);
	readInt("baseMagicToHit", combat.baseMagicToHit);
	readInt("baseMeleeToHit", combat.baseMeleeToHit);
	readInt("baseRangedToHit", combat.baseRangedToHit);
}

void LoadClassSpriteData(std::string_view classPath, PlayerSpriteData &spriteData)
{
	const std::string filename = StrCat("txtdata\\classes\\", classPath, "\\sprites.tsv");
	tl::expected<DataFile, DataFile::Error> dataFileResult = DataFile::load(filename);
	if (!dataFileResult.has_value()) {
		DataFile::reportFatalError(dataFileResult.error(), filename);
	}
	DataFile &dataFile = dataFileResult.value();

	if (tl::expected<void, DataFile::Error> result = dataFile.skipHeader();
	    !result.has_value()) {
		DataFile::reportFatalError(result.error(), filename);
	}

	auto recordIt = dataFile.begin();
	const auto recordEnd = dataFile.end();

	const auto getValueField = [&](std::string_view expectedKey) {
		if (recordIt == recordEnd) {
			app_fatal(fmt::format("Missing field {} in {}", expectedKey, filename));
		}
		DataFileRecord record = *recordIt;
		FieldIterator fieldIt = record.begin();
		const FieldIterator endField = record.end();

		const std::string_view key = (*fieldIt).value();
		if (key != expectedKey) {
			app_fatal(fmt::format("Unexpected field in {}: got {}, expected {}", filename, key, expectedKey));
		}

		++fieldIt;
		if (fieldIt == endField) {
			DataFile::reportFatalError(DataFile::Error::NotEnoughColumns, filename);
		}
		return *fieldIt;
	};

	const auto valueReader = [&](auto &&readFn) {
		return [&](std::string_view expectedKey, auto &outValue) {
			DataFileField valueField = getValueField(expectedKey);
			if (const tl::expected<void, devilution::DataFileField::Error> result = readFn(valueField, outValue);
			    !result.has_value()) {
				DataFile::reportFatalFieldError(result.error(), filename, "Value", valueField);
			}
			++recordIt;
		};
	};

	const auto readString = valueReader([](DataFileField &valueField, auto &outValue) {
		outValue = valueField.value();
		return tl::expected<void, devilution::DataFileField::Error> {};
	});

	const auto readInt = valueReader([](DataFileField &valueField, auto &outValue) {
		return valueField.parseInt(outValue);
	});

	readString("classPath", spriteData.classPath);
	readInt("stand", spriteData.stand);
	readInt("walk", spriteData.walk);
	readInt("attack", spriteData.attack);
	readInt("bow", spriteData.bow);
	readInt("swHit", spriteData.swHit);
	readInt("block", spriteData.block);
	readInt("lightning", spriteData.lightning);
	readInt("fire", spriteData.fire);
	readInt("magic", spriteData.magic);
	readInt("death", spriteData.death);
}

/** Contains the data related to each player class. */
std::vector<PlayerData> PlayersData;

std::vector<ClassAttributes> ClassAttributesPerClass;

std::vector<PlayerCombatData> PlayersCombatData;

/** Contains the data related to each player class. */
std::vector<PlayerSpriteData> PlayersSpriteData;

} // namespace

void LoadClassDatFromFile(DataFile &dataFile, const std::string_view filename)
{
	dataFile.skipHeaderOrDie(filename);

	PlayersData.reserve(PlayersData.size() + dataFile.numRecords());

	for (DataFileRecord record : dataFile) {
		if (PlayersData.size() >= static_cast<size_t>(HeroClass::NUM_MAX_CLASSES)) {
			DisplayFatalErrorAndExit(_("Loading Class Data Failed"), fmt::format(fmt::runtime(_("Could not add a class, since the maximum class number of {} has already been reached.")), static_cast<size_t>(HeroClass::NUM_MAX_CLASSES)));
		}

		RecordReader reader { record, filename };

		PlayerData &playerData = PlayersData.emplace_back();

		reader.readString("className", playerData.className);
		reader.readString("folderName", playerData.folderName);
	}
}

namespace {

void LoadClassDat()
{
	const std::string_view filename = "txtdata\\classes\\classdat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	PlayersData.clear();
	LoadClassDatFromFile(dataFile, filename);

	PlayersData.shrink_to_fit();
}

void LoadClassesAttributes()
{
	ClassAttributesPerClass.clear();
	ClassAttributesPerClass.reserve(PlayersData.size());
	PlayersCombatData.clear();
	PlayersCombatData.reserve(PlayersData.size());
	PlayersSpriteData.clear();
	PlayersSpriteData.reserve(PlayersData.size());

	for (const PlayerData &playerData : PlayersData) {
		LoadClassData(playerData.folderName, ClassAttributesPerClass.emplace_back(), PlayersCombatData.emplace_back());
		LoadClassSpriteData(playerData.folderName, PlayersSpriteData.emplace_back());
	}
}

const std::array<PlayerStartingLoadoutData, enum_size<HeroClass>::value> PlayersStartingLoadoutData { {
	// clang-format off
// HeroClass                 skill,                  spell,             spellLevel,     items[0].diablo,       items[0].hellfire, items[1].diablo,  items[1].hellfire, items[2].diablo, items[2].hellfire, items[3].diablo, items[3].hellfire, items[4].diablo, items[4].hellfire, gold,
/* HeroClass::Warrior   */ { SpellID::ItemRepair,    SpellID::Null,              0, { { { IDI_WARRIOR,         IDI_WARRIOR,    }, { IDI_WARRSHLD,   IDI_WARRSHLD,   }, { IDI_WARRCLUB,  IDI_WARRCLUB,   }, { IDI_HEAL,    IDI_HEAL,  }, { IDI_HEAL,      IDI_HEAL, }, }, },  100, },
/* HeroClass::Rogue     */ { SpellID::TrapDisarm,    SpellID::Null,              0, { { { IDI_ROGUE,           IDI_ROGUE,      }, { IDI_HEAL,       IDI_HEAL,       }, { IDI_HEAL,      IDI_HEAL,       }, { IDI_NONE,    IDI_NONE,  }, { IDI_NONE,      IDI_NONE, }, }, },  100, },
/* HeroClass::Sorcerer  */ { SpellID::StaffRecharge, SpellID::Firebolt,          2, { { { IDI_SORCERER_DIABLO, IDI_SORCERER,   }, { IDI_MANA,       IDI_HEAL,       }, { IDI_MANA,      IDI_HEAL,       }, { IDI_NONE,    IDI_NONE,  }, { IDI_NONE,      IDI_NONE, }, }, },  100, },
/* HeroClass::Monk      */ { SpellID::Search,        SpellID::Null,              0, { { { IDI_SHORTSTAFF,      IDI_SHORTSTAFF, }, { IDI_HEAL,       IDI_HEAL,       }, { IDI_HEAL,      IDI_HEAL,       }, { IDI_NONE,    IDI_NONE,  }, { IDI_NONE,      IDI_NONE, }, }, },  100, },
/* HeroClass::Bard      */ { SpellID::Identify,      SpellID::Null,              0, { { { IDI_BARDSWORD,       IDI_BARDSWORD,  }, { IDI_BARDDAGGER, IDI_BARDDAGGER, }, { IDI_HEAL,      IDI_HEAL,       }, { IDI_HEAL,    IDI_HEAL,  }, { IDI_NONE,      IDI_NONE, }, }, },  100, },
/* HeroClass::Barbarian */ { SpellID::Rage,          SpellID::Null,              0, { { { IDI_BARBARIAN,       IDI_BARBARIAN,  }, { IDI_WARRSHLD,   IDI_WARRSHLD,   }, { IDI_HEAL,      IDI_HEAL,       }, { IDI_HEAL,    IDI_HEAL,  }, { IDI_NONE,      IDI_NONE, }, }, },  100, }
	// clang-format on
} };

} // namespace

const ClassAttributes &GetClassAttributes(HeroClass playerClass)
{
	return ClassAttributesPerClass[static_cast<size_t>(playerClass)];
}

void LoadPlayerDataFiles()
{
	ReloadExperienceData();
	LoadClassDat();
	LoadClassesAttributes();
}

uint32_t GetNextExperienceThresholdForLevel(unsigned level)
{
	return ExperienceData.getThresholdForLevel(level);
}

uint8_t GetMaximumCharacterLevel()
{
	return ExperienceData.getMaxLevel();
}

const PlayerData &GetPlayerDataForClass(HeroClass playerClass)
{
	const size_t playerClassIndex = static_cast<size_t>(playerClass);
	assert(playerClassIndex < PlayersData.size());
	return PlayersData[playerClassIndex];
}

const SfxID herosounds[enum_size<HeroClass>::value][enum_size<HeroSpeech>::value] = {
	// clang-format off
	{ SfxID::Warrior1,  SfxID::Warrior2,  SfxID::Warrior3,  SfxID::Warrior4,  SfxID::Warrior5,  SfxID::Warrior6,  SfxID::Warrior7,  SfxID::Warrior8,  SfxID::Warrior9,  SfxID::Warrior10,  SfxID::Warrior11,  SfxID::Warrior12,  SfxID::Warrior13,  SfxID::Warrior14,  SfxID::Warrior15,  SfxID::Warrior16,  SfxID::Warrior17,  SfxID::Warrior18,  SfxID::Warrior19,  SfxID::Warrior20,  SfxID::Warrior21,  SfxID::Warrior22,  SfxID::Warrior23,  SfxID::Warrior24,  SfxID::Warrior25,  SfxID::Warrior26,  SfxID::Warrior27,  SfxID::Warrior28,  SfxID::Warrior29,  SfxID::Warrior30,  SfxID::Warrior31,  SfxID::Warrior32,  SfxID::Warrior33,  SfxID::Warrior34,  SfxID::Warrior35,  SfxID::Warrior36,  SfxID::Warrior37,  SfxID::Warrior38,  SfxID::Warrior39,  SfxID::Warrior40,  SfxID::Warrior41,  SfxID::Warrior42,  SfxID::Warrior43,  SfxID::Warrior44,  SfxID::Warrior45,  SfxID::Warrior46,  SfxID::Warrior47,  SfxID::Warrior48,  SfxID::Warrior49,  SfxID::Warrior50,  SfxID::Warrior51,  SfxID::Warrior52,  SfxID::Warrior53,  SfxID::Warrior54,  SfxID::Warrior55,  SfxID::Warrior56,  SfxID::Warrior57,  SfxID::Warrior58,  SfxID::Warrior59,  SfxID::Warrior60,  SfxID::Warrior61,  SfxID::Warrior62,  SfxID::Warrior63,  SfxID::Warrior64,  SfxID::Warrior65,  SfxID::Warrior66,  SfxID::Warrior67,  SfxID::Warrior68,  SfxID::Warrior69,  SfxID::Warrior70,  SfxID::Warrior71,  SfxID::Warrior72,  SfxID::Warrior73,  SfxID::Warrior74,  SfxID::Warrior75,  SfxID::Warrior76,  SfxID::Warrior77,  SfxID::Warrior78,  SfxID::Warrior79,  SfxID::Warrior80,  SfxID::Warrior81,  SfxID::Warrior82,  SfxID::Warrior83,  SfxID::Warrior84,  SfxID::Warrior85,  SfxID::Warrior86,  SfxID::Warrior87,  SfxID::Warrior88,  SfxID::Warrior89,  SfxID::Warrior90,  SfxID::Warrior91,  SfxID::Warrior92,  SfxID::Warrior93,  SfxID::Warrior94,  SfxID::Warrior95,  SfxID::Warrior96b,  SfxID::Warrior97,  SfxID::Warrior98,  SfxID::Warrior99,  SfxID::Warrior100,  SfxID::Warrior101,  SfxID::Warrior102,  SfxID::WarriorDeath  },
	{ SfxID::Rogue1,    SfxID::Rogue2,    SfxID::Rogue3,    SfxID::Rogue4,    SfxID::Rogue5,    SfxID::Rogue6,    SfxID::Rogue7,    SfxID::Rogue8,    SfxID::Rogue9,    SfxID::Rogue10,    SfxID::Rogue11,    SfxID::Rogue12,    SfxID::Rogue13,    SfxID::Rogue14,    SfxID::Rogue15,    SfxID::Rogue16,    SfxID::Rogue17,    SfxID::Rogue18,    SfxID::Rogue19,    SfxID::Rogue20,    SfxID::Rogue21,    SfxID::Rogue22,    SfxID::Rogue23,    SfxID::Rogue24,    SfxID::Rogue25,    SfxID::Rogue26,    SfxID::Rogue27,    SfxID::Rogue28,    SfxID::Rogue29,    SfxID::Rogue30,    SfxID::Rogue31,    SfxID::Rogue32,    SfxID::Rogue33,    SfxID::Rogue34,    SfxID::Rogue35,    SfxID::Rogue36,    SfxID::Rogue37,    SfxID::Rogue38,    SfxID::Rogue39,    SfxID::Rogue40,    SfxID::Rogue41,    SfxID::Rogue42,    SfxID::Rogue43,    SfxID::Rogue44,    SfxID::Rogue45,    SfxID::Rogue46,    SfxID::Rogue47,    SfxID::Rogue48,    SfxID::Rogue49,    SfxID::Rogue50,    SfxID::Rogue51,    SfxID::Rogue52,    SfxID::Rogue53,    SfxID::Rogue54,    SfxID::Rogue55,    SfxID::Rogue56,    SfxID::Rogue57,    SfxID::Rogue58,    SfxID::Rogue59,    SfxID::Rogue60,    SfxID::Rogue61,    SfxID::Rogue62,    SfxID::Rogue63,    SfxID::Rogue64,    SfxID::Rogue65,    SfxID::Rogue66,    SfxID::Rogue67,    SfxID::Rogue68,    SfxID::Rogue69,    SfxID::Rogue70,    SfxID::Rogue71,    SfxID::Rogue72,    SfxID::Rogue73,    SfxID::Rogue74,    SfxID::Rogue75,    SfxID::Rogue76,    SfxID::Rogue77,    SfxID::Rogue78,    SfxID::Rogue79,    SfxID::Rogue80,    SfxID::Rogue81,    SfxID::Rogue82,    SfxID::Rogue83,    SfxID::Rogue84,    SfxID::Rogue85,    SfxID::Rogue86,    SfxID::Rogue87,    SfxID::Rogue88,    SfxID::Rogue89,    SfxID::Rogue90,    SfxID::Rogue91,    SfxID::Rogue92,    SfxID::Rogue93,    SfxID::Rogue94,    SfxID::Rogue95,    SfxID::Rogue96,     SfxID::Rogue97,    SfxID::Rogue98,    SfxID::Rogue99,    SfxID::Rogue100,    SfxID::Rogue101,    SfxID::Rogue102,    SfxID::Rogue71       },
	{ SfxID::Sorceror1, SfxID::Sorceror2, SfxID::Sorceror3, SfxID::Sorceror4, SfxID::Sorceror5, SfxID::Sorceror6, SfxID::Sorceror7, SfxID::Sorceror8, SfxID::Sorceror9, SfxID::Sorceror10, SfxID::Sorceror11, SfxID::Sorceror12, SfxID::Sorceror13, SfxID::Sorceror14, SfxID::Sorceror15, SfxID::Sorceror16, SfxID::Sorceror17, SfxID::Sorceror18, SfxID::Sorceror19, SfxID::Sorceror20, SfxID::Sorceror21, SfxID::Sorceror22, SfxID::Sorceror23, SfxID::Sorceror24, SfxID::Sorceror25, SfxID::Sorceror26, SfxID::Sorceror27, SfxID::Sorceror28, SfxID::Sorceror29, SfxID::Sorceror30, SfxID::Sorceror31, SfxID::Sorceror32, SfxID::Sorceror33, SfxID::Sorceror34, SfxID::Sorceror35, SfxID::Sorceror36, SfxID::Sorceror37, SfxID::Sorceror38, SfxID::Sorceror39, SfxID::Sorceror40, SfxID::Sorceror41, SfxID::Sorceror42, SfxID::Sorceror43, SfxID::Sorceror44, SfxID::Sorceror45, SfxID::Sorceror46, SfxID::Sorceror47, SfxID::Sorceror48, SfxID::Sorceror49, SfxID::Sorceror50, SfxID::Sorceror51, SfxID::Sorceror52, SfxID::Sorceror53, SfxID::Sorceror54, SfxID::Sorceror55, SfxID::Sorceror56, SfxID::Sorceror57, SfxID::Sorceror58, SfxID::Sorceror59, SfxID::Sorceror60, SfxID::Sorceror61, SfxID::Sorceror62, SfxID::Sorceror63, SfxID::Sorceror64, SfxID::Sorceror65, SfxID::Sorceror66, SfxID::Sorceror67, SfxID::Sorceror68, SfxID::Sorceror69, SfxID::Sorceror70, SfxID::Sorceror71, SfxID::Sorceror72, SfxID::Sorceror73, SfxID::Sorceror74, SfxID::Sorceror75, SfxID::Sorceror76, SfxID::Sorceror77, SfxID::Sorceror78, SfxID::Sorceror79, SfxID::Sorceror80, SfxID::Sorceror81, SfxID::Sorceror82, SfxID::Sorceror83, SfxID::Sorceror84, SfxID::Sorceror85, SfxID::Sorceror86, SfxID::Sorceror87, SfxID::Sorceror88, SfxID::Sorceror89, SfxID::Sorceror90, SfxID::Sorceror91, SfxID::Sorceror92, SfxID::Sorceror93, SfxID::Sorceror94, SfxID::Sorceror95, SfxID::Sorceror96,  SfxID::Sorceror97, SfxID::Sorceror98, SfxID::Sorceror99, SfxID::Sorceror100, SfxID::Sorceror101, SfxID::Sorceror102, SfxID::Sorceror71    },
	{ SfxID::Monk1,     SfxID::None,      SfxID::None,      SfxID::None,      SfxID::None,      SfxID::None,      SfxID::None,      SfxID::Monk8,     SfxID::Monk9,     SfxID::Monk10,     SfxID::Monk11,     SfxID::Monk12,     SfxID::Monk13,     SfxID::Monk14,     SfxID::Monk15,     SfxID::Monk16,     SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::Monk24,     SfxID::None,       SfxID::None,       SfxID::Monk27,     SfxID::None,       SfxID::Monk29,     SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::Monk34,     SfxID::Monk35,     SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::Monk43,     SfxID::None,       SfxID::None,       SfxID::Monk46,     SfxID::None,       SfxID::None,       SfxID::Monk49,     SfxID::Monk50,     SfxID::None,       SfxID::Monk52,     SfxID::None,       SfxID::Monk54,     SfxID::Monk55,     SfxID::Monk56,     SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::Monk61,     SfxID::Monk62,     SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::Monk68,     SfxID::Monk69,     SfxID::Monk70,     SfxID::Monk71,     SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::None,       SfxID::Monk79,     SfxID::Monk80,     SfxID::None,       SfxID::Monk82,     SfxID::Monk83,     SfxID::None,       SfxID::None,       SfxID::None,       SfxID::Monk87,     SfxID::Monk88,     SfxID::Monk89,     SfxID::None,       SfxID::Monk91,     SfxID::Monk92,     SfxID::None,       SfxID::Monk94,     SfxID::Monk95,     SfxID::Monk96,      SfxID::Monk97,     SfxID::Monk98,     SfxID::Monk99,     SfxID::None,        SfxID::None,        SfxID::None,        SfxID::Monk71        },
	{ SfxID::Rogue1,    SfxID::Rogue2,    SfxID::Rogue3,    SfxID::Rogue4,    SfxID::Rogue5,    SfxID::Rogue6,    SfxID::Rogue7,    SfxID::Rogue8,    SfxID::Rogue9,    SfxID::Rogue10,    SfxID::Rogue11,    SfxID::Rogue12,    SfxID::Rogue13,    SfxID::Rogue14,    SfxID::Rogue15,    SfxID::Rogue16,    SfxID::Rogue17,    SfxID::Rogue18,    SfxID::Rogue19,    SfxID::Rogue20,    SfxID::Rogue21,    SfxID::Rogue22,    SfxID::Rogue23,    SfxID::Rogue24,    SfxID::Rogue25,    SfxID::Rogue26,    SfxID::Rogue27,    SfxID::Rogue28,    SfxID::Rogue29,    SfxID::Rogue30,    SfxID::Rogue31,    SfxID::Rogue32,    SfxID::Rogue33,    SfxID::Rogue34,    SfxID::Rogue35,    SfxID::Rogue36,    SfxID::Rogue37,    SfxID::Rogue38,    SfxID::Rogue39,    SfxID::Rogue40,    SfxID::Rogue41,    SfxID::Rogue42,    SfxID::Rogue43,    SfxID::Rogue44,    SfxID::Rogue45,    SfxID::Rogue46,    SfxID::Rogue47,    SfxID::Rogue48,    SfxID::Rogue49,    SfxID::Rogue50,    SfxID::Rogue51,    SfxID::Rogue52,    SfxID::Rogue53,    SfxID::Rogue54,    SfxID::Rogue55,    SfxID::Rogue56,    SfxID::Rogue57,    SfxID::Rogue58,    SfxID::Rogue59,    SfxID::Rogue60,    SfxID::Rogue61,    SfxID::Rogue62,    SfxID::Rogue63,    SfxID::Rogue64,    SfxID::Rogue65,    SfxID::Rogue66,    SfxID::Rogue67,    SfxID::Rogue68,    SfxID::Rogue69,    SfxID::Rogue70,    SfxID::Rogue71,    SfxID::Rogue72,    SfxID::Rogue73,    SfxID::Rogue74,    SfxID::Rogue75,    SfxID::Rogue76,    SfxID::Rogue77,    SfxID::Rogue78,    SfxID::Rogue79,    SfxID::Rogue80,    SfxID::Rogue81,    SfxID::Rogue82,    SfxID::Rogue83,    SfxID::Rogue84,    SfxID::Rogue85,    SfxID::Rogue86,    SfxID::Rogue87,    SfxID::Rogue88,    SfxID::Rogue89,    SfxID::Rogue90,    SfxID::Rogue91,    SfxID::Rogue92,    SfxID::Rogue93,    SfxID::Rogue94,    SfxID::Rogue95,    SfxID::Rogue96,     SfxID::Rogue97,    SfxID::Rogue98,    SfxID::Rogue99,    SfxID::Rogue100,    SfxID::Rogue101,    SfxID::Rogue102,    SfxID::Rogue71       },
	{ SfxID::Warrior1,  SfxID::Warrior2,  SfxID::Warrior3,  SfxID::Warrior4,  SfxID::Warrior5,  SfxID::Warrior6,  SfxID::Warrior7,  SfxID::Warrior8,  SfxID::Warrior9,  SfxID::Warrior10,  SfxID::Warrior11,  SfxID::Warrior12,  SfxID::Warrior13,  SfxID::Warrior14,  SfxID::Warrior15,  SfxID::Warrior16,  SfxID::Warrior17,  SfxID::Warrior18,  SfxID::Warrior19,  SfxID::Warrior20,  SfxID::Warrior21,  SfxID::Warrior22,  SfxID::Warrior23,  SfxID::Warrior24,  SfxID::Warrior25,  SfxID::Warrior26,  SfxID::Warrior27,  SfxID::Warrior28,  SfxID::Warrior29,  SfxID::Warrior30,  SfxID::Warrior31,  SfxID::Warrior32,  SfxID::Warrior33,  SfxID::Warrior34,  SfxID::Warrior35,  SfxID::Warrior36,  SfxID::Warrior37,  SfxID::Warrior38,  SfxID::Warrior39,  SfxID::Warrior40,  SfxID::Warrior41,  SfxID::Warrior42,  SfxID::Warrior43,  SfxID::Warrior44,  SfxID::Warrior45,  SfxID::Warrior46,  SfxID::Warrior47,  SfxID::Warrior48,  SfxID::Warrior49,  SfxID::Warrior50,  SfxID::Warrior51,  SfxID::Warrior52,  SfxID::Warrior53,  SfxID::Warrior54,  SfxID::Warrior55,  SfxID::Warrior56,  SfxID::Warrior57,  SfxID::Warrior58,  SfxID::Warrior59,  SfxID::Warrior60,  SfxID::Warrior61,  SfxID::Warrior62,  SfxID::Warrior63,  SfxID::Warrior64,  SfxID::Warrior65,  SfxID::Warrior66,  SfxID::Warrior67,  SfxID::Warrior68,  SfxID::Warrior69,  SfxID::Warrior70,  SfxID::Warrior71,  SfxID::Warrior72,  SfxID::Warrior73,  SfxID::Warrior74,  SfxID::Warrior75,  SfxID::Warrior76,  SfxID::Warrior77,  SfxID::Warrior78,  SfxID::Warrior79,  SfxID::Warrior80,  SfxID::Warrior81,  SfxID::Warrior82,  SfxID::Warrior83,  SfxID::Warrior84,  SfxID::Warrior85,  SfxID::Warrior86,  SfxID::Warrior87,  SfxID::Warrior88,  SfxID::Warrior89,  SfxID::Warrior90,  SfxID::Warrior91,  SfxID::Warrior92,  SfxID::Warrior93,  SfxID::Warrior94,  SfxID::Warrior95,  SfxID::Warrior96b,  SfxID::Warrior97,  SfxID::Warrior98,  SfxID::Warrior99,  SfxID::Warrior100,  SfxID::Warrior101,  SfxID::Warrior102,  SfxID::Warrior71     },
	// clang-format on
};

const PlayerCombatData &GetPlayerCombatDataForClass(HeroClass pClass)
{
	const size_t playerClassIndex = static_cast<size_t>(pClass);
	assert(playerClassIndex < PlayersCombatData.size());
	return PlayersCombatData[playerClassIndex];
}

const PlayerStartingLoadoutData &GetPlayerStartingLoadoutForClass(HeroClass pClass)
{
	const size_t playerClassIndex = static_cast<size_t>(pClass);
	assert(playerClassIndex < PlayersStartingLoadoutData.size());
	return PlayersStartingLoadoutData[playerClassIndex];
}

const PlayerSpriteData &GetPlayerSpriteDataForClass(HeroClass pClass)
{
	const size_t playerClassIndex = static_cast<size_t>(pClass);
	assert(playerClassIndex < PlayersSpriteData.size());
	return PlayersSpriteData[playerClassIndex];
}

const PlayerAnimData PlayersAnimData[] = {
	// clang-format off
// HeroClass                  unarmedFrames,  unarmedActionFrame,  unarmedShieldFrames,  unarmedShieldActionFrame,  swordFrames,  swordActionFrame,  swordShieldFrames,  swordShieldActionFrame,  bowFrames,  bowActionFrame,  axeFrames,  axeActionFrame,  maceFrames,  maceActionFrame,  maceShieldFrames,  maceShieldActionFrame,  staffFrames,  staffActionFrame,  idleFrames,  walkingFrames,  blockingFrames,  deathFrames,  castingFrames,  recoveryFrames,  townIdleFrames,  townWalkingFrames,  castingActionFrame
/* HeroClass::Warrior */   {             16,                   9,                   16,                         9,           16,                 9,                 16,                       9,         16,              11,         20,              10,          16,                9,                16,                      9,           16,                11,          10,              8,               2,           20,             20,               6,              20,                  8,                  14 },
/* HeroClass::Rogue */     {             18,                  10,                   18,                        10,           18,                10,                 18,                      10,         12,               7,         22,              13,          18,               10,                18,                     10,           16,                11,           8,              8,               4,           20,             16,               7,              20,                  8,                  12 },
/* HeroClass::Sorcerer */  {             20,                  12,                   16,                         9,           16,                12,                 16,                      12,         20,              16,         24,              16,          16,               12,                16,                     12,           16,                12,           8,              8,               6,           20,             12,               8,              20,                  8,                   8 },
/* HeroClass::Monk */      {             12,                   7,                   12,                         7,           16,                12,                 16,                      12,         20,              14,         23,              14,          16,               12,                16,                     12,           13,                 8,           8,              8,               3,           20,             18,               6,              20,                  8,                  13 },
/* HeroClass::Bard */      {             18,                  10,                   18,                        10,           18,                10,                 18,                      10,         12,              11,         22,              13,          18,               10,                18,                     10,           16,                11,           8,              8,               4,           20,             16,               7,              20,                  8,                  12 },
/* HeroClass::Barbarian */ {             16,                   9,                   16,                         9,           16,                 9,                 16,                       9,         16,              11,         20,               8,          16,                8,                16,                      8,           16,                11,          10,              8,               2,           20,             20,               6,              20,                  8,                  14 },
	// clang-format on
};

} // namespace devilution
