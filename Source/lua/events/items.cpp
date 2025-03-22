#include "lua/events/items.hpp"

#include <sol/sol.hpp>

#include "items.h"
#include "lua/lua.hpp"
#include "lua/metadoc.hpp"
#include "lua/usertypes/items.hpp"

namespace devilution {

namespace {

bool CallLuaSkipIdxHelper(const std::string_view eventName, _item_indexes idx, bool defaultCondition)
{
	sol::table events = GetLuaEvents();
	sol::object eventObj = events[eventName];
	if (!eventObj.valid() || !eventObj.is<sol::table>())
		return defaultCondition;

	sol::table eventTable = eventObj;
	sol::object triggerObj = eventTable["trigger"];
	if (!triggerObj.valid() || !triggerObj.is<sol::function>())
		return defaultCondition;

	sol::function trigger = triggerObj;
	sol::protected_function_result result = trigger(static_cast<int>(idx));
	if (!result.valid())
		return defaultCondition;

	return result.get<bool>();
}

} // namespace

bool CallLuaDungeonSpawnCondition(Item &item)
{
	bool defaultCondition = false;
	sol::table events = GetLuaEvents();
	sol::object eventObj = events["NoSpawnCondition"];

	if (!eventObj.valid() || !eventObj.is<sol::table>())
		return defaultCondition;

	sol::table eventTable = eventObj;
	sol::object triggerObj = eventTable["trigger"];

	if (!triggerObj.valid() || !triggerObj.is<sol::function>())
		return defaultCondition;

	sol::function trigger = triggerObj;
	sol::object itemObject = CreateItemObject(item);
	sol::protected_function_result result = trigger(itemObject);

	if (!result.valid())
		return defaultCondition;

	return result.get<bool>();
}

bool CallLuaRewardSkipItem(Item &item, uint8_t iCurs)
{
	bool defaultCondition = item._iCurs != iCurs;
	sol::table events = GetLuaEvents();
	sol::object eventObj = events["RewardSpawnCondition"];

	if (!eventObj.valid() || !eventObj.is<sol::table>())
		return defaultCondition;

	sol::table eventTable = eventObj;
	sol::object triggerObj = eventTable["trigger"];

	if (!triggerObj.valid() || !triggerObj.is<sol::function>())
		return defaultCondition;

	sol::function trigger = triggerObj;
	sol::object itemObject = CreateItemObject(item);
	sol::protected_function_result result = trigger(itemObject, iCurs);

	if (!result.valid())
		return defaultCondition;

	return result.get<bool>();
}

bool CallLuaVendorSkipItem(Item &item, int maxValue)
{
	bool defaultCondition = item._iIvalue > maxValue;
	sol::table events = GetLuaEvents();
	sol::object eventObj = events["VendorSpawnCondition"];

	if (!eventObj.valid() || !eventObj.is<sol::table>())
		return defaultCondition;

	sol::table vendorEvent = eventObj;
	sol::object triggerObj = vendorEvent["trigger"];

	if (!triggerObj.valid() || !triggerObj.is<sol::function>())
		return defaultCondition;

	sol::function trigger = triggerObj;
	sol::object itemObject = CreateItemObject(item);
	sol::protected_function_result result = trigger(itemObject, maxValue);

	if (!result.valid())
		return defaultCondition;

	return result.get<bool>();
}

bool CallLuaSmithPremiumHfSkipItem(Item &item, int maxValue, int strength, int magic, int dexterity, int ivalue)
{
	bool defaultCondition = item._iIvalue > maxValue || item._iMinStr > strength || item._iMinMag > magic || item._iMinDex > dexterity || item._iIvalue < ivalue;
	sol::table events = GetLuaEvents();
	sol::object eventObj = events["SmithPremiumHfSpawnCondition"];

	if (!eventObj.valid() || !eventObj.is<sol::table>())
		return defaultCondition;

	sol::table eventTable = eventObj;
	sol::object triggerObj = eventTable["trigger"];

	if (!triggerObj.valid() || !triggerObj.is<sol::function>())
		return defaultCondition;

	sol::function trigger = triggerObj;
	sol::object itemObject = CreateItemObject(item);
	sol::protected_function_result result = trigger(itemObject, maxValue, strength, magic, dexterity, ivalue);

	if (!result.valid())
		return defaultCondition;

	return result.get<bool>();
}

bool CallLuaBoyHfSkipItem(Item &item, int maxValue, int strength, int magic, int dexterity, int ivalue, int count, int maxCount)
{
	bool defaultCondition = item._iIvalue > maxValue || item._iMinStr > strength || item._iMinMag > magic || item._iMinDex > dexterity || item._iIvalue < ivalue || count < maxCount;
	sol::table events = GetLuaEvents();
	sol::object eventObj = events["BoyHfSpawnCondition"];

	if (!eventObj.valid() || !eventObj.is<sol::table>())
		return defaultCondition;

	sol::table eventTable = eventObj;
	sol::object triggerObj = eventTable["trigger"];

	if (!triggerObj.valid() || !triggerObj.is<sol::function>())
		return defaultCondition;

	sol::function trigger = triggerObj;
	sol::object itemObject = CreateItemObject(item);
	sol::protected_function_result result = trigger(itemObject, maxValue, strength, magic, dexterity, ivalue, count, maxCount);

	if (!result.valid())
		return defaultCondition;

	return result.get<bool>();
}

bool CallLuaUniqueMonsterSkipIdx(_item_indexes idx)
{
	return CallLuaSkipIdxHelper("UniqueMonsterSkipIdx", idx, false);
}

bool CallLuaMonsterSkipIdx(_item_indexes idx)
{
	return CallLuaSkipIdxHelper("MonsterSkipIdx", idx, false);
}

bool CallLuaDungeonRndItemSkipIdx(_item_indexes idx)
{
	return CallLuaSkipIdxHelper("DungeonRndItemSkipIdx", idx, false);
}

bool CallLuaDungeonTypeItemSkipIdx(_item_indexes idx)
{
	return CallLuaSkipIdxHelper("DungeonTypeItemSkipIdx", idx, false);
}

bool CallLuaDungeonSpellBookSkipIdx(_item_indexes idx)
{
	return CallLuaSkipIdxHelper("DungeonSpellBookSkipIdx", idx, false);
}

} // namespace devilution
