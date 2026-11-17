/**
 * @file shrines.hpp
 *
 * Interface of the shrine effects referenced by shrines.tsv.
 */
#pragma once

#include "engine/point.hpp"
#include <tables/shrinedat.hpp>

namespace devilution {

ShrineMessage ShrineEffectRandomAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectRandomItemDurabilityChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectArmorClassMaxDamageChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectMinMaxDamageChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectPlayerCastSpellVariantA(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectRechargesStaves(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectRepairsItems(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectRandomSpellLevelChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectRefillsChests(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectChangeSpellLevel(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectManaBaseReduction(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectRestoreLifeAndMana(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectTransformPotionsToRejuvenation(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectMagicAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectCreateManaOrRejuvPotions(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectCreateLifeOrRejuvPotions(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectPlayerCastSpellVariantB(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectFillEmptySlotsGold(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectDexterityAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectStrengthAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectVitalityAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectRevealMap(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectIdentifyItems(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectPreferredAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectCastFirewallTrap(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectExperienceToMagicAttribute(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectGoldToExperience(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectExperiencePerDungeonLevel(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectCastFlashBottomTrap(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectCastSpellTrap(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectPlayerCastSpellVariantC(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectLocalHourAttributeChange(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);
ShrineMessage ShrineEffectBreakItemOrTakeGold(DiabloGenerator &rng, Player &operatePlayer, const Point spawnPosition, const ShrineEffect &shrineEffect);

} // namespace devilution
