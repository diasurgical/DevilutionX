#pragma once

#include "engine/clx_sprite.hpp"
#include "engine/surface.hpp"
#include "utils/attributes.h"

namespace devilution {

extern DVL_API_FOR_TEST bool QuestLogIsOpen;
extern OptionalOwnedClxSpriteList pQLogCel;

void DrawQuestLog(const Surface &out);
void StartQuestlog();
void QuestlogUp();
void QuestlogDown();
void QuestlogEnter();
void QuestlogESC();

} // namespace devilution
