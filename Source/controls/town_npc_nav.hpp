/**
 * @file controls/town_npc_nav.hpp
 *
 * Town NPC navigation for accessibility.
 */
#pragma once

namespace devilution {

void SelectNextTownNpcKeyPressed();
void SelectPreviousTownNpcKeyPressed();
void GoToSelectedTownNpcKeyPressed();
void SpeakSelectedTownNpc();
void ListTownNpcsKeyPressed();
void UpdateAutoWalkTownNpc();
bool IsTownNpcActionAllowed();
void ResetAutoWalkTownNpc();

} // namespace devilution
