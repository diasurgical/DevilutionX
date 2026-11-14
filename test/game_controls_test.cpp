#include <gtest/gtest.h>

#include "controls/controller_buttons.h"
#include "controls/game_controls.h"

namespace devilution {
namespace {

ControllerButtonEvent Press(ControllerButton button)
{
	return { button, false };
}

ControllerButtonEvent Release(ControllerButton button)
{
	return { button, true };
}

TEST(SkipsMovieTest, ClosableMoviesAreSkippedByFaceAndMenuButtons)
{
	for (const ControllerButton button : { ControllerButton_BUTTON_A, ControllerButton_BUTTON_B, ControllerButton_BUTTON_START, ControllerButton_BUTTON_BACK }) {
		EXPECT_TRUE(SkipsMovie(Press(button), true));
		EXPECT_TRUE(SkipsMovie(Release(button), true));
	}
	EXPECT_FALSE(SkipsMovie(Press(ControllerButton_BUTTON_X), true));
	EXPECT_FALSE(SkipsMovie(Press(ControllerButton_NONE), true));
}

TEST(SkipsMovieTest, UnclosableMoviesAreSkippedByMenuButtonPress)
{
	EXPECT_TRUE(SkipsMovie(Press(ControllerButton_BUTTON_START), false));
	EXPECT_TRUE(SkipsMovie(Press(ControllerButton_BUTTON_BACK), false));
}

TEST(SkipsMovieTest, UnclosableMoviesIgnoreButtonRelease)
{
	// The next movie in a sequence would otherwise be skipped by the release.
	EXPECT_FALSE(SkipsMovie(Release(ControllerButton_BUTTON_START), false));
	EXPECT_FALSE(SkipsMovie(Release(ControllerButton_BUTTON_BACK), false));
}

TEST(SkipsMovieTest, UnclosableMoviesIgnoreOtherButtons)
{
	EXPECT_FALSE(SkipsMovie(Press(ControllerButton_BUTTON_A), false));
	EXPECT_FALSE(SkipsMovie(Press(ControllerButton_BUTTON_B), false));
	EXPECT_FALSE(SkipsMovie(Press(ControllerButton_BUTTON_X), false));
	EXPECT_FALSE(SkipsMovie(Press(ControllerButton_NONE), false));
}

} // namespace
} // namespace devilution
