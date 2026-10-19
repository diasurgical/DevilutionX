/**
 * @file loadsave.h
 *
 * Interface of save game functionality.
 */
#pragma once

#include <cstdint>
#ifdef BUILD_TESTING
#include <cstddef>
#include <span>
#include <vector>
#endif
#include <expected>

#include "pfile.h"
#include "player.h"
#include "utils/attributes.h"

namespace devilution {

#ifdef BUILD_TESTING
struct Monster;
DVL_API_FOR_TEST std::vector<std::byte> SaveMonsterForTest(SaveWriter &writer, Monster &monster);
DVL_API_FOR_TEST bool LoadMonsterForTest(std::span<const std::byte> bytes, Monster &monster, size_t &consumed);
#endif

extern DVL_API_FOR_TEST bool gbIsHellfireSaveGame;
extern DVL_API_FOR_TEST uint8_t giNumberOfLevels;

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
std::expected<void, std::string> LoadGame(bool firstflag);
void SaveHotkeys(SaveWriter &saveWriter, const Player &player);
void SaveHeroItems(SaveWriter &saveWriter, Player &player);
void SaveGameData(SaveWriter &saveWriter);
void SaveGame();
void SaveLevel(SaveWriter &saveWriter);
std::expected<void, std::string> LoadLevel();
std::expected<void, std::string> ConvertLevels(SaveWriter &saveWriter);
void LoadStash();
void SaveStash(SaveWriter &stashWriter);

} // namespace devilution
