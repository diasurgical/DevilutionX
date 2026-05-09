/**
 * @file quests/validation.hpp
 *
 * Interface of functions for validation of quest data.
 */
#pragma once

#include <cstdint>

#include "quests.h"
#include "tables/objdat.h"

namespace devilution {

bool IsQuestDeltaValid(quest_id qidx, quest_state qstate, uint8_t qlog, int16_t qmsg);

} // namespace devilution
