#pragma once
#include "../levels/trigs.h"

namespace DAPI {
struct TriggerData {
	bool compare(devilution::TriggerStruct &other) { return (other._tlvl == lvl && other._tmsg == type && other.position.x == x && other.position.y == y); }

	int ID;
	int x;
	int y;
	int lvl;
	int type;
};
} // namespace DAPI
