/**
 * @file player_test.h
 *
 * Helpers for player related tests.
 */
#pragma once

#include "items.h"
#include "player.h"

using namespace devilution;

static size_t CountItems(devilution::Item *items, int n)
{
	return std::count_if(items, items + n, [](devilution::Item x) { return !x.isEmpty(); });
}

static size_t Count8(int8_t *ints, int n)
{
	return std::count_if(ints, ints + n, [](int8_t x) { return x != 0; });
}

static size_t CountU8(uint8_t *ints, int n)
{
	return std::count_if(ints, ints + n, [](uint8_t x) { return x != 0; });
}

static size_t CountBool(bool *bools, int n)
{
	return std::count_if(bools, bools + n, [](bool x) { return x; });
}
