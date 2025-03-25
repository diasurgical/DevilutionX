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

TEST(StaticVector, StaticVector_push_back_full)
{
	StaticVector<int, MAX_SIZE> container;

	size_t size = MAX_SIZE;
	for (size_t i = 0; i < size; i++) {
		container.push_back(i);
	}

	EXPECT_EQ(container.size(), MAX_SIZE);
	for (size_t i = 0; i < size; i++) {
		EXPECT_EQ(container[i], i);
	}
}

TEST(StaticVector, StaticVector_erase)
{
	StaticVector<int, MAX_SIZE> container;
	std::vector<int> expected;

	SetRndSeed(testing::UnitTest::GetInstance()->random_seed());

	container.erase(container.begin());
	EXPECT_EQ(container.size(), 0);

	container = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	expected = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	container.erase(container.begin());
	EXPECT_EQ(container.size(), expected.size());
	for (size_t i = 0; i < container.size(); i++) {
		EXPECT_EQ(container[i], expected[i]);
	}

	expected = { 1, 2, 3, 4, 5, 6, 7, 8 };
	container.erase(container.end() - 1, container.end());
	EXPECT_EQ(container.size(), expected.size());
	for (size_t i = 0; i < container.size(); i++) {
		EXPECT_EQ(container[i], expected[i]);
	}

	container.erase(container.begin(), container.end() + 1);
	EXPECT_EQ(container.size(), expected.size());

	container.erase(container.begin() - 1, container.end());
	EXPECT_EQ(container.size(), expected.size());

	int erasures = container.size();
	for (int i = 0; i < erasures && container.size() > 0; i++) {
		size_t idx = RandomIntLessThan(container.size());
		container.erase(container.begin() + idx);
		expected.erase(expected.begin() + idx);
		if (container.size() > 0) {
			EXPECT_EQ(container.size(), expected.size());
			idx = idx == 0 ? 1 : idx;
			EXPECT_EQ(container[idx - 1], expected[idx - 1]);
			idx = (idx + 1) >= container.size() ? container.size() - 1 : idx;
			EXPECT_EQ(container[idx + 1], expected[idx + 1]);
		}
	}

	EXPECT_EQ(container.size(), 0);
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

	while (erase_idx.size() > 0) {
		int idx = erase_idx.front();
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

TEST(StaticVector, StaticVector_erase_range)
{
	StaticVector<int, MAX_SIZE> container;
	std::vector<int> erase_idx;
	std::vector<int> expected;

	SetRndSeed(testing::UnitTest::GetInstance()->random_seed());

	container = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	expected = { 3, 4, 5, 6, 7, 8, 9 };
	container.erase(container.begin(), container.begin() + 3);

	EXPECT_EQ(container.size(), expected.size());
	for (size_t i = 0; i < container.size(); i++) {
		EXPECT_EQ(container[i], expected[i]);
	}

	container.erase(container.begin() + 1, container.begin() + 1);
	EXPECT_EQ(container.size(), expected.size());
	for (size_t i = 0; i < container.size(); i++) {
		EXPECT_EQ(container[i], expected[i]);
	}

	int from = RandomIntBetween(0, container.size() - 1, true);
	int to = RandomIntBetween(from, container.size() - 1, true);

	container.erase(container.begin() + from, container.begin() + to);
	for (int i = to - from; i > 0; i--) {
		expected.erase(expected.begin() + from);
	}

	EXPECT_EQ(container.size(), expected.size());
	for (size_t i = 0; i < container.size(); i++) {
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
