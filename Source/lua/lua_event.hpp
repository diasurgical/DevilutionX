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

void LoadModsComplete();
void GameDrawComplete();
void GameStart();

// Generic mod net pipe: fired on receipt of a CMD_LUAMSG packet. `senderId` is the player id the
// message came from; `payload` is the opaque bytes the sender passed to system.netSend (binary-safe).
void NetMessage(int senderId, std::string_view payload);

} // namespace lua

} // namespace devilution
