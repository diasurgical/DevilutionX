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
void PlayerDataLoaded();

void StoreOpened(std::string_view name);

void OnMonsterTakeDamage(const Monster *monster, int damage, int damageType);

void OnPlayerGainExperience(const Player *player, uint32_t exp);
void OnPlayerTakeDamage(const Player *player, int damage, int damageType);
void OnCalcPlayerResistances(const Player *player, int fire, int lightning, int magic);
void OnNewCharacter(const Player &player);
void OnCreatePlrItems(Player &player);

void LoadModsComplete();
void GameDrawComplete();
void GameStart();

} // namespace lua

} // namespace devilution
