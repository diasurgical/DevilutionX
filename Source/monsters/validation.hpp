/**
 * @file monsters/validation.hpp
 *
 * Interface of functions for validation of monster data.
 */
#pragma once

#include <cstddef>

namespace devilution {

struct Monster;

bool IsEnemyIdValid(size_t enemyId);
bool IsEnemyValid(size_t monsterId, size_t enemyId);
bool IsMonsterValid(const Monster &monster);
bool IsUniqueMonsterValid(const Monster &monster);

} // namespace devilution
