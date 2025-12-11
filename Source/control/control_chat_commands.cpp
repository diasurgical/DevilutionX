#include "control_chat_commands.hpp"
#include "control/control.hpp"

#include "diablo_msg.hpp"
#include "engine/backbuffer_state.hpp"
#include "inv.h"
#include "levels/setmaps.h"
#include "storm/storm_net.hpp"
#include "utils/algorithm/container.hpp"
#include "utils/log.hpp"
#include "utils/parse_int.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"

#ifdef _DEBUG
#include "debug.h"
#endif

namespace devilution {

namespace {

struct TextCmdItem {
	const std::string text;
	const std::string description;
	const std::string requiredParameter;
	std::string (*actionProc)(const std::string_view);
};

extern std::vector<TextCmdItem> TextCmdList;

std::string TextCmdHelp(const std::string_view parameter)
{
	if (parameter.empty()) {
		std::string ret;
		StrAppend(ret, _("Available Commands:"));
		for (const TextCmdItem &textCmd : TextCmdList) {
			StrAppend(ret, " ", _(textCmd.text));
		}
		return ret;
	}
	auto textCmdIterator = c_find_if(TextCmdList, [&](const TextCmdItem &elem) { return elem.text == parameter; });
	if (textCmdIterator == TextCmdList.end())
		return StrCat(_("Command "), parameter, _(" is unknown."));
	auto &textCmdItem = *textCmdIterator;
	if (textCmdItem.requiredParameter.empty())
		return StrCat(_("Description: "), _(textCmdItem.description), _("\nParameters: No additional parameter needed."));
	return StrCat(_("Description: "), _(textCmdItem.description), _("\nParameters: "), _(textCmdItem.requiredParameter));
}

void AppendArenaOverview(std::string &ret)
{
	for (int arena = SL_FIRST_ARENA; arena <= SL_LAST; arena++) {
		StrAppend(ret, "\n", arena - SL_FIRST_ARENA + 1, " (", QuestLevelNames[arena], ")");
	}
}

std::string TextCmdArena(const std::string_view parameter)
{
	std::string ret;
	if (!gbIsMultiplayer) {
		StrAppend(ret, _("Arenas are only supported in multiplayer."));
		return ret;
	}

	if (parameter.empty()) {
		StrAppend(ret, _("What arena do you want to visit?"));
		AppendArenaOverview(ret);
		return ret;
	}

	const ParseIntResult<int> parsedParam = ParseInt<int>(parameter, /*min=*/0);
	const _setlevels arenaLevel = parsedParam.has_value() ? static_cast<_setlevels>(parsedParam.value() - 1 + SL_FIRST_ARENA) : _setlevels::SL_NONE;
	if (!IsArenaLevel(arenaLevel)) {
		StrAppend(ret, _("Invalid arena-number. Valid numbers are:"));
		AppendArenaOverview(ret);
		return ret;
	}

	if (!MyPlayer->isOnLevel(0) && !MyPlayer->isOnArenaLevel()) {
		StrAppend(ret, _("To enter a arena, you need to be in town or another arena."));
		return ret;
	}

	setlvltype = GetArenaLevelType(arenaLevel);
	StartNewLvl(*MyPlayer, WM_DIABSETLVL, arenaLevel);
	return ret;
}

std::string TextCmdArenaPot(const std::string_view parameter)
{
	std::string ret;
	if (!gbIsMultiplayer) {
		StrAppend(ret, _("Arenas are only supported in multiplayer."));
		return ret;
	}
	const int numPots = ParseInt<int>(parameter, /*min=*/1).value_or(1);

	Player &myPlayer = *MyPlayer;

	for (int potNumber = numPots; potNumber > 0; potNumber--) {
		Item item {};
		InitializeItem(item, IDI_ARENAPOT);
		GenerateNewSeed(item);
		item.updateRequiredStatsCacheForPlayer(myPlayer);

		if (!AutoPlaceItemInBelt(myPlayer, item, true, true) && !AutoPlaceItemInInventory(myPlayer, item, true)) {
			break; // inventory is full
		}
	}

	return ret;
}

std::string TextCmdInspect(const std::string_view parameter)
{
	std::string ret;
	if (!gbIsMultiplayer) {
		StrAppend(ret, _("Inspecting only supported in multiplayer."));
		return ret;
	}

	if (parameter.empty()) {
		StrAppend(ret, _("Stopped inspecting players."));
		InspectPlayer = MyPlayer;
		return ret;
	}

	const std::string param = AsciiStrToLower(parameter);
	auto it = c_find_if(Players, [&param](const Player &player) {
		return AsciiStrToLower(player._pName) == param;
	});
	if (it == Players.end()) {
		it = c_find_if(Players, [&param](const Player &player) {
			return AsciiStrToLower(player._pName).find(param) != std::string::npos;
		});
	}
	if (it == Players.end()) {
		StrAppend(ret, _("No players found with such a name"));
		return ret;
	}

	Player &player = *it;
	InspectPlayer = &player;
	StrAppend(ret, _("Inspecting player: "));
	StrAppend(ret, player._pName);
	OpenCharPanel();
	if (!SpellbookFlag)
		invflag = true;
	RedrawEverything();
	return ret;
}

bool IsQuestEnabled(const Quest &quest)
{
	switch (quest._qidx) {
	case Q_FARMER:
		return gbIsHellfire && !sgGameInitInfo.bCowQuest;
	case Q_JERSEY:
		return gbIsHellfire && sgGameInitInfo.bCowQuest;
	case Q_GIRL:
		return gbIsHellfire && sgGameInitInfo.bTheoQuest;
	case Q_CORNSTN:
		return gbIsHellfire && !gbIsMultiplayer;
	case Q_GRAVE:
	case Q_DEFILER:
	case Q_NAKRUL:
		return gbIsHellfire;
	case Q_TRADER:
		return false;
	default:
		return quest._qactive != QUEST_NOTAVAIL;
	}
}

std::string TextCmdLevelSeed(const std::string_view parameter)
{
	const std::string_view levelType = setlevel ? "set level" : "dungeon level";

	char gameId[] = {
		static_cast<char>((sgGameInitInfo.programid >> 24) & 0xFF),
		static_cast<char>((sgGameInitInfo.programid >> 16) & 0xFF),
		static_cast<char>((sgGameInitInfo.programid >> 8) & 0xFF),
		static_cast<char>(sgGameInitInfo.programid & 0xFF),
		'\0'
	};

	const std::string_view mode = gbIsMultiplayer ? "MP" : "SP";
	const std::string_view questPool = UseMultiplayerQuests() ? "MP" : "Full";

	uint32_t questFlags = 0;
	for (const Quest &quest : Quests) {
		questFlags <<= 1;
		if (IsQuestEnabled(quest))
			questFlags |= 1;
	}

	return StrCat(
	    "Seedinfo for ", levelType, " ", currlevel, "\n",
	    "seed: ", DungeonSeeds[currlevel], "\n",
#ifdef _DEBUG
	    "Mid1: ", glMid1Seed[currlevel], "\n",
	    "Mid2: ", glMid2Seed[currlevel], "\n",
	    "Mid3: ", glMid3Seed[currlevel], "\n",
	    "End: ", glEndSeed[currlevel], "\n",
#endif
	    "\n",
	    gameId, " ", mode, "\n",
	    questPool, " quests: ", questFlags, "\n",
	    "Storybook: ", DungeonSeeds[16]);
}

std::string TextCmdPing(const std::string_view parameter)
{
	std::string ret;
	const std::string param = AsciiStrToLower(parameter);
	auto it = c_find_if(Players, [&param](const Player &player) {
		return AsciiStrToLower(player._pName) == param;
	});
	if (it == Players.end()) {
		it = c_find_if(Players, [&param](const Player &player) {
			return AsciiStrToLower(player._pName).find(param) != std::string::npos;
		});
	}
	if (it == Players.end()) {
		StrAppend(ret, _("No players found with such a name"));
		return ret;
	}

	Player &player = *it;
	DvlNetLatencies latencies = DvlNet_GetLatencies(player.getId());

	StrAppend(ret, fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} means: Character Name */ "Latency statistics for {:s}:")), player.name()));

	StrAppend(ret, "\n", fmt::format(fmt::runtime(_(/* TRANSLATORS: Network connectivity statistics */ "Echo latency: {:d} ms")), latencies.echoLatency));

	if (latencies.providerLatency) {
		if (latencies.isRelayed && *latencies.isRelayed) {
			StrAppend(ret, "\n", fmt::format(fmt::runtime(_(/* TRANSLATORS: Network connectivity statistics */ "Provider latency: {:d} ms (Relayed)")), *latencies.providerLatency));
		} else {
			StrAppend(ret, "\n", fmt::format(fmt::runtime(_(/* TRANSLATORS: Network connectivity statistics */ "Provider latency: {:d} ms")), *latencies.providerLatency));
		}
	}

	return ret;
}

std::vector<TextCmdItem> TextCmdList = {
	{ "/help", N_("Prints help overview or help for a specific command."), N_("[command]"), &TextCmdHelp },
	{ "/arena", N_("Enter a PvP Arena."), N_("<arena-number>"), &TextCmdArena },
	{ "/arenapot", N_("Gives Arena Potions."), N_("<number>"), &TextCmdArenaPot },
	{ "/inspect", N_("Inspects stats and equipment of another player."), N_("<player name>"), &TextCmdInspect },
	{ "/seedinfo", N_("Show seed infos for current level."), "", &TextCmdLevelSeed },
	{ "/ping", N_("Show latency statistics for another player."), N_("<player name>"), &TextCmdPing },
};

} // namespace

bool CheckChatCommand(const std::string_view text)
{
	if (text.size() < 1 || text[0] != '/')
		return false;

	auto textCmdIterator = c_find_if(TextCmdList, [&](const TextCmdItem &elem) { return text.find(elem.text) == 0 && (text.length() == elem.text.length() || text[elem.text.length()] == ' '); });
	if (textCmdIterator == TextCmdList.end()) {
		InitDiabloMsg(StrCat(_("Command "), "\"", text, "\"", _(" is unknown.")));
		return true;
	}

	const TextCmdItem &textCmd = *textCmdIterator;
	std::string_view parameter = "";
	if (text.length() > (textCmd.text.length() + 1))
		parameter = text.substr(textCmd.text.length() + 1);
	const std::string result = textCmd.actionProc(parameter);
	if (result != "")
		InitDiabloMsg(result);
	return true;
}

} // namespace devilution
