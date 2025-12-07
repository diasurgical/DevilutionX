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

bool IsMonsterValid(const Monster &monster)
{
	const CMonster &monsterType = LevelMonsterTypes[monster.levelType];
	const _monster_id monsterId = monsterType.type;
	const size_t monsterIndex = static_cast<size_t>(monsterId);

	if (monsterIndex >= MonstersData.size()) {
		return false;
	}

	if (monster.isUnique() && !IsUniqueMonsterValid(monster)) {
		return false;
	}

	return true;
}

bool IsUniqueMonsterValid(const Monster &monster)
{
	assert(monster.isUnique());

	const size_t uniqueMonsterIndex = static_cast<size_t>(monster.uniqueType);
	if (uniqueMonsterIndex >= UniqueMonstersData.size()) {
		return false;
	}

	const CMonster &monsterType = LevelMonsterTypes[monster.levelType];
	const _monster_id monsterId = monsterType.type;
	const UniqueMonsterData &uniqueMonsterData = UniqueMonstersData.at(uniqueMonsterIndex);
	if (monsterId != uniqueMonsterData.mtype) {
		return false;
	}

	return true;
}

} // namespace devilution
