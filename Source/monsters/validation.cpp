/**
 * @file monsters/validation.cpp
 *
 * Implementation of functions for validation of monster data.
 */

#include "monsters/validation.hpp"

#include <cstddef>

#include "monster.h"
#include "player.h"

namespace devilution {

namespace {

bool IsEnemyValid(size_t enemyId, bool checkMonsterTable)
{
	if (enemyId < MaxMonsters)
		return !checkMonsterTable || Monsters[enemyId].hitPoints > 0;
	const size_t playerId = enemyId - MaxMonsters;
	return playerId < Players.size() && Players[playerId].plractive;
}

} // namespace

bool IsEnemyIdValid(size_t enemyId)
{
	return IsEnemyValid(enemyId, false);
}

bool IsEnemyValid(size_t monsterId, size_t enemyId)
{
	if (monsterId >= MaxMonsters)
		return false;
	if (monsterId == enemyId)
		return false;
	return IsEnemyValid(enemyId, true);
}

} // namespace devilution
