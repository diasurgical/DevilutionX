#pragma once

#include <cstdint>
#include <string_view>

namespace devilution {

struct Player;
struct Monster;

namespace lua {

void MonsterDataLoaded();
void UniqueMonsterDataLoaded();
void ItemDataLoaded();
void UniqueItemDataLoaded();

void StoreOpened(std::string_view name);

void OnMonsterTakeDamage(const Monster *monster, int damage, int damageType);

bool OnPlayerDeathDropEar(const Player *player);
bool OnPlayerDeathDropGold(const Player *player);
bool OnPlayerDeathDropItem(const Player *player);
void OnPlayerGainExperience(const Player *player, uint32_t exp);
void OnPlayerTakeDamage(const Player *player, int damage, int damageType);

void LoadModsComplete();
void GameDrawComplete();
void GameStart();

} // namespace lua

} // namespace devilution
