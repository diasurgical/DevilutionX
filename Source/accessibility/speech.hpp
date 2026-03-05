#pragma once

#include "utils/string_or_view.hpp"

namespace devilution {

struct Object;

[[nodiscard]] StringOrView DoorLabelForSpeech(const Object &door);

void UpdatePlayerLowHpWarningSound();
void UpdateLowDurabilityWarnings();
void UpdateBossHealthAnnouncements();
void UpdateAttackableMonsterAnnouncements();
void UpdateInteractableDoorAnnouncements();

} // namespace devilution
