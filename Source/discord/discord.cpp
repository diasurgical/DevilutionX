#include "discord.h"

#ifdef _WIN32
// On Windows, discordsrc-src/cpp/discord.h includes windows.h
#define NOMINMAX 1
#define WIN32_LEAN_AND_MEAN
#endif
#include <discordsrc-src/cpp/discord.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <string>
#include <tuple>

#include <fmt/format.h>

#include "config.h"
#include "levels/gendung.h"
#include "levels/setmaps.h"
#include "lua/lua_global.hpp"
#include "multi.h"
#include "panels/charpanel.hpp"
#include "player.h"
#include "playerdat.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {
void ModChanged()
{
	discord_manager::UpdateMenu(true);
}
const auto ModChangedHandler = (AddModsChangedHandler(ModChanged), true);
} // namespace

namespace discord_manager {

// App ID used for DevilutionX's Diablo (classic Diablo's is 496571953147150354)
constexpr discord::ClientId DiscordDevilutionxAppId = 795760213524742205;

constexpr auto IgnoreResult = [](discord::Result result) {};

discord::Core *discord_core = []() -> discord::Core * {
	discord::Core *core;
	discord::Result result = discord::Core::Create(DiscordDevilutionxAppId, DiscordCreateFlags_NoRequireDiscord, &core);
	if (result != discord::Result::Ok) {
		core = nullptr;
	}
	return core;
}();

struct PlayerData {
	dungeon_type dungeonArea;
	_setlevels questMap;
	Uint8 dungeonLevel;
	Uint8 playerLevel;
	int playerGfx;

	// Why??? This is POD
	bool operator!=(const PlayerData &other) const
	{
		return std::tie(dungeonArea, dungeonLevel, playerLevel, playerGfx) != std::tie(other.dungeonArea, other.dungeonLevel, other.playerLevel, other.playerGfx);
	}
};

bool want_menu_update = true;
PlayerData tracked_data;
Sint64 start_time = 0;

std::string GetLocationString()
{
	// Quest Level Name
	if (setlevel) {
		return std::string(_(QuestLevelNames[setlvlnum]));
	}

	// Dungeon Name
	constexpr std::array<const char *, DTYPE_LAST + 1> DungeonStrs = { N_("Town"), N_("Cathedral"), N_("Catacombs"), N_("Caves"), N_("Hell"), N_("Nest"), N_("Crypt") };
	std::string dungeonStr;
	if (tracked_data.dungeonArea != DTYPE_NONE) {
		dungeonStr = _(DungeonStrs[tracked_data.dungeonArea]);
	} else {
		dungeonStr = _(/* TRANSLATORS: type of dungeon (i.e. Cathedral, Caves)*/ "None");
	}

	// Dungeon Level
	if (tracked_data.dungeonLevel > 0) {
		int level = tracked_data.dungeonLevel;
		if (tracked_data.dungeonArea == DTYPE_NEST)
			level -= 16;
		else if (tracked_data.dungeonArea == DTYPE_CRYPT)
			level -= 20;

		return fmt::format(fmt::runtime(_(/* TRANSLATORS: dungeon type and floor number i.e. "Cathedral 3"*/ "{} {}")), dungeonStr, level);
	}
	return dungeonStr;
}

std::string GetCharacterString()
{
	return fmt::format(fmt::runtime(_(/* TRANSLATORS: Discord character, i.e. "Lv 6 Warrior" */ "Lv {} {}")), tracked_data.playerLevel, MyPlayer->getClassName());
}

std::string GetDetailString()
{
	return StrCat(GetCharacterString(), " - ", GetLocationString());
}

std::string GetStateString()
{
	constexpr std::array<const char *, 3> DifficultyStrs = { N_("Normal"), N_("Nightmare"), N_("Hell") };
	const std::string_view difficultyStr = _(DifficultyStrs[sgGameInitInfo.nDifficulty]);
	return fmt::format(fmt::runtime(_(/* TRANSLATORS: Discord state i.e. "Nightmare difficulty" */ "{} difficulty")), difficultyStr);
}

std::string GetTooltipString()
{
	return StrCat(MyPlayer->_pName, " - ", GetCharacterString());
}

std::string GetPlayerAssetString()
{
	char chars[5] {
		GetPlayerSpriteDataForClass(MyPlayer->_pClass).classChar,
		ArmourChar[tracked_data.playerGfx >> 4],
		WepChar[tracked_data.playerGfx & 0xF],
		'a',
		's'
	};
	return std::string(chars, 5);
}

void ResetStartTime()
{
	start_time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

const char *GetIconAsset()
{
	return gbIsHellfire ? "hellfire" : "icon";
}

void UpdateGame()
{
	if (discord_core == nullptr)
		return;

	auto newData = PlayerData {
		leveltype, setlvlnum, currlevel, MyPlayer->getCharacterLevel(), MyPlayer->_pgfxnum
	};
	if (newData != tracked_data) {
		tracked_data = newData;

		// Update status strings
		discord::Activity activity = {};
		activity.SetName(PROJECT_NAME);
		activity.SetState(GetStateString().c_str());
		activity.SetDetails(GetDetailString().c_str());
		activity.SetInstance(true);

		activity.GetTimestamps().SetStart(start_time);

		// Set image assets
		activity.GetAssets().SetLargeImage(GetPlayerAssetString().c_str());
		activity.GetAssets().SetLargeText(GetTooltipString().c_str());
		activity.GetAssets().SetSmallImage(GetIconAsset());
		activity.GetAssets().SetSmallText(gszProductName);

		discord_core->ActivityManager().UpdateActivity(activity, IgnoreResult);
	}
	discord_core->RunCallbacks();
}

void StartGame()
{
	tracked_data = PlayerData { dungeon_type::DTYPE_NONE, _setlevels::SL_NONE, 0, 0, 0 };
	want_menu_update = true;
	ResetStartTime();
}

void UpdateMenu(bool forced)
{
	if (discord_core == nullptr)
		return;

	if (want_menu_update || forced) {
		if (!forced) {
			ResetStartTime();
		}
		want_menu_update = false;

		discord::Activity activity = {};
		activity.SetName(PROJECT_NAME);
		activity.SetState(_(/* TRANSLATORS: Discord activity, not in game */ "In Menu").data());

		activity.GetTimestamps().SetStart(start_time);

		activity.GetAssets().SetLargeImage(GetIconAsset());
		activity.GetAssets().SetLargeText(gszProductName);

		discord_core->ActivityManager().UpdateActivity(activity, IgnoreResult);
	}
	discord_core->RunCallbacks();
}

} // namespace discord_manager
} // namespace devilution
