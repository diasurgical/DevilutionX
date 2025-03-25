#include <cstdlib>
#include <gtest/gtest.h>

#include "engine/random.hpp"
#include "utils/static_vector.hpp"

using namespace devilution;

namespace {

constexpr size_t MAX_SIZE = 32;

TEST(StaticVector, StaticVector_push_back)
{
	StaticVector<int, MAX_SIZE> container;

	SetRndSeed(testing::UnitTest::GetInstance()->random_seed());
	size_t size = RandomIntBetween(10, MAX_SIZE, true);

	for (size_t i = 0; i < size; i++) {
		container.push_back(i);
	}

	EXPECT_EQ(container.size(), size);
	for (size_t i = 0; i < size; i++) {
		EXPECT_EQ(container[i], i);
	}
}

TEST(StaticVector, StaticVector_erase_random)
{
	StaticVector<int, MAX_SIZE> container;
	std::vector<int> erase_idx;
	std::vector<int> expected;

	SetRndSeed(testing::UnitTest::GetInstance()->random_seed());
	size_t size = RandomIntBetween(10, MAX_SIZE, true);

	for (size_t i = 0; i < size; i++) {
		container.push_back(i);
	}

	int erasures = RandomIntBetween(1, size, true);


	for (int i = 0; i < erasures; i++) {
		erase_idx.push_back(RandomIntBetween(0, size, true));
	}

	for (int i = 0; expected.size() < container.size(); i++) {
		expected.push_back(i);
	}

	for (int idx : erase_idx) {
		container.erase(container.begin() + idx, container.begin() + idx + 1);
		expected.erase(expected.begin() + idx);
		erase_idx.erase(erase_idx.begin());
		for (int i = erase_idx.size() - 1; i >= 0; i--) {
			erase_idx[i] -= erase_idx[i] != 0 ? 1 : 0;
		}
	}

	EXPECT_EQ(container.size(), expected.size());
	for (size_t i = 0; i < expected.size(); i++) {
		EXPECT_EQ(container[i], expected[i]);
	}
}

TEST(StaticVector, StaticVector_clear)
{
	StaticVector<int, MAX_SIZE> container;

	container.clear();
	EXPECT_EQ(container.size(), 0);

	container = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	container.clear();
	EXPECT_EQ(container.size(), 0);
}

} // namespace
