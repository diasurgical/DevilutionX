#pragma once

#include <cstdint>
#include <string_view>

namespace devilution {

struct Player;
struct Monster;
struct Item;

namespace lua {

void MonsterDataLoaded();
void UniqueMonsterDataLoaded();
void ItemDataLoaded();
void UniqueItemDataLoaded();
void PlayerDataLoaded();

void StoreOpened(std::string_view name);

void OnMonsterTakeDamage(const Monster *monster, int damage, int damageType);

void OnPlayerGainExperience(const Player *player, uint32_t exp);
void OnPlayerTakeDamage(const Player *player, int damage, int damageType);

void LoadModsComplete();
void GameDrawComplete();
void GameStart();

/** @brief Query: whether an item may be placed in the open stash. Returns defaultValue unless a mod overrides it. */
bool OnItemAllowedInStash(const Item *item, bool defaultValue);

} // namespace lua

} // namespace devilution
