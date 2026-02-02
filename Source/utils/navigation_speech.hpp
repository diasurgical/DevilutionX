/**
 * @file utils/navigation_speech.hpp
 *
 * Navigation speech: exit/stairs/portal/unexplored speech and keyboard walk keys.
 */
#pragma once

namespace devilution {

void SpeakNearestExitKeyPressed();
void SpeakNearestTownPortalInTownKeyPressed();
void SpeakNearestStairsDownKeyPressed();
void SpeakNearestStairsUpKeyPressed();
void KeyboardWalkNorthKeyPressed();
void KeyboardWalkSouthKeyPressed();
void KeyboardWalkEastKeyPressed();
void KeyboardWalkWestKeyPressed();
void SpeakNearestUnexploredTileKeyPressed();
bool IsKeyboardWalkAllowed();

} // namespace devilution
