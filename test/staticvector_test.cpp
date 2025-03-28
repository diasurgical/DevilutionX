#include <cstdlib>
#include <gtest/gtest.h>

#include "engine/random.hpp"
#include "utils/static_vector.hpp"

using namespace devilution;

namespace {

constexpr int MAX_SIZE = 32;

TEST(StaticVector, StaticVector_push_back)
{
	StaticVector<size_t, MAX_SIZE> container;

	SetRndSeed(testing::UnitTest::GetInstance()->random_seed());
	size_t size = RandomIntBetween(10, MAX_SIZE);

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
	StaticVector<size_t, MAX_SIZE> container;

	size_t size = MAX_SIZE;
	for (size_t i = 0; i < size; i++) {
		container.push_back(i);
	}

	EXPECT_EQ(container.size(), MAX_SIZE);
	for (size_t i = 0; i < size; i++) {
		EXPECT_EQ(container[i], i);
	}

#ifdef _DEBUG
	ASSERT_DEATH(container.push_back(size + 1), "");
#endif
}

TEST(StaticVector, StaticVector_erase)
{
	StaticVector<size_t, MAX_SIZE> container;
	std::vector<size_t> expected;

	SetRndSeed(testing::UnitTest::GetInstance()->random_seed());

#ifdef _DEBUG
	ASSERT_DEATH(container.erase(container.begin()), "");
#endif

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

	while (!container.empty()) {
		size_t idx = RandomIntLessThan(static_cast<int32_t>(container.size()));
		container.erase(container.begin() + idx);
		expected.erase(expected.begin() + idx);
		if (container.size() > 0) {
			EXPECT_EQ(container.size(), expected.size());
			idx = idx <= 1 ? 0 : idx - 1;
			EXPECT_EQ(container[idx], expected[idx]);
			idx = (idx + 1) >= container.size() ? container.size() - 1 : idx + 1;
			EXPECT_EQ(container[idx], expected[idx]);
		}
	}
	EXPECT_EQ(container.size(), 0);

#ifdef _DEBUG
	ASSERT_DEATH(container.erase(container.begin(), container.end() + 1), "");
	ASSERT_DEATH(container.erase(container.begin() - 1, container.end()), "");
#endif
}

TEST(StaticVector, StaticVector_erase_random)
{
	StaticVector<size_t, MAX_SIZE> container;
	std::vector<size_t> expected;

	SetRndSeed(testing::UnitTest::GetInstance()->random_seed());
	size_t size = RandomIntBetween(10, MAX_SIZE);
	size_t erasures = RandomIntBetween(1, static_cast<int32_t>(size) - 1, true);

	for (size_t i = 0; i < size; i++) {
		container.push_back(i);
		expected.push_back(i);
	}

	while (erasures-- > 0) {
		size_t idx = RandomIntLessThan(static_cast<int32_t>(container.size()));
		container.erase(container.begin() + idx);
		expected.erase(expected.begin() + idx);
	}

	EXPECT_EQ(container.size(), expected.size());
	for (size_t i = 0; i < expected.size(); i++) {
		EXPECT_EQ(container[i], expected[i]);
	}
}

TEST(StaticVector, StaticVector_erase_range)
{
	StaticVector<size_t, MAX_SIZE> container;
	std::vector<size_t> erase_idx;
	std::vector<size_t> expected;

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

	int32_t from = RandomIntBetween(0, static_cast<int32_t>(container.size()) - 1);
	int32_t to = RandomIntBetween(from + 1, static_cast<int32_t>(container.size()));
	container.erase(container.begin() + from, container.begin() + to);
	for (int32_t i = to - from; i > 0; i--) {
		expected.erase(expected.begin() + from);
	}

	EXPECT_EQ(container.size(), expected.size());
	for (size_t i = 0; i < container.size(); i++) {
		EXPECT_EQ(container[i], expected[i]);
	}
}

TEST(StaticVector, StaticVector_clear)
{
	StaticVector<size_t, MAX_SIZE> container;

	container.clear();
	EXPECT_EQ(container.size(), 0);

	container = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	container.clear();
	EXPECT_EQ(container.size(), 0);
}

} // namespace
