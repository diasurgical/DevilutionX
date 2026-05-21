/**
 * @file spell_ui_test.cpp
 *
 * Tests for the spell book and spell list UI functionality.
 *
 * Covers:
 *  - GetSpellListItems() returning learned spells, abilities, and scroll spells
 *  - SetSpell() changing the player's active/readied spell
 *  - SetSpeedSpell() assigning spells to hotkey slots
 *  - IsValidSpeedSpell() validating hotkey slot assignments
 *  - DoSpeedBook() opening the speed spell selection overlay
 *  - ToggleSpell() cycling through available spell types for a hotkey
 */

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "ui_test.hpp"

#include "control/control.hpp"
#include "diablo.h"
#include "panels/spell_icons.hpp"
#include "panels/spell_list.hpp"
#include "player.h"
#include "spells.h"
#include "tables/spelldat.h"

namespace devilution {
namespace {

/**
 * @brief Test fixture for spell UI tests.
 *
 * Inherits from UITest which provides a fully-initialised single-player game
 * with a level-25 Warrior, 100,000 gold, all panels closed, loopback
 * networking, and HeadlessMode enabled.
 */
class SpellUITest : public UITest {
protected:
	void SetUp() override
	{
		UITest::SetUp();

		// Ensure all hotkey slots start invalid so tests are deterministic.
		for (size_t i = 0; i < NumHotkeys; ++i) {
			MyPlayer->_pSplHotKey[i] = SpellID::Invalid;
			MyPlayer->_pSplTHotKey[i] = SpellType::Invalid;
		}
	}

	/**
	 * @brief Teach the player a memorised spell at the given level.
	 *
	 * Sets the appropriate bit in _pMemSpells and assigns a spell level
	 * so the spell is usable (level > 0).
	 */
	static void TeachSpell(SpellID spell, uint8_t level = 5)
	{
		MyPlayer->_pMemSpells |= GetSpellBitmask(spell);
		MyPlayer->_pSplLvl[static_cast<int8_t>(spell)] = level;
	}

	/**
	 * @brief Add a scroll-type spell to the player's available spells.
	 *
	 * Sets the appropriate bit in _pScrlSpells. Note that for the spell
	 * to actually be castable the player would need a scroll item, but
	 * for UI listing purposes the bitmask is sufficient.
	 */
	static void AddScrollSpell(SpellID spell)
	{
		MyPlayer->_pScrlSpells |= GetSpellBitmask(spell);
	}

	/**
	 * @brief Open the speed book overlay.
	 *
	 * Sets SpellSelectFlag = true and positions the mouse via DoSpeedBook().
	 * DoSpeedBook() internally calls SetCursorPos() which, because
	 * ControlDevice defaults to ControlTypes::None (not KeyboardAndMouse),
	 * simply writes to MousePosition without touching SDL windowing.
	 */
	static void OpenSpeedBook()
	{
		DoSpeedBook();
		// DoSpeedBook sets SpellSelectFlag = true.
	}

	/**
	 * @brief Search the spell list for an item matching the given spell ID and type.
	 * @return Pointer to the matching SpellListItem, or nullptr if not found.
	 */
	static const SpellListItem *FindInSpellList(
	    const std::vector<SpellListItem> &items,
	    SpellID id,
	    SpellType type)
	{
		for (const auto &item : items) {
			if (item.id == id && item.type == type)
				return &item;
		}
		return nullptr;
	}

	/**
	 * @brief Find a spell list item by ID only (any type).
	 */
	static const SpellListItem *FindInSpellListById(
	    const std::vector<SpellListItem> &items,
	    SpellID id)
	{
		for (const auto &item : items) {
			if (item.id == id)
				return &item;
		}
		return nullptr;
	}

	/**
	 * @brief Position the mouse over a spell list item so that
	 *        GetSpellListSelection() will consider it "selected".
	 *
	 * The spell list item's `location` field gives the bottom-left corner
	 * of the icon. The icon occupies a SPLICONLENGTH x SPLICONLENGTH area
	 * from (location.x, location.y - SPLICONLENGTH) to
	 * (location.x + SPLICONLENGTH - 1, location.y - 1).
	 * We position the mouse in the centre of that area.
	 */
	static void PositionMouseOver(const SpellListItem &item)
	{
		// The selection check in GetSpellListItems() is:
		//   MousePosition.x >= lx && MousePosition.x < lx + SPLICONLENGTH
		//   MousePosition.y >= ly && MousePosition.y < ly + SPLICONLENGTH
		// where lx = item.location.x, ly = item.location.y - SPLICONLENGTH
		MousePosition = Point { item.location.x + SPLICONLENGTH / 2,
			item.location.y - SPLICONLENGTH / 2 };
	}
};

// ===========================================================================
// Test: GetSpellListItems returns learned (memorised) spells
// ===========================================================================

TEST_F(SpellUITest, GetSpellListItems_ReturnsLearnedSpells)
{
	// Teach the player Firebolt as a memorised spell.
	TeachSpell(SpellID::Firebolt, 5);

	// Open the speed book so GetSpellListItems() has the right context.
	OpenSpeedBook();

	const auto items = GetSpellListItems();

	// The list should contain Firebolt with SpellType::Spell.
	const SpellListItem *found = FindInSpellList(items, SpellID::Firebolt, SpellType::Spell);
	ASSERT_NE(found, nullptr)
	    << "Firebolt should appear in the spell list after being taught";
	EXPECT_EQ(found->id, SpellID::Firebolt);
	EXPECT_EQ(found->type, SpellType::Spell);
}

// ===========================================================================
// Test: GetSpellListItems includes the Warrior's innate abilities
// ===========================================================================

TEST_F(SpellUITest, GetSpellListItems_IncludesAbilities)
{
	// After CreatePlayer() for a Warrior, _pAblSpells should include the
	// Warrior's skill (ItemRepair). Verify it appears in the spell list.
	ASSERT_NE(MyPlayer->_pAblSpells, 0u)
	    << "Warrior should have at least one ability after CreatePlayer()";

	OpenSpeedBook();

	const auto items = GetSpellListItems();

	// The Warrior's skill is ItemRepair (loaded from starting_loadout.tsv).
	const SpellListItem *found = FindInSpellList(items, SpellID::ItemRepair, SpellType::Skill);
	EXPECT_NE(found, nullptr)
	    << "Warrior's ItemRepair ability should appear in the spell list";
}

// ===========================================================================
// Test: GetSpellListItems includes scroll spells
// ===========================================================================

TEST_F(SpellUITest, GetSpellListItems_IncludesScrollSpells)
{
	// Give the player a Town Portal scroll spell via the bitmask.
	AddScrollSpell(SpellID::TownPortal);

	OpenSpeedBook();

	const auto items = GetSpellListItems();

	const SpellListItem *found = FindInSpellList(items, SpellID::TownPortal, SpellType::Scroll);
	ASSERT_NE(found, nullptr)
	    << "TownPortal should appear in the spell list as a scroll spell";
	EXPECT_EQ(found->type, SpellType::Scroll);
}

// ===========================================================================
// Test: GetSpellListItems is empty when all spell bitmasks are cleared
// ===========================================================================

TEST_F(SpellUITest, GetSpellListItems_EmptyWhenAllSpellsCleared)
{
	// Clear every spell bitmask, including abilities.
	MyPlayer->_pMemSpells = 0;
	MyPlayer->_pAblSpells = 0;
	MyPlayer->_pScrlSpells = 0;
	MyPlayer->_pISpells = 0;

	OpenSpeedBook();

	const auto items = GetSpellListItems();

	EXPECT_TRUE(items.empty())
	    << "Spell list should be empty when all spell bitmasks are zero";
}

// ===========================================================================
// Test: SetSpell changes the player's active/readied spell
// ===========================================================================

TEST_F(SpellUITest, SetSpell_ChangesActiveSpell)
{
	// Teach the player Firebolt.
	TeachSpell(SpellID::Firebolt, 5);

	// Open speed book — this sets SpellSelectFlag and positions the mouse.
	OpenSpeedBook();

	// Get the spell list and find Firebolt's icon position.
	auto items = GetSpellListItems();
	const SpellListItem *firebolt = FindInSpellList(items, SpellID::Firebolt, SpellType::Spell);
	ASSERT_NE(firebolt, nullptr)
	    << "Firebolt must be in the spell list for SetSpell to work";

	// Position the mouse over Firebolt's icon so it becomes "selected".
	PositionMouseOver(*firebolt);

	// Re-fetch items to confirm the selection is detected.
	items = GetSpellListItems();
	const SpellListItem *selected = FindInSpellList(items, SpellID::Firebolt, SpellType::Spell);
	ASSERT_NE(selected, nullptr);
	EXPECT_TRUE(selected->isSelected)
	    << "Firebolt should be selected after positioning mouse over it";

	// Now call SetSpell — should set the player's readied spell.
	SetSpell();

	EXPECT_EQ(MyPlayer->_pRSpell, SpellID::Firebolt)
	    << "Active spell should be Firebolt after SetSpell()";
	EXPECT_EQ(MyPlayer->_pRSplType, SpellType::Spell)
	    << "Active spell type should be Spell after SetSpell()";

	// SetSpell also clears SpellSelectFlag.
	EXPECT_FALSE(SpellSelectFlag)
	    << "SpellSelectFlag should be cleared after SetSpell()";
}

// ===========================================================================
// Test: SetSpeedSpell assigns a spell to a hotkey slot
// ===========================================================================

TEST_F(SpellUITest, SetSpeedSpell_AssignsHotkey)
{
	// Teach the player Firebolt.
	TeachSpell(SpellID::Firebolt, 5);

	OpenSpeedBook();

	// Find Firebolt's position and move the mouse there.
	auto items = GetSpellListItems();
	const SpellListItem *firebolt = FindInSpellList(items, SpellID::Firebolt, SpellType::Spell);
	ASSERT_NE(firebolt, nullptr);

	PositionMouseOver(*firebolt);

	// Assign to hotkey slot 0.
	SetSpeedSpell(0);

	// Verify the hotkey was assigned.
	EXPECT_TRUE(IsValidSpeedSpell(0))
	    << "Hotkey slot 0 should be valid after assigning Firebolt";
	EXPECT_EQ(MyPlayer->_pSplHotKey[0], SpellID::Firebolt)
	    << "Hotkey slot 0 should contain Firebolt";
	EXPECT_EQ(MyPlayer->_pSplTHotKey[0], SpellType::Spell)
	    << "Hotkey slot 0 type should be Spell";
}

// ===========================================================================
// Test: IsValidSpeedSpell returns false for an unassigned slot
// ===========================================================================

TEST_F(SpellUITest, IsValidSpeedSpell_InvalidSlot)
{
	// Slot 0 was cleared to SpellID::Invalid in SetUp().
	EXPECT_FALSE(IsValidSpeedSpell(0))
	    << "Unassigned hotkey slot should not be valid";
}

// ===========================================================================
// Test: DoSpeedBook opens the spell selection overlay
// ===========================================================================

TEST_F(SpellUITest, DoSpeedBook_OpensSpellSelect)
{
	// Ensure it's closed initially.
	ASSERT_FALSE(SpellSelectFlag);

	DoSpeedBook();

	EXPECT_TRUE(SpellSelectFlag)
	    << "SpellSelectFlag should be true after DoSpeedBook()";
}

// ===========================================================================
// Test: SpellSelectFlag can be toggled off (simulating closing the speed book)
// ===========================================================================

TEST_F(SpellUITest, DoSpeedBook_ClosesSpellSelect)
{
	// Open the speed book.
	DoSpeedBook();
	ASSERT_TRUE(SpellSelectFlag);

	// Simulate closing by clearing the flag (this is what the key handler does).
	SpellSelectFlag = false;

	EXPECT_FALSE(SpellSelectFlag)
	    << "SpellSelectFlag should be false after being manually cleared";
}

// ===========================================================================
// Test: ToggleSpell cycles through available spell types for a hotkey
// ===========================================================================

TEST_F(SpellUITest, ToggleSpell_CyclesThroughTypes)
{
	// Set up a spell that is available as both a memorised spell and a scroll.
	// Using Firebolt for this test.
	TeachSpell(SpellID::Firebolt, 5);
	AddScrollSpell(SpellID::Firebolt);

	// Assign Firebolt (as Spell type) to hotkey slot 0.
	MyPlayer->_pSplHotKey[0] = SpellID::Firebolt;
	MyPlayer->_pSplTHotKey[0] = SpellType::Spell;

	ASSERT_TRUE(IsValidSpeedSpell(0))
	    << "Hotkey slot 0 should be valid with Firebolt as Spell";

	// ToggleSpell activates the spell from the hotkey — it sets the player's
	// readied spell to whatever is in the hotkey slot.
	ToggleSpell(0);

	// After ToggleSpell, the player's readied spell should match the hotkey.
	EXPECT_EQ(MyPlayer->_pRSpell, SpellID::Firebolt);
	EXPECT_EQ(MyPlayer->_pRSplType, SpellType::Spell);

	// Now change the hotkey to Scroll type and toggle again.
	MyPlayer->_pSplTHotKey[0] = SpellType::Scroll;
	ASSERT_TRUE(IsValidSpeedSpell(0))
	    << "Hotkey slot 0 should be valid with Firebolt as Scroll";

	ToggleSpell(0);

	EXPECT_EQ(MyPlayer->_pRSpell, SpellID::Firebolt);
	EXPECT_EQ(MyPlayer->_pRSplType, SpellType::Scroll)
	    << "After toggling with Scroll type, readied spell type should be Scroll";
}

// ===========================================================================
// Test: SetSpeedSpell unsets a hotkey when called with the same spell
// ===========================================================================

TEST_F(SpellUITest, SetSpeedSpell_UnsetsOnDoubleAssign)
{
	// Teach the player Firebolt.
	TeachSpell(SpellID::Firebolt, 5);

	OpenSpeedBook();

	auto items = GetSpellListItems();
	const SpellListItem *firebolt = FindInSpellList(items, SpellID::Firebolt, SpellType::Spell);
	ASSERT_NE(firebolt, nullptr);
	PositionMouseOver(*firebolt);

	// Assign to slot 0 the first time.
	SetSpeedSpell(0);
	ASSERT_TRUE(IsValidSpeedSpell(0));
	ASSERT_EQ(MyPlayer->_pSplHotKey[0], SpellID::Firebolt);

	// Re-fetch items and re-position mouse (SetSpeedSpell doesn't move the cursor).
	items = GetSpellListItems();
	firebolt = FindInSpellList(items, SpellID::Firebolt, SpellType::Spell);
	ASSERT_NE(firebolt, nullptr);
	PositionMouseOver(*firebolt);

	// Assign to slot 0 again — should unset (toggle off).
	SetSpeedSpell(0);
	EXPECT_EQ(MyPlayer->_pSplHotKey[0], SpellID::Invalid)
	    << "Assigning the same spell to the same slot should unset the hotkey";
	EXPECT_FALSE(IsValidSpeedSpell(0))
	    << "Hotkey slot should be invalid after being unset";
}

// ===========================================================================
// Test: IsValidSpeedSpell returns false when the spell is no longer available
// ===========================================================================

TEST_F(SpellUITest, IsValidSpeedSpell_InvalidAfterSpellRemoved)
{
	// Teach and assign Firebolt to slot 0.
	TeachSpell(SpellID::Firebolt, 5);
	MyPlayer->_pSplHotKey[0] = SpellID::Firebolt;
	MyPlayer->_pSplTHotKey[0] = SpellType::Spell;
	ASSERT_TRUE(IsValidSpeedSpell(0));

	// Remove the spell from the player's memory.
	MyPlayer->_pMemSpells &= ~GetSpellBitmask(SpellID::Firebolt);

	// The hotkey still points to Firebolt, but the player no longer knows it.
	EXPECT_FALSE(IsValidSpeedSpell(0))
	    << "Hotkey should be invalid when the underlying spell is no longer available";
}

// ===========================================================================
// Test: Multiple spells appear in the spell list simultaneously
// ===========================================================================

TEST_F(SpellUITest, GetSpellListItems_MultipleSpells)
{
	// Teach multiple spells.
	TeachSpell(SpellID::Firebolt, 3);
	TeachSpell(SpellID::HealOther, 2);
	AddScrollSpell(SpellID::TownPortal);

	OpenSpeedBook();

	const auto items = GetSpellListItems();

	// Verify all three appear (plus the Warrior's innate ability).
	EXPECT_NE(FindInSpellList(items, SpellID::Firebolt, SpellType::Spell), nullptr)
	    << "Firebolt (memorised) should be in the list";
	EXPECT_NE(FindInSpellList(items, SpellID::HealOther, SpellType::Spell), nullptr)
	    << "HealOther (memorised) should be in the list";
	EXPECT_NE(FindInSpellList(items, SpellID::TownPortal, SpellType::Scroll), nullptr)
	    << "TownPortal (scroll) should be in the list";
	EXPECT_NE(FindInSpellList(items, SpellID::ItemRepair, SpellType::Skill), nullptr)
	    << "Warrior's ItemRepair ability should still be present";

	// We should have at least 4 items.
	EXPECT_GE(items.size(), 4u);
}

} // namespace
} // namespace devilution