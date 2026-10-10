#pragma once

#include "../towners.h"

namespace DAPI {

struct TownerData {
	int ID;
	devilution::_talker_id _ttype;
	int _tx;
	int _ty;
	char _tName[32];
};
} // namespace DAPI
