/**
 * @file utils/accessibility_announcements.hpp
 *
 * Periodic accessibility announcements (low HP warning, durability, boss health,
 * attackable monsters, interactable doors).
 */
#pragma once

#include "utils/string_or_view.hpp"

namespace devilution {

struct Object;

void UpdatePlayerLowHpWarningSound();
void UpdateLowDurabilityWarnings();
void UpdateBossHealthAnnouncements();
void UpdateAttackableMonsterAnnouncements();
StringOrView DoorLabelForSpeech(const Object &door);
void UpdateInteractableDoorAnnouncements();

} // namespace devilution
