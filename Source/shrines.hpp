/**
 * @file shrines.hpp
 *
 * Interface of the shrine effects referenced by shrines.tsv.
 */
#pragma once

#include "engine/point.hpp"

namespace devilution {

class DiabloGenerator;
struct Player;

int OperateShrineMysterious(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineHidden(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineGloomy(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineWeird(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineMagical(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineStone(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineReligious(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineEnchanted(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineThaumaturgic(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineFascinating(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineCryptic(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineEldritch(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineEerie(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineDivine(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineHoly(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineSacred(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineSpiritual(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineSpooky(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineAbandoned(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineCreepy(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineQuiet(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineSecluded(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineOrnate(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineGlimmering(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineTainted(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineOily(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineGlowing(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineMendicant(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineSparkling(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineTown(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineShimmering(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineSolar(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);
int OperateShrineMurphys(DiabloGenerator &rng, Player &player, Point spawnPosition, int powerParam1, int powerParam2);

} // namespace devilution
