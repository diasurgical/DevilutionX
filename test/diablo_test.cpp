#include <gtest/gtest.h>

#include <string>

#ifdef USE_SDL3
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_mouse.h>
#elifdef USE_SDL1
#include <SDL_keysym.h>
#else
#include <SDL_keycode.h>
#include <SDL_mouse.h>
#endif

#include "controls/keymapper.hpp"
#include "diablo.h"
#include "game_mode.hpp"
#include "options.h"
#include "player.h"
#include "utils/paths.h"

using namespace devilution;

TEST(Diablo, diablo_pause_game_unpause)
{
	gbIsMultiplayer = false;
	PauseMode = 1;
	diablo_pause_game();
	EXPECT_EQ(PauseMode, 0);
}

namespace {

class GameSpeedBindings : public testing::Test {
protected:
	static void SetUpTestSuite()
	{
		InitKeymapActions();
	}

	void SetUp() override
	{
		paths::SetConfigPath(paths::BasePath() + "test/fixtures/keybindings");
		LoadOptions();
		MyPlayer = &player_;
		gbIsMultiplayer = false;
		PauseMode = 0;
		SetGameSpeed(35);
	}

	void TearDown() override
	{
		MyPlayer = nullptr;
		gbIsMultiplayer = false;
		paths::SetConfigPath(originalConfigPath_);
	}

	Player player_ = {};
	const std::string originalConfigPath_ = paths::ConfigPath();
};

// Scroll events are dispatched as virtual keys on SDL2 and SDL3.
#ifndef USE_SDL1
constexpr auto ScrollUpKey = static_cast<SDL_Keycode>(MouseScrollUpButton);
constexpr auto ScrollDownKey = static_cast<SDL_Keycode>(MouseScrollDownButton);
constexpr auto MiddleMouseKey = static_cast<SDL_Keycode>(SDL_BUTTON_MIDDLE | KeymapperMouseButtonMask);

TEST_F(GameSpeedBindings, ConfiguredMouseActions)
{
	KeymapperPress(ScrollUpKey);
	EXPECT_EQ(sgGameInitInfo.nTickRate, 40);
	KeymapperPress(ScrollDownKey);
	EXPECT_EQ(sgGameInitInfo.nTickRate, 35);
	SetGameSpeed(MaxGameSpeed - 2);
	KeymapperPress(ScrollUpKey);
	EXPECT_EQ(sgGameInitInfo.nTickRate, MaxGameSpeed);
	KeymapperPress(MiddleMouseKey);
	EXPECT_EQ(sgGameInitInfo.nTickRate, MinGameSpeed);
	KeymapperPress(ScrollDownKey);
	EXPECT_EQ(sgGameInitInfo.nTickRate, MinGameSpeed);
	EXPECT_EQ(*GetOptions().Gameplay.tickRate, MinGameSpeed);
}

TEST_F(GameSpeedBindings, DisabledInMultiplayer)
{
	gbIsMultiplayer = true;
	for (const SDL_Keycode key : { ScrollUpKey, ScrollDownKey, MiddleMouseKey }) {
		KeymapperPress(key);
		EXPECT_EQ(sgGameInitInfo.nTickRate, 35);
	}
}
#endif

TEST_F(GameSpeedBindings, OptInByDefault)
{
	// No diablo.ini here: load the defaults for a fresh installation.
	paths::SetConfigPath(paths::BasePath() + "test/fixtures");
	LoadOptions();
	const auto &keymapper = GetOptions().Keymapper;
	EXPECT_EQ(keymapper.KeyForAction("QuickSpellPrevious"), MouseScrollUpButton);
	EXPECT_EQ(keymapper.KeyForAction("QuickSpellNext"), MouseScrollDownButton);
	EXPECT_EQ(keymapper.KeyForAction("IncreaseGameSpeed"), SDLK_UNKNOWN);
	EXPECT_EQ(keymapper.KeyForAction("DecreaseGameSpeed"), SDLK_UNKNOWN);
	EXPECT_EQ(keymapper.KeyForAction("ResetGameSpeed"), SDLK_UNKNOWN);
}

} // namespace
