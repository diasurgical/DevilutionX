#pragma once

#include <stdint.h>

#include "itemdat.h"

namespace devilution {

struct Item;

bool CallLuaDungeonSpawnCondition(Item &item);
bool CallLuaRewardSkipItem(Item &item, uint8_t iCurs);
bool CallLuaVendorSkipItem(Item &item, int maxValue);
bool CallLuaSmithPremiumHfSkipItem(Item &item, int maxValue, int strength, int magic, int dexterity, int ivalue);
bool CallLuaBoyHfSkipItem(Item &item, int maxValue, int strength, int magic, int dexterity, int ivalue, int count, int maxCount);
bool CallLuaUniqueMonsterSkipIdx(_item_indexes idx);
bool CallLuaMonsterSkipIdx(_item_indexes idx);
bool CallLuaDungeonRndItemSkipIdx(_item_indexes idx);
bool CallLuaDungeonTypeItemSkipIdx(_item_indexes idx);
bool CallLuaDungeonSpellBookSkipIdx(_item_indexes idx);

} // namespace devilution
