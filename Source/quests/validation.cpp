/**
 * @file quests/validation.cpp
 *
 * Implementation of functions for validation of quest data.
 */

#include "quests/validation.hpp"

#include <cstdint>

#include "objdat.h"
#include "quests.h"
#include "textdat.h"
#include "utils/is_of.hpp"

namespace devilution {

bool IsQuestDeltaValid(quest_id qidx, quest_state qstate, uint8_t qlog, int16_t qmsg)
{
	if (IsNoneOf(qlog, 0, 1))
		return false;

	if (qmsg < 0 || static_cast<size_t>(qmsg) >= Speeches.size())
		return false;

	switch (qstate) {
	case QUEST_NOTAVAIL:
	case QUEST_INIT:
	case QUEST_ACTIVE:
	case QUEST_DONE:
		return true;

	case QUEST_HIVE_TEASE1:
	case QUEST_HIVE_TEASE2:
	case QUEST_HIVE_ACTIVE:
		return qidx == Q_JERSEY;

	case QUEST_HIVE_DONE:
		return IsAnyOf(qidx, Q_FARMER, Q_JERSEY);

	default:
		return false;
	}
}

} // namespace devilution
