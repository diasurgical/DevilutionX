/**
 * @file utils/navigation_speech.hpp
 *
 * Navigation speech: exit/stairs/portal/unexplored speech and keyboard walk keys.
 */
#pragma once

#include <string>
#include <vector>

namespace devilution {

struct TriggerStruct;
struct Portal;

std::string TriggerLabelForSpeech(const TriggerStruct &trigger);
std::string TownPortalLabelForSpeech(const Portal &portal);
std::vector<int> CollectTownDungeonTriggerIndices();

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
