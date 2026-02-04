/**
 * @file controls/accessibility_keys.hpp
 *
 * UI accessibility key handlers and action-guard helpers.
 */
#pragma once

#include <string>

namespace devilution {

void SpeakPlayerHealthPercentageKeyPressed();
void SpeakExperienceToNextLevelKeyPressed();
std::string BuildCurrentLocationForSpeech();
void SpeakCurrentLocationKeyPressed();
void InventoryKeyPressed();
void CharacterSheetKeyPressed();
void PartyPanelSideToggleKeyPressed();
void QuestLogKeyPressed();
void SpeakSelectedSpeedbookSpell();
void DisplaySpellsKeyPressed();
void SpellBookKeyPressed();
void CycleSpellHotkeys(bool next);

bool IsPlayerDead();
bool IsGameRunning();
bool CanPlayerTakeAction();
bool CanAutomapBeToggledOff();

} // namespace devilution
