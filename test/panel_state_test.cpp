/**
 * @file panel_state_test.cpp
 *
 * Tests for the panel state machine — the mutual-exclusion and toggle rules
 * that govern which side panels (inventory, spellbook, character sheet,
 * quest log, stash, visual store) can be open simultaneously.
 *
 * These tests call the same functions the keymapper invokes (or their
 * constituent parts where the top-level function lacks a header declaration).
 * Assertions are purely on boolean panel-state flags, making them resilient
 * to rendering, layout, or widget-tree refactors.
 */

#include <gtest/gtest.h>

#include "ui_test.hpp"

#include "control/control.hpp"
#include "inv.h"
#include "options.h"
#include "panels/spell_list.hpp"
#include "qol/stash.h"
#include "qol/visual_store.h"
#include "quests.h"
#include "stores.h"

namespace devilution {
namespace {

// ---------------------------------------------------------------------------
// The *KeyPressed() functions in diablo.cpp have no header declarations.
// Rather than modifying production headers just for tests, we replicate their
// observable behaviour by toggling the same globals and calling the same
// sub-functions that *are* declared in public headers.
//
// Each helper here is a faithful mirror of the corresponding function in
// diablo.cpp, minus the CanPanelsCoverView()/SetCursorPos() cursor-
// repositioning logic that is irrelevant in headless mode.
// ---------------------------------------------------------------------------

/// Mirror of InventoryKeyPressed() (Source/diablo.cpp).
void DoInventoryKeyPress()
{
	if (IsPlayerInStore())
		return;
	invflag = !invflag;
	SpellbookFlag = false;
	CloseStash();
	if (IsVisualStoreOpen)
		CloseVisualStore();
}

/// Mirror of SpellBookKeyPressed() (Source/diablo.cpp).
void DoSpellBookKeyPress()
{
	if (IsPlayerInStore())
		return;
	SpellbookFlag = !SpellbookFlag;
	CloseInventory(); // closes stash, visual store, and sets invflag=false
}

/// Mirror of CharacterSheetKeyPressed() (Source/diablo.cpp).
void DoCharacterSheetKeyPress()
{
	if (IsPlayerInStore())
		return;
	ToggleCharPanel(); // OpenCharPanel closes quest log, stash, visual store
}

/// Mirror of QuestLogKeyPressed() (Source/diablo.cpp).
void DoQuestLogKeyPress()
{
	if (IsPlayerInStore())
		return;
	if (!QuestLogIsOpen) {
		StartQuestlog();
	} else {
		QuestLogIsOpen = false;
	}
	CloseCharPanel();
	CloseStash();
	if (IsVisualStoreOpen)
		CloseVisualStore();
}

/// Mirror of DisplaySpellsKeyPressed() (Source/diablo.cpp).
void DoDisplaySpellsKeyPress()
{
	if (IsPlayerInStore())
		return;
	CloseCharPanel();
	QuestLogIsOpen = false;
	CloseInventory();
	SpellbookFlag = false;
	if (!SpellSelectFlag) {
		DoSpeedBook();
	} else {
		SpellSelectFlag = false;
	}
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class PanelStateTest : public UITest {
protected:
	void SetUp() override
	{
		UITest::SetUp();

		// Initialise quests so StartQuestlog() doesn't crash.
		InitQuests();
	}
};

// ===== Basic toggle tests =================================================

TEST_F(PanelStateTest, InventoryTogglesOnAndOff)
{
	ASSERT_FALSE(invflag);

	DoInventoryKeyPress();
	EXPECT_TRUE(invflag);

	DoInventoryKeyPress();
	EXPECT_FALSE(invflag);
}

TEST_F(PanelStateTest, SpellBookTogglesOnAndOff)
{
	ASSERT_FALSE(SpellbookFlag);

	DoSpellBookKeyPress();
	EXPECT_TRUE(SpellbookFlag);

	DoSpellBookKeyPress();
	EXPECT_FALSE(SpellbookFlag);
}

TEST_F(PanelStateTest, CharacterSheetTogglesOnAndOff)
{
	ASSERT_FALSE(CharFlag);

	DoCharacterSheetKeyPress();
	EXPECT_TRUE(CharFlag);

	DoCharacterSheetKeyPress();
	EXPECT_FALSE(CharFlag);
}

TEST_F(PanelStateTest, QuestLogTogglesOnAndOff)
{
	ASSERT_FALSE(QuestLogIsOpen);

	DoQuestLogKeyPress();
	EXPECT_TRUE(QuestLogIsOpen);

	DoQuestLogKeyPress();
	EXPECT_FALSE(QuestLogIsOpen);
}

// ===== Right-panel mutual exclusion (inventory vs spellbook) ==============

TEST_F(PanelStateTest, OpeningInventoryClosesSpellBook)
{
	DoSpellBookKeyPress();
	ASSERT_TRUE(SpellbookFlag);

	DoInventoryKeyPress();
	EXPECT_TRUE(invflag);
	EXPECT_FALSE(SpellbookFlag) << "Opening inventory must close spellbook";
}

TEST_F(PanelStateTest, OpeningSpellBookClosesInventory)
{
	DoInventoryKeyPress();
	ASSERT_TRUE(invflag);

	DoSpellBookKeyPress();
	EXPECT_TRUE(SpellbookFlag);
	EXPECT_FALSE(invflag) << "Opening spellbook must close inventory";
}

// ===== Left-panel mutual exclusion (character sheet vs quest log) ==========

TEST_F(PanelStateTest, OpeningCharSheetClosesQuestLog)
{
	DoQuestLogKeyPress();
	ASSERT_TRUE(QuestLogIsOpen);

	DoCharacterSheetKeyPress();
	EXPECT_TRUE(CharFlag);
	EXPECT_FALSE(QuestLogIsOpen) << "Opening character sheet must close quest log";
}

TEST_F(PanelStateTest, OpeningQuestLogClosesCharSheet)
{
	DoCharacterSheetKeyPress();
	ASSERT_TRUE(CharFlag);

	DoQuestLogKeyPress();
	EXPECT_TRUE(QuestLogIsOpen);
	EXPECT_FALSE(CharFlag) << "Opening quest log must close character sheet";
}

// ===== Cross-side independence =============================================
// Left-side panels should NOT close right-side panels and vice versa.

TEST_F(PanelStateTest, InventoryDoesNotCloseCharSheet)
{
	DoCharacterSheetKeyPress();
	ASSERT_TRUE(CharFlag);

	DoInventoryKeyPress();
	EXPECT_TRUE(invflag);
	EXPECT_TRUE(CharFlag) << "Inventory (right) must not close char sheet (left)";
}

TEST_F(PanelStateTest, CharSheetDoesNotCloseInventory)
{
	DoInventoryKeyPress();
	ASSERT_TRUE(invflag);

	DoCharacterSheetKeyPress();
	EXPECT_TRUE(CharFlag);
	EXPECT_TRUE(invflag) << "Char sheet (left) must not close inventory (right)";
}

TEST_F(PanelStateTest, SpellBookDoesNotCloseQuestLog)
{
	DoQuestLogKeyPress();
	ASSERT_TRUE(QuestLogIsOpen);

	DoSpellBookKeyPress();
	EXPECT_TRUE(SpellbookFlag);
	EXPECT_TRUE(QuestLogIsOpen) << "Spellbook (right) must not close quest log (left)";
}

TEST_F(PanelStateTest, QuestLogDoesNotCloseSpellBook)
{
	DoSpellBookKeyPress();
	ASSERT_TRUE(SpellbookFlag);

	DoQuestLogKeyPress();
	EXPECT_TRUE(QuestLogIsOpen);
	EXPECT_TRUE(SpellbookFlag) << "Quest log (left) must not close spellbook (right)";
}

// ===== Both sides open at once =============================================

TEST_F(PanelStateTest, CanOpenBothSidesSimultaneously)
{
	DoInventoryKeyPress();
	DoCharacterSheetKeyPress();

	EXPECT_TRUE(invflag);
	EXPECT_TRUE(CharFlag);
	EXPECT_TRUE(IsRightPanelOpen());
	EXPECT_TRUE(IsLeftPanelOpen());
}

TEST_F(PanelStateTest, SpellBookAndQuestLogBothOpen)
{
	DoSpellBookKeyPress();
	DoQuestLogKeyPress();

	EXPECT_TRUE(SpellbookFlag);
	EXPECT_TRUE(QuestLogIsOpen);
	EXPECT_TRUE(IsRightPanelOpen());
	EXPECT_TRUE(IsLeftPanelOpen());
}

// ===== Rapid cycling =======================================================

TEST_F(PanelStateTest, RapidRightPanelCycling)
{
	DoInventoryKeyPress();
	EXPECT_TRUE(invflag);
	EXPECT_FALSE(SpellbookFlag);

	DoSpellBookKeyPress();
	EXPECT_FALSE(invflag);
	EXPECT_TRUE(SpellbookFlag);

	DoInventoryKeyPress();
	EXPECT_TRUE(invflag);
	EXPECT_FALSE(SpellbookFlag);

	DoSpellBookKeyPress();
	EXPECT_FALSE(invflag);
	EXPECT_TRUE(SpellbookFlag);

	DoSpellBookKeyPress(); // toggle off
	EXPECT_FALSE(invflag);
	EXPECT_FALSE(SpellbookFlag);
}

TEST_F(PanelStateTest, RapidLeftPanelCycling)
{
	DoCharacterSheetKeyPress();
	EXPECT_TRUE(CharFlag);
	EXPECT_FALSE(QuestLogIsOpen);

	DoQuestLogKeyPress();
	EXPECT_FALSE(CharFlag);
	EXPECT_TRUE(QuestLogIsOpen);

	DoCharacterSheetKeyPress();
	EXPECT_TRUE(CharFlag);
	EXPECT_FALSE(QuestLogIsOpen);

	DoQuestLogKeyPress();
	EXPECT_FALSE(CharFlag);
	EXPECT_TRUE(QuestLogIsOpen);

	DoQuestLogKeyPress(); // toggle off
	EXPECT_FALSE(CharFlag);
	EXPECT_FALSE(QuestLogIsOpen);
}

// ===== IsLeftPanelOpen / IsRightPanelOpen helpers ==========================

TEST_F(PanelStateTest, IsLeftPanelOpenReflectsCharFlag)
{
	EXPECT_FALSE(IsLeftPanelOpen());
	DoCharacterSheetKeyPress();
	EXPECT_TRUE(IsLeftPanelOpen());
	DoCharacterSheetKeyPress();
	EXPECT_FALSE(IsLeftPanelOpen());
}

TEST_F(PanelStateTest, IsLeftPanelOpenReflectsQuestLog)
{
	EXPECT_FALSE(IsLeftPanelOpen());
	DoQuestLogKeyPress();
	EXPECT_TRUE(IsLeftPanelOpen());
	DoQuestLogKeyPress();
	EXPECT_FALSE(IsLeftPanelOpen());
}

TEST_F(PanelStateTest, IsRightPanelOpenReflectsInventory)
{
	EXPECT_FALSE(IsRightPanelOpen());
	DoInventoryKeyPress();
	EXPECT_TRUE(IsRightPanelOpen());
	DoInventoryKeyPress();
	EXPECT_FALSE(IsRightPanelOpen());
}

TEST_F(PanelStateTest, IsRightPanelOpenReflectsSpellbook)
{
	EXPECT_FALSE(IsRightPanelOpen());
	DoSpellBookKeyPress();
	EXPECT_TRUE(IsRightPanelOpen());
	DoSpellBookKeyPress();
	EXPECT_FALSE(IsRightPanelOpen());
}

// ===== Stash interactions ==================================================

TEST_F(PanelStateTest, IsLeftPanelOpenReflectsStash)
{
	EXPECT_FALSE(IsLeftPanelOpen());
	IsStashOpen = true;
	EXPECT_TRUE(IsLeftPanelOpen());
	IsStashOpen = false;
	EXPECT_FALSE(IsLeftPanelOpen());
}

TEST_F(PanelStateTest, OpeningInventoryClosesStash)
{
	IsStashOpen = true;
	ASSERT_TRUE(IsLeftPanelOpen());

	DoInventoryKeyPress();
	EXPECT_TRUE(invflag);
	EXPECT_FALSE(IsStashOpen) << "Opening inventory must close stash";
}

TEST_F(PanelStateTest, OpeningQuestLogClosesStash)
{
	IsStashOpen = true;
	ASSERT_TRUE(IsLeftPanelOpen());

	DoQuestLogKeyPress();
	EXPECT_TRUE(QuestLogIsOpen);
	EXPECT_FALSE(IsStashOpen) << "Opening quest log must close stash";
}

TEST_F(PanelStateTest, OpeningCharSheetClosesStash)
{
	IsStashOpen = true;
	ASSERT_TRUE(IsLeftPanelOpen());

	DoCharacterSheetKeyPress();
	EXPECT_TRUE(CharFlag);
	EXPECT_FALSE(IsStashOpen) << "Opening character sheet must close stash";
}

// ===== Store blocks panel toggling =========================================

TEST_F(PanelStateTest, InventoryBlockedWhileInStore)
{
	ActiveStore = TalkID::Smith;
	ASSERT_TRUE(IsPlayerInStore());

	DoInventoryKeyPress();
	EXPECT_FALSE(invflag) << "Inventory toggle must be blocked while in store";
}

TEST_F(PanelStateTest, SpellBookBlockedWhileInStore)
{
	ActiveStore = TalkID::Witch;
	ASSERT_TRUE(IsPlayerInStore());

	DoSpellBookKeyPress();
	EXPECT_FALSE(SpellbookFlag) << "Spellbook toggle must be blocked while in store";
}

TEST_F(PanelStateTest, CharSheetBlockedWhileInStore)
{
	ActiveStore = TalkID::Healer;
	ASSERT_TRUE(IsPlayerInStore());

	DoCharacterSheetKeyPress();
	EXPECT_FALSE(CharFlag) << "Char sheet toggle must be blocked while in store";
}

TEST_F(PanelStateTest, QuestLogBlockedWhileInStore)
{
	ActiveStore = TalkID::Storyteller;
	ASSERT_TRUE(IsPlayerInStore());

	DoQuestLogKeyPress();
	EXPECT_FALSE(QuestLogIsOpen) << "Quest log toggle must be blocked while in store";
}

TEST_F(PanelStateTest, DisplaySpellsBlockedWhileInStore)
{
	ActiveStore = TalkID::Smith;
	ASSERT_TRUE(IsPlayerInStore());

	DoDisplaySpellsKeyPress();
	// The key observation is that nothing should change — no panels opened.
	EXPECT_FALSE(SpellSelectFlag);
}

// ===== DisplaySpells (speed book) ==========================================

TEST_F(PanelStateTest, DisplaySpellsClosesAllPanels)
{
	DoInventoryKeyPress();
	DoCharacterSheetKeyPress();
	ASSERT_TRUE(invflag);
	ASSERT_TRUE(CharFlag);

	DoDisplaySpellsKeyPress();
	EXPECT_FALSE(invflag) << "Display spells must close inventory";
	EXPECT_FALSE(SpellbookFlag) << "Display spells must close spellbook";
	EXPECT_FALSE(CharFlag) << "Display spells must close character sheet";
	EXPECT_FALSE(QuestLogIsOpen) << "Display spells must close quest log";
}

// ===== Complex multi-step scenarios ========================================

TEST_F(PanelStateTest, FullPanelWorkflow)
{
	// Open inventory
	DoInventoryKeyPress();
	EXPECT_TRUE(invflag);
	EXPECT_FALSE(SpellbookFlag);

	// Open character sheet alongside
	DoCharacterSheetKeyPress();
	EXPECT_TRUE(invflag);
	EXPECT_TRUE(CharFlag);

	// Switch right panel to spellbook (closes inventory)
	DoSpellBookKeyPress();
	EXPECT_FALSE(invflag);
	EXPECT_TRUE(SpellbookFlag);
	EXPECT_TRUE(CharFlag); // left panel unaffected

	// Switch left panel to quest log (closes char sheet)
	DoQuestLogKeyPress();
	EXPECT_TRUE(SpellbookFlag); // right panel unaffected
	EXPECT_FALSE(CharFlag);
	EXPECT_TRUE(QuestLogIsOpen);

	// Close everything with display spells
	DoDisplaySpellsKeyPress();
	EXPECT_FALSE(invflag);
	EXPECT_FALSE(SpellbookFlag);
	EXPECT_FALSE(CharFlag);
	EXPECT_FALSE(QuestLogIsOpen);
}

TEST_F(PanelStateTest, StorePreventsAllToggles)
{
	ActiveStore = TalkID::SmithBuy;
	ASSERT_TRUE(IsPlayerInStore());

	DoInventoryKeyPress();
	DoSpellBookKeyPress();
	DoCharacterSheetKeyPress();
	DoQuestLogKeyPress();
	DoDisplaySpellsKeyPress();

	EXPECT_FALSE(invflag);
	EXPECT_FALSE(SpellbookFlag);
	EXPECT_FALSE(CharFlag);
	EXPECT_FALSE(QuestLogIsOpen);
	EXPECT_FALSE(SpellSelectFlag);
}

TEST_F(PanelStateTest, PanelsWorkAfterStoreCloses)
{
	// Open store, try to open panels (should be blocked), then close store.
	ActiveStore = TalkID::Smith;
	DoInventoryKeyPress();
	EXPECT_FALSE(invflag);

	// Close the store.
	ActiveStore = TalkID::None;
	ASSERT_FALSE(IsPlayerInStore());

	// Now panels should work again.
	DoInventoryKeyPress();
	EXPECT_TRUE(invflag);

	DoCharacterSheetKeyPress();
	EXPECT_TRUE(CharFlag);
}

// ===== Edge cases ==========================================================

TEST_F(PanelStateTest, NoPanelsOpenInitially)
{
	EXPECT_FALSE(invflag);
	EXPECT_FALSE(SpellbookFlag);
	EXPECT_FALSE(CharFlag);
	EXPECT_FALSE(QuestLogIsOpen);
	EXPECT_FALSE(SpellSelectFlag);
	EXPECT_FALSE(IsStashOpen);
	EXPECT_FALSE(IsVisualStoreOpen);
	EXPECT_FALSE(IsLeftPanelOpen());
	EXPECT_FALSE(IsRightPanelOpen());
}

TEST_F(PanelStateTest, ToggleSamePanelTwiceReturnsToOriginal)
{
	// Double-toggle each panel — should end up closed.
	DoInventoryKeyPress();
	DoInventoryKeyPress();
	EXPECT_FALSE(invflag);

	DoSpellBookKeyPress();
	DoSpellBookKeyPress();
	EXPECT_FALSE(SpellbookFlag);

	DoCharacterSheetKeyPress();
	DoCharacterSheetKeyPress();
	EXPECT_FALSE(CharFlag);

	DoQuestLogKeyPress();
	DoQuestLogKeyPress();
	EXPECT_FALSE(QuestLogIsOpen);
}

TEST_F(PanelStateTest, OpeningSpellBookClosesStash)
{
	IsStashOpen = true;
	ASSERT_TRUE(IsLeftPanelOpen());

	// Spellbook is a right-side panel, but CloseInventory() (called inside
	// SpellBookKeyPressed) also closes the stash.
	DoSpellBookKeyPress();
	EXPECT_TRUE(SpellbookFlag);
	EXPECT_FALSE(IsStashOpen) << "Opening spellbook calls CloseInventory which closes stash";
}

TEST_F(PanelStateTest, MultipleStoreTypesAllBlockPanels)
{
	// Verify a selection of different TalkID values all count as "in store".
	const TalkID stores[] = {
		TalkID::Smith,
		TalkID::SmithBuy,
		TalkID::SmithSell,
		TalkID::SmithRepair,
		TalkID::Witch,
		TalkID::WitchBuy,
		TalkID::Boy,
		TalkID::BoyBuy,
		TalkID::Healer,
		TalkID::HealerBuy,
		TalkID::Storyteller,
		TalkID::StorytellerIdentify,
		TalkID::SmithPremiumBuy,
		TalkID::Confirm,
		TalkID::NoMoney,
		TalkID::NoRoom,
		TalkID::Gossip,
		TalkID::Tavern,
		TalkID::Drunk,
		TalkID::Barmaid,
	};

	for (TalkID store : stores) {
		CloseAllPanels();
		ActiveStore = store;
		ASSERT_TRUE(IsPlayerInStore())
		    << "TalkID value " << static_cast<int>(store) << " should count as in-store";

		DoInventoryKeyPress();
		EXPECT_FALSE(invflag)
		    << "Inventory should be blocked for TalkID " << static_cast<int>(store);

		ActiveStore = TalkID::None;
	}
}

} // namespace
} // namespace devilution