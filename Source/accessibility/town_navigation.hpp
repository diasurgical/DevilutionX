#pragma once

namespace devilution {

bool IsTownNpcActionAllowed();
void SpeakSelectedTownNpc();

void SelectNextTownNpcKeyPressed();
void SelectPreviousTownNpcKeyPressed();
void GoToSelectedTownNpcKeyPressed();
void ListTownNpcsKeyPressed();
void UpdateAutoWalkTownNpc();
void CancelTownNpcAutoWalk();

} // namespace devilution
