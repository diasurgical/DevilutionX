/**
 * @file char_panel_test.cpp
 *
 * Tests for the character panel (stat display and stat point allocation).
 *
 * Covers: open/close/toggle, stat point allocation via CheckChrBtns +
 * ReleaseChrBtns, no-allocation when stat points are zero, and stat
 * value consistency checks.
 *
 * All assertions are on game state (CharFlag, stat values, stat points).
 * No assertions on rendering, pixel positions, or widget layout.
 *
 * NOTE: The actual stat increase (e.g. +1 Strength) is applied via
 * NetSendCmdParam1 → loopback → OnAddStrength → ModifyPlrStr. With
 * SELCONN_LOOPBACK the message is queued but not processed synchronously
 * (there is no message pump in the test harness). Therefore stat
 * allocation tests verify the LOCAL side-effects — _pStatPts decreasing
 * and CheckChrBtns/ReleaseChrBtns flow — rather than the final stat value.
 */

#include <gtest/gtest.h>

#include "ui_test.hpp"

#include "control/control.hpp"
#include "control/control_panel.hpp"
#include "diablo.h"
#include "player.h"

namespace devilution {
namespace {

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class CharPanelTest : public UITest {
protected:
	void SetUp() override
	{
		UITest::SetUp();

		// Reset stat allocation state.
		CharPanelButtonActive = false;
		for (int i = 0; i < 4; i++)
			CharPanelButton[i] = false;
	}

	/**
	 * @brief Simulate pressing and releasing a stat button for the given
	 *        attribute.
	 *
	 * Positions MousePosition inside the adjusted button rect, calls
	 * CheckChrBtns() to "press", then ReleaseChrBtns() to "release"
	 * (which triggers the stat decrease locally and sends a net command).
	 */
	void ClickStatButton(CharacterAttribute attribute)
	{
		auto buttonId = static_cast<size_t>(attribute);
		Rectangle button = CharPanelButtonRect[buttonId];
		SetPanelObjectPosition(UiPanels::Character, button);

		// Position mouse in the centre of the button.
		MousePosition = Point {
			button.position.x + button.size.width / 2,
			button.position.y + button.size.height / 2
		};

		CheckChrBtns();
		ReleaseChrBtns(false);
	}
};

// ===========================================================================
// Open / Close / Toggle
// ===========================================================================

TEST_F(CharPanelTest, Open_SetsCharFlag)
{
	ASSERT_FALSE(CharFlag);

	OpenCharPanel();

	EXPECT_TRUE(CharFlag);
}

TEST_F(CharPanelTest, Close_ClearsCharFlag)
{
	OpenCharPanel();
	ASSERT_TRUE(CharFlag);

	CloseCharPanel();

	EXPECT_FALSE(CharFlag);
}

TEST_F(CharPanelTest, Toggle_OpensWhenClosed)
{
	ASSERT_FALSE(CharFlag);

	ToggleCharPanel();

	EXPECT_TRUE(CharFlag);
}

TEST_F(CharPanelTest, Toggle_ClosesWhenOpen)
{
	OpenCharPanel();
	ASSERT_TRUE(CharFlag);

	ToggleCharPanel();

	EXPECT_FALSE(CharFlag);
}

TEST_F(CharPanelTest, Toggle_DoubleToggle_ReturnsToClosed)
{
	ASSERT_FALSE(CharFlag);

	ToggleCharPanel();
	ToggleCharPanel();

	EXPECT_FALSE(CharFlag);
}

// ===========================================================================
// Stat point allocation — verify _pStatPts decrease (local effect)
// ===========================================================================

TEST_F(CharPanelTest, StatAllocation_StrengthDecreasesStatPoints)
{
	MyPlayer->_pStatPts = 5;
	const int ptsBefore = MyPlayer->_pStatPts;

	ClickStatButton(CharacterAttribute::Strength);

	EXPECT_EQ(MyPlayer->_pStatPts, ptsBefore - 1)
	    << "Stat points should decrease by 1 after allocating to Strength";
}

TEST_F(CharPanelTest, StatAllocation_MagicDecreasesStatPoints)
{
	MyPlayer->_pStatPts = 5;
	const int ptsBefore = MyPlayer->_pStatPts;

	ClickStatButton(CharacterAttribute::Magic);

	EXPECT_EQ(MyPlayer->_pStatPts, ptsBefore - 1)
	    << "Stat points should decrease by 1 after allocating to Magic";
}

TEST_F(CharPanelTest, StatAllocation_DexterityDecreasesStatPoints)
{
	MyPlayer->_pStatPts = 5;
	const int ptsBefore = MyPlayer->_pStatPts;

	ClickStatButton(CharacterAttribute::Dexterity);

	EXPECT_EQ(MyPlayer->_pStatPts, ptsBefore - 1)
	    << "Stat points should decrease by 1 after allocating to Dexterity";
}

TEST_F(CharPanelTest, StatAllocation_VitalityDecreasesStatPoints)
{
	MyPlayer->_pStatPts = 5;
	const int ptsBefore = MyPlayer->_pStatPts;

	ClickStatButton(CharacterAttribute::Vitality);

	EXPECT_EQ(MyPlayer->_pStatPts, ptsBefore - 1)
	    << "Stat points should decrease by 1 after allocating to Vitality";
}

TEST_F(CharPanelTest, StatAllocation_CheckChrBtnsActivatesButton)
{
	MyPlayer->_pStatPts = 5;

	auto buttonId = static_cast<size_t>(CharacterAttribute::Strength);
	Rectangle button = CharPanelButtonRect[buttonId];
	SetPanelObjectPosition(UiPanels::Character, button);
	MousePosition = Point {
		button.position.x + button.size.width / 2,
		button.position.y + button.size.height / 2
	};

	CheckChrBtns();

	EXPECT_TRUE(CharPanelButtonActive)
	    << "CharPanelButtonActive should be true after CheckChrBtns with stat points";
	EXPECT_TRUE(CharPanelButton[buttonId])
	    << "The specific button should be marked as pressed";
}

// ===========================================================================
// No stat points available
// ===========================================================================

TEST_F(CharPanelTest, NoStatPoints_CheckChrBtnsDoesNothing)
{
	MyPlayer->_pStatPts = 0;

	// Position mouse over the first button.
	Rectangle button = CharPanelButtonRect[0];
	SetPanelObjectPosition(UiPanels::Character, button);
	MousePosition = Point {
		button.position.x + button.size.width / 2,
		button.position.y + button.size.height / 2
	};

	CheckChrBtns();

	EXPECT_FALSE(CharPanelButtonActive)
	    << "Buttons should not activate when there are no stat points";
}

TEST_F(CharPanelTest, NoStatPoints_StatPointsUnchanged)
{
	MyPlayer->_pStatPts = 0;

	ClickStatButton(CharacterAttribute::Strength);

	EXPECT_EQ(MyPlayer->_pStatPts, 0)
	    << "Stat points should remain zero";
}

// ===========================================================================
// Stat values match player
// ===========================================================================

TEST_F(CharPanelTest, StatValues_MatchPlayerStruct)
{
	// The level-25 Warrior created by UITest should have known stat values.
	// Just verify the getter returns matching values.
	EXPECT_EQ(MyPlayer->GetBaseAttributeValue(CharacterAttribute::Strength),
	    MyPlayer->_pBaseStr);
	EXPECT_EQ(MyPlayer->GetBaseAttributeValue(CharacterAttribute::Magic),
	    MyPlayer->_pBaseMag);
	EXPECT_EQ(MyPlayer->GetBaseAttributeValue(CharacterAttribute::Dexterity),
	    MyPlayer->_pBaseDex);
	EXPECT_EQ(MyPlayer->GetBaseAttributeValue(CharacterAttribute::Vitality),
	    MyPlayer->_pBaseVit);
}

// ===========================================================================
// Multiple allocations
// ===========================================================================

TEST_F(CharPanelTest, MultipleAllocations_AllStatPointsUsed)
{
	MyPlayer->_pStatPts = 3;

	ClickStatButton(CharacterAttribute::Strength);
	ClickStatButton(CharacterAttribute::Strength);
	ClickStatButton(CharacterAttribute::Strength);

	EXPECT_EQ(MyPlayer->_pStatPts, 0)
	    << "All 3 stat points should be consumed";
}

TEST_F(CharPanelTest, MultipleAllocations_DifferentStats)
{
	MyPlayer->_pStatPts = 4;

	ClickStatButton(CharacterAttribute::Strength);
	ClickStatButton(CharacterAttribute::Magic);
	ClickStatButton(CharacterAttribute::Dexterity);
	ClickStatButton(CharacterAttribute::Vitality);

	EXPECT_EQ(MyPlayer->_pStatPts, 0)
	    << "All 4 stat points should be consumed across different stats";
}

// ===========================================================================
// Edge case: allocation stops at max stat
// ===========================================================================

TEST_F(CharPanelTest, AllocationStopsAtMaxStat)
{
	// Set strength to the maximum.
	const int maxStr = MyPlayer->GetMaximumAttributeValue(CharacterAttribute::Strength);
	MyPlayer->_pBaseStr = maxStr;
	MyPlayer->_pStatPts = 5;

	// Position mouse and try to press — CheckChrBtns should skip the button
	// because the stat is already at max.
	auto buttonId = static_cast<size_t>(CharacterAttribute::Strength);
	Rectangle button = CharPanelButtonRect[buttonId];
	SetPanelObjectPosition(UiPanels::Character, button);
	MousePosition = Point {
		button.position.x + button.size.width / 2,
		button.position.y + button.size.height / 2
	};

	CheckChrBtns();

	EXPECT_FALSE(CharPanelButton[buttonId])
	    << "Strength button should not activate when stat is at maximum";
	EXPECT_EQ(MyPlayer->_pStatPts, 5)
	    << "Stat points should be unchanged";
}

} // namespace
} // namespace devilution