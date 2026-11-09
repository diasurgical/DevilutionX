#pragma once

#include "engine/point.hpp"
#include "player.h"
#include <tables/shrinedat.hpp>

#include "engine/random.hpp"

namespace devilution {

int OperateShrineMysterious(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineHidden(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineGloomy(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineWeird(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineCastSpellPlayerPosition(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineStone(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineReligious(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineEnchanted(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineThaumaturgic(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineCostOfWisdom(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineEldritch(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineEerie(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineDivine(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineCastSpellDestinyZero(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineSpiritual(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineSpooky(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineAbandoned(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineCreepy(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineQuiet(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineSecluded(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineGlimmering(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineTainted(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineOily(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineGlowing(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineMendicant(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineSparkling(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineCastSpellShrinePosition(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineShimmering(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineSolar(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);
int OperateShrineMurphys(DiabloGenerator &rng, Player &player, Point spawnPosition, const ShrineData &shrineData);

} // namespace devilution
