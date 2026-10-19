#pragma once

namespace devilution {

/** Optional gameplay tuning. Defaults preserve the standard game rules. */
struct GameplayRules {
	int maxResistance = 75;
	int goldStackLimit = 5000;
	int vendorValueLimit = 140000;
	int boyValueLimit = 90000;
	int nightmareToHitBonus = 85;
	int hellToHitBonus = 120;
	int nightmareAcBonus = 50;
	int hellAcBonus = 80;
	int hellHitPointBonus = 200;
	int fireballDamageMultiplier = 2;
	int fireballDamageBonus = 4;
	int chainLightningLevelDivisor = 1;
	int allowMovementDuringDiabloDeath = 0;
	int automapStartsActive = 0;
};

inline GameplayRules Gameplay;

/** Reset defaults, then read the optional txtdata/gameplay.tsv override. */
void LoadGameplayRules();

} // namespace devilution
