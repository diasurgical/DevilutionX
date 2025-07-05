#pragma once

#include <map>
#include <vector>

#include "Item.h"
#include "Player.h"
#include "Towner.h"
#include "Trigger.h"

namespace DAPI {
enum struct StoreOption {
	TALK,
	IDENTIFYANITEM,
	EXIT,
	HEAL,
	BUYITEMS,
	WIRTPEEK,
	BUYBASIC,
	BUYPREMIUM,
	SELL,
	REPAIR,
	RECHARGE,
	BACK
};

struct GameData {
	bool menuOpen;
	int pcurs;
	bool chrflag;
	bool invflag;
	bool qtextflag;
	int currlevel;
	size_t lastLogSize;

	std::map<int, PlayerData> playerList;
	std::vector<ItemData> itemList;
	std::vector<int> groundItems;
	std::map<int, TownerData> townerList;
	std::vector<StoreOption> storeList;
	std::vector<int> storeItems;
	std::vector<TriggerData> triggerList;
};
} // namespace DAPI
