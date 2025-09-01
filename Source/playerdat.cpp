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
#include <magic_enum/magic_enum_utility.hpp>

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

void LoadClassStartingLoadoutData(std::string_view classPath, PlayerStartingLoadoutData &startingLoadoutData)
{
	const std::string filename = StrCat("txtdata\\classes\\", classPath, "\\starting_loadout.tsv");
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

	const auto readSpellId = valueReader([](DataFileField &valueField, auto &outValue) -> tl::expected<void, devilution::DataFileField::Error> {
		const auto result = ParseSpellId(valueField.value());
		if (!result.has_value()) {
			return tl::make_unexpected(devilution::DataFileField::Error::InvalidValue);
		}

		outValue = result.value();
	});

	const auto readItemId = valueReader([](DataFileField &valueField, auto &outValue) -> tl::expected<void, devilution::DataFileField::Error> {
		const auto result = ParseItemId(valueField.value());
		if (!result.has_value()) {
			return tl::make_unexpected(devilution::DataFileField::Error::InvalidValue);
		}

		outValue = result.value();
	});

	readSpellId("skill", startingLoadoutData.skill);
	readSpellId("spell", startingLoadoutData.spell);
	readInt("spellLevel", startingLoadoutData.spellLevel);
	for (size_t i = 0; i < startingLoadoutData.items.size(); ++i) {
		readItemId(StrCat("item", i), startingLoadoutData.items[i]);
	}
	readInt("gold", startingLoadoutData.gold);
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

void LoadClassAnimData(std::string_view classPath, PlayerAnimData &animData)
{
	const std::string filename = StrCat("txtdata\\classes\\", classPath, "\\animations.tsv");
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

	readInt("unarmedFrames", animData.unarmedFrames);
	readInt("unarmedActionFrame", animData.unarmedActionFrame);
	readInt("unarmedShieldFrames", animData.unarmedShieldFrames);
	readInt("unarmedShieldActionFrame", animData.unarmedShieldActionFrame);
	readInt("swordFrames", animData.swordFrames);
	readInt("swordActionFrame", animData.swordActionFrame);
	readInt("swordShieldFrames", animData.swordShieldFrames);
	readInt("swordShieldActionFrame", animData.swordShieldActionFrame);
	readInt("bowFrames", animData.bowFrames);
	readInt("bowActionFrame", animData.bowActionFrame);
	readInt("axeFrames", animData.axeFrames);
	readInt("axeActionFrame", animData.axeActionFrame);
	readInt("maceFrames", animData.maceFrames);
	readInt("maceActionFrame", animData.maceActionFrame);
	readInt("maceShieldFrames", animData.maceShieldFrames);
	readInt("maceShieldActionFrame", animData.maceShieldActionFrame);
	readInt("staffFrames", animData.staffFrames);
	readInt("staffActionFrame", animData.staffActionFrame);
	readInt("idleFrames", animData.idleFrames);
	readInt("walkingFrames", animData.walkingFrames);
	readInt("blockingFrames", animData.blockingFrames);
	readInt("deathFrames", animData.deathFrames);
	readInt("castingFrames", animData.castingFrames);
	readInt("recoveryFrames", animData.recoveryFrames);
	readInt("townIdleFrames", animData.townIdleFrames);
	readInt("townWalkingFrames", animData.townWalkingFrames);
	readInt("castingActionFrame", animData.castingActionFrame);
}

void LoadClassSounds(std::string_view classPath, ankerl::unordered_dense::map<HeroSpeech, SfxID> &sounds)
{
	const std::string filename = StrCat("txtdata\\classes\\", classPath, "\\sounds.tsv");
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

	const auto readSfxId = valueReader([](DataFileField &valueField, auto &outValue) -> tl::expected<void, devilution::DataFileField::Error> {
		const auto result = ParseSfxId(valueField.value());
		if (!result.has_value()) {
			return tl::make_unexpected(devilution::DataFileField::Error::InvalidValue);
		}

		outValue = result.value();
	});

	magic_enum::enum_for_each<HeroSpeech>([&](const HeroSpeech speech) {
		readSfxId(magic_enum::enum_name(speech), sounds[speech]);
	});
}

/** Contains the data related to each player class. */
std::vector<PlayerData> PlayersData;

std::vector<ClassAttributes> ClassAttributesPerClass;

std::vector<PlayerCombatData> PlayersCombatData;

std::vector<PlayerStartingLoadoutData> PlayersStartingLoadoutData;

/** Contains the data related to each player class. */
std::vector<PlayerSpriteData> PlayersSpriteData;

std::vector<PlayerAnimData> PlayersAnimData;

std::vector<ankerl::unordered_dense::map<HeroSpeech, SfxID>> herosounds;

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
	PlayersStartingLoadoutData.clear();
	PlayersStartingLoadoutData.reserve(PlayersData.size());
	PlayersSpriteData.clear();
	PlayersSpriteData.reserve(PlayersData.size());
	PlayersAnimData.clear();
	PlayersAnimData.reserve(PlayersData.size());
	herosounds.clear();
	herosounds.reserve(PlayersData.size());

	for (const PlayerData &playerData : PlayersData) {
		LoadClassData(playerData.folderName, ClassAttributesPerClass.emplace_back(), PlayersCombatData.emplace_back());
		LoadClassStartingLoadoutData(playerData.folderName, PlayersStartingLoadoutData.emplace_back());
		LoadClassSpriteData(playerData.folderName, PlayersSpriteData.emplace_back());
		LoadClassAnimData(playerData.folderName, PlayersAnimData.emplace_back());
		LoadClassSounds(playerData.folderName, herosounds.emplace_back());
	}
}

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

SfxID GetHeroSound(HeroClass clazz, HeroSpeech speech)
{
	const size_t playerClassIndex = static_cast<size_t>(clazz);
	assert(playerClassIndex < herosounds.size());
	const auto findIt = herosounds[playerClassIndex].find(speech);
	if (findIt != herosounds[playerClassIndex].end()) {
		return findIt->second;
	}

	return SfxID::None;
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

const PlayerAnimData &GetPlayerAnimDataForClass(HeroClass pClass)
{
	const size_t playerClassIndex = static_cast<size_t>(pClass);
	assert(playerClassIndex < PlayersAnimData.size());
	return PlayersAnimData[playerClassIndex];
}

} // namespace devilution
