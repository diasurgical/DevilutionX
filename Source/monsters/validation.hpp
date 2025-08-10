/**
 * @file monsters/validation.hpp
 *
 * Interface of functions for validation of monster data.
 */
#pragma once

#include <cstddef>

namespace devilution {

bool IsEnemyIdValid(size_t enemyId);
bool IsEnemyValid(size_t monsterId, size_t enemyId);

} // namespace devilution
