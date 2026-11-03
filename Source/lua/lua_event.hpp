#pragma once

#include <cstdint>
#include <string_view>

namespace devilution {

struct Item;
struct Player;
struct Monster;

namespace lua {

void MonsterDataLoaded();
void UniqueMonsterDataLoaded();
void ItemDataLoaded();
void UniqueItemDataLoaded();

void StoreOpened(std::string_view name);

void OnMonsterTakeDamage(const Monster *monster, int damage, int damageType);

void OnPlayerGainExperience(const Player *player, uint32_t exp);
void OnPlayerTakeDamage(const Player *player, int damage, int damageType);

void OnCustomItemRecreated(Item &item);
void OnItemUsed(const Player &player, int mid, int spellID);

void LoadModsComplete();
void GameDrawComplete();
void GameStart();

} // namespace lua

} // namespace devilution
