/**
 * @file utils/accessibility_announcements.hpp
 *
 * Periodic accessibility announcements (low HP warning, durability, boss health,
 * attackable monsters, interactable doors).
 */
#pragma once

#include "utils/string_or_view.hpp"

namespace devilution {

struct Monster;
struct Object;

void UpdatePlayerLowHpWarningSound();
void UpdateLowDurabilityWarnings();
void UpdateBossHealthAnnouncements();
void UpdateAttackableMonsterAnnouncements();
StringOrView MonsterLabelForSpeech(const Monster &monster);
StringOrView DoorLabelForSpeech(const Object &door);
void UpdateInteractableDoorAnnouncements();

} // namespace devilution
