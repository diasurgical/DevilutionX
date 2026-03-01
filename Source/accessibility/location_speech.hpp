#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "engine/displacement.hpp"
#include "engine/point.hpp"

namespace devilution {

struct Player;

// Path-finding utilities for speech/navigation (BFS over the dungeon grid).
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeech(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable = false);
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechRespectingDoors(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable = false);
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechIgnoringMonsters(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable = false);
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechRespectingDoorsIgnoringMonsters(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable = false);
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechLenient(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable = false);
std::optional<std::vector<int8_t>> FindKeyboardWalkPathToClosestReachableForSpeech(const Player &player, Point startPosition, Point destinationPosition, Point &closestPosition);

void AppendKeyboardWalkPathForSpeech(std::string &message, const std::vector<int8_t> &path);
void AppendDirectionalFallback(std::string &message, const Displacement &delta);

// Key handler that speaks the nearest unexplored tile direction.
void SpeakNearestUnexploredTileKeyPressed();

[[nodiscard]] std::string BuildCurrentLocationForSpeech();

// Key handlers for player status announcements.
void SpeakPlayerHealthPercentageKeyPressed();
void SpeakExperienceToNextLevelKeyPressed();
void SpeakCurrentLocationKeyPressed();

// Key handlers for navigation.
void SpeakNearestExitKeyPressed();
void SpeakNearestTownPortalInTownKeyPressed();
void SpeakNearestStairsDownKeyPressed();
void SpeakNearestStairsUpKeyPressed();

// Keyboard directional walk.
bool IsKeyboardWalkAllowed();
void KeyboardWalkNorthKeyPressed();
void KeyboardWalkSouthKeyPressed();
void KeyboardWalkEastKeyPressed();
void KeyboardWalkWestKeyPressed();

} // namespace devilution
