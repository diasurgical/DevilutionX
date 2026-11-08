/**
 * @file town_registry_test.cpp
 *
 * Unit tests for town registry, entry points, and InitializeTristram.
 */

#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "levels/town_data.h"
#include "tables/textdat.h"
#include "tables/townerdat.hpp"
#include "towners.h"

using namespace devilution;

namespace {

constexpr size_t TristramTriggerCount = 6;
constexpr size_t TristramEntryPointCount = 8;

constexpr Point TristramCathedralTrigPosition = { 25, 29 };

constexpr Point TownEntryDefaultFallback = { 75, 68 };

constexpr uint8_t TestTownAlphaId = 7;
constexpr uint8_t TestTownBetaId = 11;
constexpr uint8_t TestTownModId = 3;
constexpr uint8_t TestTownPortalId = 4;
constexpr uint8_t UnknownTownId = 99;

constexpr int NonMatchingWarpFromLevel = 999;

TEST(TownRegistry, HasTown_ReturnsFalseForUnregistered)
{
	TownRegistry registry;
	EXPECT_FALSE(registry.HasTown(UnknownTownId));
}

TEST(TownRegistry, RegisterTown_HasTown_GetTown_RoundTrip)
{
	TownRegistry registry;
	TownConfig cfg;
	cfg.name = "Testburg";
	cfg.townId = TestTownAlphaId;
	registry.RegisterTown(cfg);
	EXPECT_TRUE(registry.HasTown(TestTownAlphaId));
	EXPECT_EQ(registry.GetTown(TestTownAlphaId).name, "Testburg");
}

TEST(TownRegistry, GetTown_ThrowsForUnregistered)
{
	TownRegistry registry;
	EXPECT_THROW(static_cast<void>(registry.GetTown(UnknownTownId)), std::out_of_range);
}

TEST(TownRegistry, SetCurrentTown_GetCurrentTown_RoundTrip)
{
	TownRegistry registry;
	TownConfig cfg;
	cfg.townId = TestTownAlphaId;
	registry.RegisterTown(cfg);
	registry.SetCurrentTown(TestTownAlphaId);
	EXPECT_EQ(registry.GetCurrentTown(), std::optional<uint8_t> { TestTownAlphaId });
}

TEST(TownRegistry, GetTown_IndependentOfRegistrationOrder)
{
	TownConfig alpha;
	alpha.name = "Alpha";
	alpha.townId = TestTownAlphaId;
	TownConfig beta;
	beta.name = "Beta";
	beta.townId = TestTownBetaId;

	TownRegistry forward;
	forward.RegisterTown(alpha);
	forward.RegisterTown(beta);

	TownRegistry reverse;
	reverse.RegisterTown(beta);
	reverse.RegisterTown(alpha);

	EXPECT_EQ(forward.GetTown(TestTownAlphaId).name, "Alpha");
	EXPECT_EQ(reverse.GetTown(TestTownAlphaId).name, "Alpha");
	EXPECT_EQ(forward.GetTown(TestTownBetaId).name, "Beta");
	EXPECT_EQ(reverse.GetTown(TestTownBetaId).name, "Beta");
}

TEST(TownRegistry, RegisterTown_OverwriteSameTownId)
{
	TownRegistry registry;
	TownConfig first;
	first.name = "First";
	first.townId = TestTownAlphaId;
	TownConfig second;
	second.name = "Second";
	second.townId = TestTownAlphaId;

	registry.RegisterTown(first);
	registry.RegisterTown(second);
	EXPECT_EQ(registry.GetTown(TestTownAlphaId).name, "Second");
	EXPECT_FALSE(registry.HasTown(TestTownBetaId));
}

TEST(TownRegistry, HasTown_FalseWhenTristramNotRegistered)
{
	TownRegistry registry;
	TownConfig cfg;
	cfg.townId = TestTownAlphaId;
	registry.RegisterTown(cfg);
	EXPECT_FALSE(registry.HasTown(TristramTownId));
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

TEST(TownConfig, GetEntryPoint_ENTRY_TOWNSWITCH)
{
	TownConfig cfg;
	cfg.entries = {
		{ ENTRY_TOWNSWITCH, { 28, 28 }, -1 },
	};
	EXPECT_EQ(cfg.GetEntryPoint(ENTRY_TOWNSWITCH), Point(28, 28));
}

TEST(RoleNameForShortName, MapsLegacyModNames)
{
	const std::optional<std::string_view> drunk = RoleNameForShortName("farnham");
	ASSERT_TRUE(drunk.has_value());
	EXPECT_EQ(*drunk, "drunk");
	const std::optional<std::string_view> smith = RoleNameForShortName("griswold");
	ASSERT_TRUE(smith.has_value());
	EXPECT_EQ(*smith, "smith");
	EXPECT_FALSE(RoleNameForShortName("not_an_npc").has_value());
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
	EXPECT_EQ(registry.GetTown(TristramTownId).name, "Tristram");
}

TEST_F(InitializeTristramTest, CurrentTownIsTristram)
{
	EXPECT_EQ(GetTownRegistry().GetCurrentTown(), std::optional<uint8_t> { TristramTownId });
}

TEST_F(InitializeTristramTest, GetTownZeroReturnsTristram)
{
	EXPECT_EQ(GetTownRegistry().GetTown(TristramTownId).townId, TristramTownId);
}

TEST_F(InitializeTristramTest, TristramTriggerCount)
{
	const TownConfig &tristram = GetTownRegistry().GetTown(TristramTownId);
	EXPECT_EQ(tristram.triggers.size(), TristramTriggerCount);
}

TEST_F(InitializeTristramTest, TristramFirstTriggerIsCathedralStairs)
{
	const TownConfig &tristram = GetTownRegistry().GetTown(TristramTownId);
	ASSERT_FALSE(tristram.triggers.empty());
	const TownTrigger &first = tristram.triggers.front();
	EXPECT_EQ(first.position, TristramCathedralTrigPosition);
	EXPECT_EQ(first.msg, WM_DIABNEXTLVL);
}

TEST_F(InitializeTristramTest, TristramEntryPointCount)
{
	const TownConfig &tristram = GetTownRegistry().GetTown(TristramTownId);
	EXPECT_EQ(tristram.entries.size(), TristramEntryPointCount);
}

TEST_F(InitializeTristramTest, TristramPortalPositionsMatchLegacy)
{
	const TownConfig &tristram = GetTownRegistry().GetTown(TristramTownId);
	for (size_t i = 0; i < NumTownPortalSlots; ++i) {
		EXPECT_EQ(tristram.portalPositions[i], DefaultTristramPortalPositions[i]);
	}
}

TEST_F(InitializeTristramTest, GetPortalTownPositionMatchesTristramWhenCurrent)
{
	for (size_t i = 0; i < NumTownPortalSlots; ++i) {
		EXPECT_EQ(GetPortalTownPosition(i), DefaultTristramPortalPositions[i]);
	}
}

TEST_F(InitializeTristramTest, GetPortalTownPositionUsesTownOverrides)
{
	TownConfig c;
	c.name = "PortTest";
	c.townId = TestTownPortalId;
	c.portalPositions = DefaultTristramPortalPositions;
	c.portalPositions[0] = { 99, 88 };
	GetTownRegistry().RegisterTown(c);
	GetTownRegistry().SetCurrentTown(TestTownPortalId);
	EXPECT_EQ(GetPortalTownPosition(0), Point(99, 88));
	EXPECT_EQ(GetPortalTownPosition(1), DefaultTristramPortalPositions[1]);
	GetTownRegistry().SetCurrentTown(TristramTownId);
}

TEST_F(InitializeTristramTest, TristramSectorCount)
{
	const TownConfig &tristram = GetTownRegistry().GetTown(TristramTownId);
	EXPECT_EQ(tristram.sectors.size(), 4U);
}

TEST_F(InitializeTristramTest, TristramAssetsMatchVanillaPaths)
{
	const TownAssets &assets = GetTownRegistry().GetTown(TristramTownId).assets;
	EXPECT_EQ(assets.dungeonCelPath, R"(nlevels\towndata\town.cel)");
	EXPECT_EQ(assets.megaTilePath, R"(nlevels\towndata\town.til)");
	EXPECT_EQ(assets.pieceMinPath, R"(nlevels\towndata\town.min)");
	EXPECT_EQ(assets.specialCelsPath, R"(levels\towndata\towns)");
	EXPECT_EQ(assets.solPath, R"(nlevels\towndata\town.sol)");
	EXPECT_EQ(assets.palettePath, R"(levels\towndata\town.pal)");
	EXPECT_EQ(assets.microTileLen, static_cast<uint_fast8_t>(16));
	EXPECT_STREQ(TownAssets::RetailDungeonCel, R"(levels\towndata\town.cel)");
	EXPECT_STREQ(TownAssets::RetailMegaTile, R"(levels\towndata\town.til)");
	EXPECT_STREQ(TownAssets::RetailPieceMin, R"(levels\towndata\town.min)");
	EXPECT_STREQ(TownAssets::RetailSol, R"(levels\towndata\town.sol)");
}

TEST(InitializeTristram, PreservesValidCurrentTownOnRepeatCall)
{
	InitializeTristram();
	TownConfig modTown;
	modTown.name = "Mod";
	modTown.townId = TestTownModId;
	TownRegistry &reg = GetTownRegistry();
	reg.RegisterTown(modTown);
	reg.SetCurrentTown(TestTownModId);

	InitializeTristram();

	EXPECT_EQ(reg.GetCurrentTown(), std::optional<uint8_t> { TestTownModId });
	reg.SetCurrentTown(TristramTownId);
}

class TownNpcTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		saved_ = TownersDataEntries;
		TownersDataEntries.clear();

		TownerDataEntry healer;
		healer.type = TOWN_HEALER;
		healer.name = "Pepin the Healer";
		healer.shortName = "Pepin";
		healer.position = { 55, 79 };
		healer.direction = Direction::SouthEast;
		healer.animWidth = 96;
		healer.animPath = "towners\\healer\\healer";
		healer.animFrames = 20;
		healer.animDelay = 6;
		healer.gossipTexts = { TEXT_PEPIN2 };
		TownersDataEntries.push_back(healer);
	}

	void TearDown() override { TownersDataEntries = saved_; }

private:
	std::vector<TownerDataEntry> saved_;
};

TEST_F(TownNpcTest, NoNpcs_UsesDefaultEntries)
{
	TownConfig cfg;
	const std::vector<TownerDataEntry> entries = ResolveTownerEntries(&cfg);
	ASSERT_EQ(entries.size(), TownersDataEntries.size());
	EXPECT_EQ(entries[0].position, TownersDataEntries[0].position);
	EXPECT_EQ(ResolveTownerEntries(nullptr).size(), TownersDataEntries.size());
}

TEST_F(TownNpcTest, PositionOnly_KeepsTristramData)
{
	TownConfig cfg;
	TownNpc npc;
	npc.role = "healer";
	npc.position = { 40, 52 };
	cfg.npcs.push_back(npc);

	const std::vector<TownerDataEntry> entries = ResolveTownerEntries(&cfg);
	ASSERT_EQ(entries.size(), 1U);
	EXPECT_EQ(entries[0].type, TOWN_HEALER);
	EXPECT_EQ(entries[0].position, (Point { 40, 52 }));
	EXPECT_EQ(entries[0].animPath, TownersDataEntries[0].animPath);
	EXPECT_EQ(entries[0].gossipTexts, TownersDataEntries[0].gossipTexts);
	EXPECT_EQ(entries[0].shortName, "Pepin");
}

TEST_F(TownNpcTest, CustomFields_AreApplied)
{
	TownConfig cfg;
	TownNpc npc;
	npc.role = "healer";
	npc.position = { 10, 11 };
	npc.name = "Tremain the Priest";
	npc.animPath = "towners\\priest\\priest8";
	npc.gossip = std::vector<std::string> { "TEXT_PEPIN3" };
	cfg.npcs.push_back(npc);

	const std::vector<TownerDataEntry> entries = ResolveTownerEntries(&cfg);
	ASSERT_EQ(entries.size(), 1U);
	EXPECT_EQ(entries[0].name, "Tremain the Priest");
	EXPECT_TRUE(entries[0].shortName.empty());
	EXPECT_EQ(entries[0].animPath, "towners\\priest\\priest8");
	ASSERT_EQ(entries[0].gossipTexts.size(), 1U);
	EXPECT_EQ(entries[0].gossipTexts[0], TEXT_PEPIN3);
}

TEST_F(TownNpcTest, UnknownRoleAndGossip_AreSkipped)
{
	TownConfig cfg;
	TownNpc badRole;
	badRole.role = "jester";
	cfg.npcs.push_back(badRole);
	TownNpc badGossip;
	badGossip.role = "healer";
	badGossip.gossip = std::vector<std::string> { "NOT_A_SPEECH", "TEXT_PEPIN3" };
	cfg.npcs.push_back(badGossip);

	const std::vector<TownerDataEntry> entries = ResolveTownerEntries(&cfg);
	ASSERT_EQ(entries.size(), 1U);
	ASSERT_EQ(entries[0].gossipTexts.size(), 1U);
	EXPECT_EQ(entries[0].gossipTexts[0], TEXT_PEPIN3);
}

TEST(TownerNames, ParseRole)
{
	EXPECT_EQ(ParseTownerRole("healer"), std::optional<_talker_id> { TOWN_HEALER });
	EXPECT_EQ(ParseTownerRole("storyteller"), std::optional<_talker_id> { TOWN_STORY });
	EXPECT_FALSE(ParseTownerRole("jester").has_value());
}

class TownerNameLookupTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		savedTowners_ = std::move(Towners);
		savedEntries_ = TownersDataEntries;
		savedLongNames_ = TownerLongNames;
		Towners.clear();
		TownersDataEntries.clear();
		TownerDataEntry smith;
		smith.type = TOWN_SMITH;
		smith.name = "Griswold the Blacksmith";
		smith.shortName = "Griswold";
		TownersDataEntries.push_back(smith);
		TownerLongNames.clear();
		TownerLongNames[TOWN_HEALER] = "Pepin the Healer";
		SetTalkingTowner(std::nullopt);
	}

	void TearDown() override
	{
		SetTalkingTowner(std::nullopt);
		Towners = std::move(savedTowners_);
		TownersDataEntries = savedEntries_;
		TownerLongNames = savedLongNames_;
	}

	/** Mirrors InitTownerInfo: an empty short name falls back to the full name. */
	static void AddTowner(_talker_id type, std::string_view name, std::string_view shortName = {})
	{
		Towner &towner = Towners.emplace_back();
		towner._ttype = type;
		towner.name = name;
		towner.shortName = shortName.empty() ? name : shortName;
	}

private:
	std::vector<Towner> savedTowners_;
	std::vector<TownerDataEntry> savedEntries_;
	std::unordered_map<_talker_id, std::string> savedLongNames_;
};

TEST_F(TownerNameLookupTest, DefaultHealer_UsesTristramNames)
{
	AddTowner(TOWN_HEALER, "Pepin the Healer", "Pepin");
	EXPECT_EQ(GetTownerShortName(TOWN_HEALER), "Pepin");
	EXPECT_EQ(GetTownerFullName(TOWN_HEALER), "Pepin the Healer");
}

TEST_F(TownerNameLookupTest, RenamedHealer_UsesCustomName)
{
	AddTowner(TOWN_HEALER, "Tremain the Priest");
	EXPECT_EQ(GetTownerShortName(TOWN_HEALER), "Tremain the Priest");
	EXPECT_EQ(GetTownerFullName(TOWN_HEALER), "Tremain the Priest");
}

TEST_F(TownerNameLookupTest, DuplicateHealers_UseTheOneTalkedTo)
{
	AddTowner(TOWN_HEALER, "Akara the Healer");
	AddTowner(TOWN_HEALER, "Tremain the Priest");

	SetTalkingTowner(1);
	EXPECT_EQ(GetTownerShortName(TOWN_HEALER), "Tremain the Priest");
	SetTalkingTowner(0);
	EXPECT_EQ(GetTownerShortName(TOWN_HEALER), "Akara the Healer");
	SetTalkingTowner(std::nullopt);
	EXPECT_EQ(GetTownerShortName(TOWN_HEALER), "Akara the Healer");
	EXPECT_EQ(GetTalkingTowner(TOWN_HEALER), &Towners[0]);
}

TEST_F(TownerNameLookupTest, TalkingToOtherRole_DoesNotAffectLookup)
{
	AddTowner(TOWN_SMITH, "Griswold the Blacksmith", "Griswold");
	AddTowner(TOWN_HEALER, "Pepin the Healer", "Pepin");
	SetTalkingTowner(0);
	EXPECT_EQ(GetTownerFullName(TOWN_HEALER), "Pepin the Healer");
	EXPECT_EQ(GetTalkingTowner(TOWN_HEALER), &Towners[1]);
}

TEST_F(TownerNameLookupTest, StaleTalkingIndex_IsIgnored)
{
	AddTowner(TOWN_HEALER, "Pepin the Healer");
	SetTalkingTowner(5);
	EXPECT_EQ(GetTalkingTowner(TOWN_HEALER), &Towners[0]);
}

TEST_F(TownerNameLookupTest, UnspawnedRole_FallsBackToDefaults)
{
	EXPECT_EQ(GetTownerShortName(TOWN_SMITH), "Griswold");
	EXPECT_EQ(GetTownerFullName(TOWN_HEALER), "Pepin the Healer");
	EXPECT_EQ(GetTalkingTowner(TOWN_SMITH), nullptr);
}

} // namespace
