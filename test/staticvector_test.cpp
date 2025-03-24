#include <cstdlib>
#include <ctime>
#include <gtest/gtest.h>

#include "utils/static_vector.hpp"

#define MAX_SIZE 32

using namespace devilution;

namespace {
StaticVector<int, MAX_SIZE> container;

TEST(StaticVector, StaticVector_push_back)
{
	std::srand(clock());
	size_t size = 1 + std::rand() % (MAX_SIZE - 1);
	for (size_t i = 0; i < size; i++) {
		container.push_back(i);
	}

	for (size_t i = 0; i < container.size(); i++) {
		EXPECT_EQ(container[i], i);
	}
}

TEST(StaticVector, StaticVector_erase)
{
	std::srand(clock());
	int erasures = std::rand() % container.size();

	std::vector<int> erase_idx;
	std::vector<int> expected;

	erase_idx.push_back(0);
	erase_idx.push_back(container.size() - 1);

	for (int i = 0; i < erasures; i++) {
		erase_idx.push_back(std::rand() % container.size());
	}

	for (int i = 0; expected.size() < container.size(); i++) {
		expected.push_back(i);
	}

	for (int idx : erase_idx) {
		/* This is actually to guard against gcc's vector implementation causing
		 * crashes, rather than StaticVector
		 */
		if (expected.size() == 0) {
			break;
		}
		container.erase(container.begin() + idx, container.begin() + idx + 1);
		expected.erase(expected.begin() + idx);
	}

	EXPECT_EQ(container.size(), expected.size() );
	for (size_t i = 0; i < expected.size(); i++) {
		EXPECT_EQ(container[i], expected[i]);
	}
}

TEST(StaticVector, StaticVector_clear)
{
	std::vector<int> expected;

	container.clear();
	expected.clear();

	EXPECT_EQ(container.size(), expected.size());

	for(size_t i = 0; i < container.size(); i++) {
		EXPECT_EQ(container[i], expected[i]);
	}
}

} // namespace
