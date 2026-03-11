/**
 * @file loadsave.h
 *
 * Interface of save game functionality.
 */
#pragma once

#include <cstdint>

#include <expected.hpp>

#include "pfile.h"
#include "player.h"
#include "utils/attributes.h"

namespace devilution {

extern DVL_API_FOR_TEST bool gbIsHellfireSaveGame;
extern DVL_API_FOR_TEST uint8_t giNumberOfLevels;

enum class SaveKind : uint8_t {
	Manual,
	Auto,
};

enum class SaveResult : uint8_t {
	Success,
	FailedButPreviousSavePreserved,
	FailedNoValidSave,
};

void RemoveInvalidItem(Item &pItem);
_item_indexes RemapItemIdxFromDiablo(_item_indexes i);
_item_indexes RemapItemIdxToDiablo(_item_indexes i);
_item_indexes RemapItemIdxFromSpawn(_item_indexes i);
_item_indexes RemapItemIdxToSpawn(_item_indexes i);
bool IsHeaderValid(uint32_t magicNumber);
void LoadHotkeys();
void LoadHeroItems(Player &player);
/**
 * @brief Remove invalid inventory items from the inventory grid
 * @param player The player to remove invalid items from
 */
void RemoveEmptyInventory(Player &player);

/**
 * @brief Load game state
 * @param firstflag Can be set to false if we are simply reloading the current game
 */
tl::expected<void, std::string> LoadGame(bool firstflag);
void SaveHotkeys(SaveWriter &saveWriter, const Player &player);
void SaveHeroItems(SaveWriter &saveWriter, Player &player);
void SaveGameData(SaveWriter &saveWriter);
SaveResult SaveGame(SaveKind kind);
void SaveLevel(SaveWriter &saveWriter);
tl::expected<void, std::string> LoadLevel();
tl::expected<void, std::string> ConvertLevels(SaveWriter &saveWriter);
void LoadStash();
void SaveStash(SaveWriter &stashWriter);

} // namespace devilution
