#include <algorithm>

#include <gtest/gtest.h>

#include "engine/assets.hpp"
#include "game_mode.hpp"
#include "items.h"
#include "mods/mod_identity.h"
#include "objects.h"
#include "panels/spell_book.hpp"
#include "player.h"
#include "tables/gameplay.h"
#include "tables/misdat.h"
#include "tables/monstdat.h"
#include "tables/playerdat.hpp"
#include "tables/presentation.hpp"
#include "tables/questdat.hpp"
#include "tables/spelldat.h"
#include "tables/textdat.h"
#include "tables/townerdat.hpp"
#include "utils/file_util.h"
#include "utils/paths.h"

namespace devilution {
namespace {

void LoadRules()
{
	LoadTextData();
	LoadGameplayRules();
	LoadPresentationRules();
	LoadPlayerDataFiles();
	LoadSpellData();
	LoadSpellBookLayout();
	LoadMissileData();
	LoadMonsterData();
	LoadItemData();
	LoadObjectData();
	LoadShrineRules();
	LoadQuestData();
	LoadTownerData();
}

class AbysmalTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		savedPrefPath = paths::PrefPath();
		savedAssetsPath = paths::AssetsPath();
		paths::SetPrefPath(paths::BasePath() + "test/abysmal/");
		paths::SetAssetsPath(paths::BasePath() + "assets/");
		gbIsHellfire = false;
		gbIsSpawn = false;
		gbIsMultiplayer = false;
		Players.resize(1);
		MyPlayer = &Players[0];
		*MyPlayer = {};
		UnloadModArchives();
		LoadModArchives({ { "abysmal" } });
		LoadRules();
	}

	void TearDown() override
	{
		FreeSpellBook();
		UnloadModArchives();
		LoadModArchives({});
		LoadRules();
		paths::SetPrefPath(savedPrefPath);
		paths::SetAssetsPath(savedAssetsPath);
	}

	std::string savedPrefPath;
	std::string savedAssetsPath;
};

TEST_F(AbysmalTest, DataLoadsInStartupOrderAndVanillaReloadRestoresDefaults)
{
	EXPECT_EQ(GetActiveModSaveExtension(), "asv");
	EXPECT_EQ(GetSpellBookPageCount(), 6u);
	EXPECT_EQ(GetSpellData(SpellID::Rage).staffValueMultiplier, 3);
	EXPECT_EQ(GetSpellData(SpellID::Rage).iconFrame, 16);
	EXPECT_EQ(GetSpellData(SpellID::BoneSpirit).iconFrame, 30);
	EXPECT_EQ(GetSpellData(SpellID::DoomSerpents).iconFrame, 34);
	EXPECT_EQ(GetSpellData(SpellID::Invisibility).iconFrame, 31);
	EXPECT_EQ(GetSpellData(SpellID::Resurrect).iconFrame, 19);
	EXPECT_EQ(Presentation.victoryMovie, "gendata\\doom.smk");
	EXPECT_EQ(Presentation.bookcaseName, "Bookshelf");
	EXPECT_EQ(Gameplay.maxResistance, 70);
	EXPECT_EQ(Gameplay.goldStackLimit, 65000);
	EXPECT_EQ(Gameplay.automapStartsActive, 1);
	EXPECT_EQ(GetClassAttributes(HeroClass::Warrior).baseStr, 35);
	EXPECT_EQ(GetClassAttributes(HeroClass::Warrior).maxVit, 135);
	EXPECT_EQ(GetClassAttributes(HeroClass::Rogue).maxDex, 255);
	EXPECT_EQ(GetClassAttributes(HeroClass::Sorcerer).baseVit, 0);
	EXPECT_EQ(GetClassAttributes(HeroClass::Sorcerer).maxDex, 115);
	EXPECT_EQ(GetPlayerStartingLoadoutForClass(HeroClass::Rogue).skill, SpellID::Infravision);
	EXPECT_EQ(GetPlayerStartingLoadoutForClass(HeroClass::Sorcerer).skill, SpellID::Identify);
	EXPECT_EQ(MonstersData.size(), NUM_DEFAULT_MTYPES + 1u);
	EXPECT_EQ(UniqueMonstersData.size(), 100u);
	EXPECT_EQ(AllItemsList.size(), 181u);
	EXPECT_EQ(UniqueItems.size(), 90u);
	EXPECT_EQ(ItemPrefixes.size(), 83u);
	EXPECT_EQ(ItemSuffixes.size(), 95u);
	EXPECT_EQ(SpellsData.size(), 37u);
	EXPECT_NE(GetMissileData(MissileID::DoomSerpents).addFn, nullptr);
	EXPECT_NE(GetMissileData(MissileID::BloodRitual).processFn, nullptr);
	EXPECT_NE(GetMissileData(MissileID::Invisibility).processFn, nullptr);
	EXPECT_EQ(QuestsData[Q_BUTCHER]._qdlvl, 3);
	EXPECT_EQ(QuestsData[Q_BUTCHER]._qdmultlvl, 3);
	EXPECT_EQ(QuestsData[Q_SKELKING]._qdlvl, 4);
	EXPECT_EQ(QuestsData[Q_SKELKING]._qdmultlvl, 4);
	for (const TownerDataEntry &towner : TownersDataEntries) {
		if (towner.type == TOWN_WITCH) {
			EXPECT_EQ(towner.position, Point(55, 44));
		}
		if (towner.type == TOWN_PEGBOY) {
			EXPECT_EQ(towner.position, Point(67, 38));
		}
	}

	UnloadModArchives();
	LoadModArchives({});
	LoadRules();
	EXPECT_TRUE(GetActiveModSaveExtension().empty());
	EXPECT_EQ(GetSpellBookPageCount(), 4u);
	EXPECT_EQ(GetSpellData(SpellID::Rage).staffValueMultiplier, 1);
	for (SpellID spell : { SpellID::Rage, SpellID::BoneSpirit, SpellID::DoomSerpents, SpellID::Invisibility, SpellID::Resurrect })
		EXPECT_EQ(GetSpellData(spell).iconFrame, -1);
	EXPECT_TRUE(Presentation.victoryMovie.empty());
	EXPECT_TRUE(Presentation.bookcaseName.empty());
	EXPECT_EQ(Gameplay.maxResistance, 75);
	EXPECT_EQ(Gameplay.goldStackLimit, 5000);
	EXPECT_EQ(Gameplay.automapStartsActive, 0);
	EXPECT_EQ(GetClassAttributes(HeroClass::Warrior).baseStr, 30);
	EXPECT_EQ(GetPlayerStartingLoadoutForClass(HeroClass::Rogue).skill, SpellID::TrapDisarm);
	EXPECT_EQ(GetPlayerStartingLoadoutForClass(HeroClass::Sorcerer).skill, SpellID::StaffRecharge);
	EXPECT_EQ(QuestsData[Q_BUTCHER]._qdlvl, 2);
	EXPECT_EQ(QuestsData[Q_SKELKING]._qdlvl, 3);
	EXPECT_EQ(AllItemsList.size(), 168u);
}

TEST_F(AbysmalTest, EveryUniqueHasAnAvailableBaseAndRewardsSurviveRecreation)
{
	for (const UniqueItem &unique : UniqueItems) {
		bool available = false;
		for (size_t i = 0; i < AllItemsList.size(); ++i) {
			const ItemData &base = AllItemsList[i];
			if (base.iItemId == unique.UIItemId && base.dropRate != 0 && IsItemAvailable(static_cast<int>(i)))
				available = true;
		}
		EXPECT_TRUE(available) << unique.UIName;
	}

	for (const int uid : { 9, 33, 64 }) {
		const auto base = std::find_if(AllItemsList.begin(), AllItemsList.end(), [uid](const ItemData &item) {
			return item.iItemId == UniqueItems[uid].UIItemId;
		});
		ASSERT_NE(base, AllItemsList.end());
		const auto index = static_cast<_item_indexes>(std::distance(AllItemsList.begin(), base));
		Item item {};
		SetupForcedUniqueItem(*MyPlayer, item, index, static_cast<_unique_items>(uid));
		Item recreated {};
		RecreateItem(*MyPlayer, recreated, item.IDidx, item._iCreateInfo,
		    item._iSeed, item._ivalue, item.dwBuff);
		EXPECT_EQ(recreated._iMagical, ITEM_QUALITY_UNIQUE);
		EXPECT_EQ(recreated._iUid, uid);
		EXPECT_STREQ(recreated._iIName, item._iIName);
		EXPECT_EQ(recreated._iCurs, item._iCurs);
		EXPECT_EQ(recreated._iFlags, item._iFlags);
		EXPECT_EQ(recreated._iPLStr, item._iPLStr);
	}
}

TEST_F(AbysmalTest, ExpandedSpellbookLoadsWithOriginalGameAssets)
{
	if (!FileExists(paths::BasePath() + "DIABDAT.MPQ")
	    && !FileExists(paths::BasePath() + "diabdat.mpq"))
		GTEST_SKIP() << "Original Diablo MPQ required for spellbook graphics";
	LoadCoreArchives();
	LoadGameArchives();
	const auto result = InitSpellBook();
	ASSERT_TRUE(result.has_value()) << result.error();
	EXPECT_EQ(GetSpellBookPageCount(), 6u);
}

} // namespace
} // namespace devilution
