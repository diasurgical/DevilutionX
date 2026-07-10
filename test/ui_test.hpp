/**
 * @file ui_test.hpp
 *
 * Shared test fixture for UI domain tests.
 *
 * Provides a fully-initialised single-player game state with a level-25 Warrior
 * who has plenty of gold. All panels are closed before and after each test.
 *
 * Tests that need MPQ assets skip gracefully when they are not available.
 */
#pragma once

#include <algorithm>

#include <gtest/gtest.h>

#include "control/control.hpp"
#include "controls/control_mode.hpp"
#include "cursor.h"
#include "engine/assets.hpp"
#include "inv.h"
#include "items.h"
#include "minitext.h"
#include "options.h"
#include "player.h"
#include "qol/stash.h"
#include "qol/visual_store.h"
#include "quests.h"
#include "stores.h"
#include "storm/storm_net.hpp"
#include "tables/itemdat.h"
#include "tables/playerdat.hpp"
#include "tables/spelldat.h"

namespace devilution {

constexpr const char UITestMissingMpqMsg[] = "MPQ assets (spawn.mpq or DIABDAT.MPQ) not found - skipping test suite";

/**
 * @brief Base test fixture that boots enough of the engine for UI-level tests.
 *
 * Usage:
 *   class MyTest : public UITest { ... };
 *   TEST_F(MyTest, SomeScenario) { ... }
 *
 * The fixture guarantees:
 *  - A single-player game with one Warrior at character level 25 and 100 000 gold.
 *  - ControlMode pinned to KeyboardAndMouse (tests are deterministic regardless of host input).
 *  - All panels / stores / overlays closed before and after each test.
 *  - visualStoreUI option disabled so we always exercise the text-based store path.
 *  - A loopback network provider (needed by functions that send net commands).
 */
class UITest : public ::testing::Test {
protected:
	/* ---- once per test suite ---- */

	static void SetUpTestSuite()
	{
		LoadCoreArchives();
		LoadGameArchives();

		missingAssets_ = !HaveMainData();
		if (missingAssets_)
			return;

		LoadPlayerDataFiles();
		LoadItemData();
		LoadSpellData();
		LoadQuestData();
		// Note: do NOT call InitCursor() here. CreatePlayer() calls
		// CreatePlrItems() which does its own InitCursor()/FreeCursor() cycle.
	}

	/* ---- every test ---- */

	void SetUp() override
	{
		if (missingAssets_) {
			GTEST_SKIP() << UITestMissingMpqMsg;
		}

		Players.resize(1);
		MyPlayer = &Players[0];

		// Ensure we are in single-player Diablo mode.
		gbIsHellfire = false;
		gbIsMultiplayer = false;

		CreatePlayer(*MyPlayer, HeroClass::Warrior);
		MyPlayer->setCharacterLevel(25);

		// CreatePlayer() calls CreatePlrItems() which does InitCursor()/FreeCursor().
		// Re-initialise cursors because store operations (StoreAutoPlace, etc.)
		// need cursor sprite data to determine item sizes.
		InitCursor();

		// Give the player a generous amount of gold, distributed as inventory gold piles.
		SetPlayerGold(100000);

		// Initialise stash with some gold too so stash-related paths work.
		Stash = {};
		Stash.gold = 0;

		// Pin the control mode so behaviour is deterministic.
		ControlMode = ControlTypes::KeyboardAndMouse;

		// Always use the text-based store path so we can drive its state machine.
		GetOptions().Gameplay.visualStoreUI.SetValue(false);
		GetOptions().Gameplay.showItemGraphicsInStores.SetValue(false);

		// Close everything that might be open.
		CloseAllPanels();

		// Set up loopback networking (needed for inventory mutations that send net commands).
		SNetInitializeProvider(SELCONN_LOOPBACK, nullptr);

		// Clear store state.
		InitStores();

		// Make sure quest-text overlay is off.
		qtextflag = false;
	}

	void TearDown() override
	{
		CloseAllPanels();
		ActiveStore = TalkID::None;
		FreeCursor();
	}

	/* ---- helpers ---- */

	/**
	 * @brief Close every panel / overlay without relying on ClosePanels()
	 * (which has no header declaration and does cursor-position side-effects
	 * that are undesirable in headless tests).
	 */
	static void CloseAllPanels()
	{
		invflag = false;
		SpellbookFlag = false;
		CharFlag = false;
		QuestLogIsOpen = false;
		SpellSelectFlag = false;
		IsStashOpen = false;
		if (IsVisualStoreOpen) {
			IsVisualStoreOpen = false;
		}
		ActiveStore = TalkID::None;
	}

	/**
	 * @brief Give the player exactly @p amount gold, placed as inventory gold piles.
	 */
	static void SetPlayerGold(int amount)
	{
		// Clear existing gold piles.
		for (int i = 0; i < MyPlayer->_pNumInv; ++i) {
			if (MyPlayer->InvList[i]._itype == ItemType::Gold) {
				MyPlayer->InvList[i].clear();
			}
		}

		MyPlayer->_pGold = 0;

		if (amount <= 0)
			return;

		// Place gold in a single pile (up to GOLD_MAX_LIMIT per pile).
		int remaining = amount;
		int slot = 0;
		while (remaining > 0 && slot < InventoryGridCells) {
			int pileSize = std::min(remaining, static_cast<int>(GOLD_MAX_LIMIT));
			MyPlayer->InvList[slot]._itype = ItemType::Gold;
			MyPlayer->InvList[slot]._ivalue = pileSize;
			MyPlayer->InvGrid[slot] = static_cast<int8_t>(slot + 1);
			remaining -= pileSize;
			slot++;
		}
		MyPlayer->_pNumInv = slot;
		MyPlayer->_pGold = amount;
	}

	/**
	 * @brief Clear the player's inventory completely (items + grid + gold).
	 */
	static void ClearInventory()
	{
		for (int i = 0; i < InventoryGridCells; i++) {
			MyPlayer->InvList[i] = {};
			MyPlayer->InvGrid[i] = 0;
		}
		MyPlayer->_pNumInv = 0;
		MyPlayer->_pGold = 0;
	}

	/**
	 * @brief Clear the player's belt.
	 */
	static void ClearBelt()
	{
		for (int i = 0; i < MaxBeltItems; i++) {
			MyPlayer->SpdList[i].clear();
		}
	}

	/**
	 * @brief Clear all equipment slots.
	 */
	static void ClearEquipment()
	{
		for (auto &bodyItem : MyPlayer->InvBody) {
			bodyItem = {};
		}
	}

	/**
	 * @brief Completely strip the player of all items and gold.
	 */
	void StripPlayer()
	{
		ClearInventory();
		ClearBelt();
		ClearEquipment();
		MyPlayer->_pGold = 0;
	}

	/**
	 * @brief Place a simple item in the player's inventory at the first free slot.
	 * @return The inventory index where the item was placed, or -1 on failure.
	 */
	int PlaceItemInInventory(const Item &item)
	{
		int idx = MyPlayer->_pNumInv;
		if (idx >= InventoryGridCells)
			return -1;
		MyPlayer->InvList[idx] = item;
		MyPlayer->InvGrid[idx] = static_cast<int8_t>(idx + 1);
		MyPlayer->_pNumInv = idx + 1;
		return idx;
	}

private:
	static bool missingAssets_;
};

// Static member definition — must appear in exactly one translation unit.
// Since this is a header, we use `inline` (C++17).
inline bool UITest::missingAssets_ = false;

} // namespace devilution