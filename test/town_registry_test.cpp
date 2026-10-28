/**
 * @file town_registry_test.cpp
 *
 * Unit tests for town registry, entry points, and InitializeTristram.
 */

#include <gtest/gtest.h>

#include <stdexcept>

#include "levels/town_data.h"

using namespace devilution;

namespace {

constexpr size_t TristramEntryPointCount = 7;

constexpr Point TownEntryDefaultFallback = { 75, 68 };

constexpr uint8_t TestTownAlphaSaveId = 7;
constexpr uint8_t TestTownBetaSaveId = 11;
constexpr uint8_t UnknownSaveId = 99;

constexpr int NonMatchingWarpFromLevel = 999;

TEST(TownRegistry, HasTown_ReturnsFalseForUnregistered)
{
	TownRegistry registry;
	EXPECT_FALSE(registry.HasTown("nowhere"));
}

TEST(TownRegistry, RegisterTown_HasTown_GetTown_RoundTrip)
{
	TownRegistry registry;
	TownConfig cfg;
	cfg.name = "Testburg";
	registry.RegisterTown("testburg", cfg);
	EXPECT_TRUE(registry.HasTown("testburg"));
	EXPECT_EQ(registry.GetTown("testburg").name, "Testburg");
}

TEST(TownRegistry, GetTown_ThrowsForUnregistered)
{
	TownRegistry registry;
	EXPECT_THROW(static_cast<void>(registry.GetTown("missing")), std::out_of_range);
}

TEST(TownRegistry, SetCurrentTown_GetCurrentTown_RoundTrip)
{
	TownRegistry registry;
	TownConfig cfg;
	registry.RegisterTown("a", cfg);
	registry.SetCurrentTown("a");
	EXPECT_EQ(registry.GetCurrentTown(), "a");
}

TEST(TownRegistry, GetTownBySaveId_FindsBySaveId)
{
	TownRegistry registry;
	TownConfig alpha;
	alpha.name = "Alpha";
	alpha.saveId = TestTownAlphaSaveId;
	TownConfig beta;
	beta.name = "Beta";
	beta.saveId = TestTownBetaSaveId;
	registry.RegisterTown("alpha", alpha);
	registry.RegisterTown("beta", beta);
	EXPECT_EQ(registry.GetTownBySaveId(TestTownAlphaSaveId), "alpha");
	EXPECT_EQ(registry.GetTownBySaveId(TestTownBetaSaveId), "beta");
}

TEST(TownRegistry, GetTownBySaveId_ReturnsTristramForUnknownNonZeroSaveId)
{
	TownRegistry registry;
	EXPECT_EQ(registry.GetTownBySaveId(UnknownSaveId), TristramTownId);
}

TEST(TownRegistry, GetTownBySaveId_ReturnsTristramWhenZeroNotRegistered)
{
	TownRegistry registry;
	TownConfig cfg;
	cfg.saveId = TestTownAlphaSaveId;
	registry.RegisterTown("only_alpha", cfg);
	EXPECT_EQ(registry.GetTownBySaveId(0), TristramTownId);
}

TEST(TownConfig, GetEntryPoint_ENTRY_MAIN)
{
	TownConfig cfg;
	cfg.entries = {
		{ ENTRY_MAIN, { 10, 20 }, -1 },
	};
	EXPECT_EQ(cfg.GetEntryPoint(ENTRY_MAIN), Point(10, 20));
}

TEST(TownConfig, GetEntryPoint_ENTRY_TWARPUP_MatchesWarpFromLevel)
{
	TownConfig cfg;
	constexpr int WarpFromLevel = 5;
	cfg.entries = {
		{ ENTRY_TWARPUP, { 49, 22 }, WarpFromLevel },
	};
	EXPECT_EQ(cfg.GetEntryPoint(ENTRY_TWARPUP, WarpFromLevel), Point(49, 22));
}

TEST(TownConfig, GetEntryPoint_ENTRY_TWARPUP_NonMatchingWarpUsesDefault)
{
	TownConfig cfg;
	cfg.entries = {
		{ ENTRY_TWARPUP, { 49, 22 }, 5 },
	};
	EXPECT_EQ(cfg.GetEntryPoint(ENTRY_TWARPUP, NonMatchingWarpFromLevel), TownEntryDefaultFallback);
}

TEST(TownConfig, GetEntryPoint_EmptyEntriesUsesDefault)
{
	TownConfig cfg;
	cfg.entries.clear();
	EXPECT_EQ(cfg.GetEntryPoint(ENTRY_MAIN), TownEntryDefaultFallback);
}

class InitializeTristramTest : public ::testing::Test {
protected:
	static void SetUpTestSuite()
	{
		InitializeTristram();
	}
};

TEST_F(InitializeTristramTest, RegistryHasTristram)
{
	TownRegistry &registry = GetTownRegistry();
	EXPECT_TRUE(registry.HasTown(TristramTownId));
}

TEST_F(InitializeTristramTest, CurrentTownIsTristram)
{
	EXPECT_EQ(GetTownRegistry().GetCurrentTown(), TristramTownId);
}

TEST_F(InitializeTristramTest, GetTownBySaveIdZeroReturnsTristram)
{
	EXPECT_EQ(GetTownRegistry().GetTownBySaveId(0), TristramTownId);
}

TEST_F(InitializeTristramTest, TristramEntryPointCount)
{
	const TownConfig &tristram = GetTownRegistry().GetTown(TristramTownId);
	EXPECT_EQ(tristram.entries.size(), TristramEntryPointCount);
}

TEST_F(InitializeTristramTest, TristramSectorCount)
{
	const TownConfig &tristram = GetTownRegistry().GetTown(TristramTownId);
	EXPECT_EQ(tristram.sectors.size(), 4U);
}

TEST(InitializeTristram, PreservesValidCurrentTownOnRepeatCall)
{
	InitializeTristram();
	TownConfig modTown;
	modTown.name = "Mod";
	modTown.saveId = 3;
	TownRegistry &reg = GetTownRegistry();
	reg.RegisterTown("modtown", modTown);
	reg.SetCurrentTown("modtown");

	InitializeTristram();

	EXPECT_EQ(reg.GetCurrentTown(), "modtown");
	reg.SetCurrentTown(TristramTownId);
}

} // namespace
