/**
 * @file utils/navigation_speech.cpp
 *
 * Navigation speech: exit/stairs/portal/unexplored speech and keyboard walk keys.
 */
#include "utils/navigation_speech.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include <fmt/format.h>

#ifdef USE_SDL3
#include <SDL3/SDL_keycode.h>
#else
#include <SDL.h>
#endif

#include "automap.h"
#include "control/control.hpp"
#include "controls/accessibility_keys.hpp"
#include "controls/plrctrls.h"
#include "diablo.h"
#include "help.h"
#include "inv.h"
#include "levels/gendung.h"
#include "levels/setmaps.h"
#include "levels/tile_properties.hpp"
#include "levels/trigs.h"
#include "minitext.h"
#include "missiles.h"
#include "multi.h"
#include "player.h"
#include "portal.h"
#include "qol/chatlog.h"
#include "qol/stash.h"
#include "quests.h"
#include "stores.h"
#include "utils/language.h"
#include "utils/screen_reader.hpp"
#include "utils/str_cat.hpp"
#include "utils/sdl_compat.h"
#include "utils/walk_path_speech.hpp"

namespace devilution {

namespace {

std::optional<Point> FindNearestUnexploredTile(Point startPosition)
{
	if (!InDungeonBounds(startPosition))
		return std::nullopt;

	std::array<bool, MAXDUNX * MAXDUNY> visited {};
	std::queue<Point> queue;

	const auto enqueue = [&](Point position) {
		if (!InDungeonBounds(position))
			return;

		const size_t index = static_cast<size_t>(position.x) + static_cast<size_t>(position.y) * MAXDUNX;
		if (visited[index])
			return;

		if (!IsTileWalkable(position, /*ignoreDoors=*/true))
			return;

		visited[index] = true;
		queue.push(position);
	};

	enqueue(startPosition);

	constexpr std::array<Direction, 4> Neighbors = {
		Direction::NorthEast,
		Direction::SouthWest,
		Direction::SouthEast,
		Direction::NorthWest,
	};

	while (!queue.empty()) {
		const Point position = queue.front();
		queue.pop();

		if (!HasAnyOf(dFlags[position.x][position.y], DungeonFlag::Explored))
			return position;

		for (const Direction dir : Neighbors) {
			enqueue(position + dir);
		}
	}

	return std::nullopt;
}

std::string TriggerLabelForSpeech(const TriggerStruct &trigger)
{
	switch (trigger._tmsg) {
	case WM_DIABNEXTLVL:
		if (leveltype == DTYPE_TOWN)
			return std::string { _("Cathedral entrance") };
		return std::string { _("Stairs down") };
	case WM_DIABPREVLVL:
		return std::string { _("Stairs up") };
	case WM_DIABTOWNWARP:
		switch (trigger._tlvl) {
		case 5:
			return fmt::format(fmt::runtime(_("Town warp to {:s}")), _("Catacombs"));
		case 9:
			return fmt::format(fmt::runtime(_("Town warp to {:s}")), _("Caves"));
		case 13:
			return fmt::format(fmt::runtime(_("Town warp to {:s}")), _("Hell"));
		case 17:
			return fmt::format(fmt::runtime(_("Town warp to {:s}")), _("Nest"));
		case 21:
			return fmt::format(fmt::runtime(_("Town warp to {:s}")), _("Crypt"));
		default:
			return fmt::format(fmt::runtime(_("Town warp to level {:d}")), trigger._tlvl);
		}
	case WM_DIABTWARPUP:
		return std::string { _("Warp up") };
	case WM_DIABRETOWN:
		return std::string { _("Return to town") };
	case WM_DIABWARPLVL:
		return std::string { _("Warp") };
	case WM_DIABSETLVL:
		return std::string { _("Set level") };
	case WM_DIABRTNLVL:
		return std::string { _("Return level") };
	default:
		return std::string { _("Exit") };
	}
}

std::optional<int> LockedTownDungeonTriggerIndex;

std::vector<int> CollectTownDungeonTriggerIndices()
{
	std::vector<int> result;
	result.reserve(static_cast<size_t>(std::max(0, numtrigs)));

	for (int i = 0; i < numtrigs; ++i) {
		if (IsAnyOf(trigs[i]._tmsg, WM_DIABNEXTLVL, WM_DIABTOWNWARP))
			result.push_back(i);
	}

	std::sort(result.begin(), result.end(), [](int a, int b) {
		const TriggerStruct &ta = trigs[a];
		const TriggerStruct &tb = trigs[b];

		const int kindA = ta._tmsg == WM_DIABNEXTLVL ? 0 : (ta._tmsg == WM_DIABTOWNWARP ? 1 : 2);
		const int kindB = tb._tmsg == WM_DIABNEXTLVL ? 0 : (tb._tmsg == WM_DIABTOWNWARP ? 1 : 2);
		if (kindA != kindB)
			return kindA < kindB;

		if (ta._tmsg == WM_DIABTOWNWARP && tb._tmsg == WM_DIABTOWNWARP && ta._tlvl != tb._tlvl)
			return ta._tlvl < tb._tlvl;

		return a < b;
	});

	return result;
}

std::optional<int> FindDefaultTownDungeonTriggerIndex(const std::vector<int> &candidates)
{
	for (const int index : candidates) {
		if (trigs[index]._tmsg == WM_DIABNEXTLVL)
			return index;
	}
	if (!candidates.empty())
		return candidates.front();
	return std::nullopt;
}

std::optional<int> FindLockedTownDungeonTriggerIndex(const std::vector<int> &candidates)
{
	if (!LockedTownDungeonTriggerIndex)
		return std::nullopt;
	if (std::find(candidates.begin(), candidates.end(), *LockedTownDungeonTriggerIndex) != candidates.end())
		return *LockedTownDungeonTriggerIndex;
	return std::nullopt;
}

std::optional<int> FindNextTownDungeonTriggerIndex(const std::vector<int> &candidates, int current)
{
	if (candidates.empty())
		return std::nullopt;

	const auto it = std::find(candidates.begin(), candidates.end(), current);
	if (it == candidates.end())
		return candidates.front();
	if (std::next(it) == candidates.end())
		return candidates.front();
	return *std::next(it);
}

std::optional<int> FindNearestTriggerIndexWithMessage(int message)
{
	if (numtrigs <= 0 || MyPlayer == nullptr)
		return std::nullopt;

	const Point playerPosition = MyPlayer->position.future;
	std::optional<int> bestIndex;
	int bestDistance = 0;

	for (int i = 0; i < numtrigs; ++i) {
		if (trigs[i]._tmsg != message)
			continue;

		const Point triggerPosition { trigs[i].position.x, trigs[i].position.y };
		const int distance = playerPosition.WalkingDistance(triggerPosition);
		if (!bestIndex || distance < bestDistance) {
			bestIndex = i;
			bestDistance = distance;
		}
	}

	return bestIndex;
}

std::optional<Point> FindNearestTownPortalOnCurrentLevel()
{
	if (MyPlayer == nullptr || leveltype == DTYPE_TOWN)
		return std::nullopt;

	const Point playerPosition = MyPlayer->position.future;
	const int currentLevel = setlevel ? static_cast<int>(setlvlnum) : currlevel;

	std::optional<Point> bestPosition;
	int bestDistance = 0;

	for (int i = 0; i < MAXPORTAL; ++i) {
		const Portal &portal = Portals[i];
		if (!portal.open)
			continue;
		if (portal.setlvl != setlevel)
			continue;
		if (portal.level != currentLevel)
			continue;

		const int distance = playerPosition.WalkingDistance(portal.position);
		if (!bestPosition || distance < bestDistance) {
			bestPosition = portal.position;
			bestDistance = distance;
		}
	}

	return bestPosition;
}

struct TownPortalInTown {
	int portalIndex;
	Point position;
	int distance;
};

std::optional<TownPortalInTown> FindNearestTownPortalInTown()
{
	if (MyPlayer == nullptr || leveltype != DTYPE_TOWN)
		return std::nullopt;

	const Point playerPosition = MyPlayer->position.future;

	std::optional<TownPortalInTown> best;
	int bestDistance = 0;

	for (const Missile &missile : Missiles) {
		if (missile._mitype != MissileID::TownPortal)
			continue;
		if (missile._misource < 0 || missile._misource >= MAXPORTAL)
			continue;
		if (!Portals[missile._misource].open)
			continue;

		const Point portalPosition = missile.position.tile;
		const int distance = playerPosition.WalkingDistance(portalPosition);
		if (!best || distance < bestDistance) {
			best = TownPortalInTown {
				.portalIndex = missile._misource,
				.position = portalPosition,
				.distance = distance,
			};
			bestDistance = distance;
		}
	}

	return best;
}

[[nodiscard]] std::string TownPortalLabelForSpeech(const Portal &portal)
{
	if (portal.level <= 0)
		return std::string { _("Town portal") };

	if (portal.setlvl) {
		const auto questLevel = static_cast<_setlevels>(portal.level);
		const char *questLevelName = QuestLevelNames[questLevel];
		if (questLevelName == nullptr || questLevelName[0] == '\0')
			return std::string { _("Town portal to set level") };

		return fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} is a set/quest level name. */ "Town portal to {:s}")), _(questLevelName));
	}

	constexpr std::array<const char *, DTYPE_LAST + 1> DungeonStrs = {
		N_("Town"),
		N_("Cathedral"),
		N_("Catacombs"),
		N_("Caves"),
		N_("Hell"),
		N_("Nest"),
		N_("Crypt"),
	};
	std::string dungeonStr;
	if (portal.ltype >= DTYPE_TOWN && portal.ltype <= DTYPE_LAST) {
		dungeonStr = _(DungeonStrs[static_cast<size_t>(portal.ltype)]);
	} else {
		dungeonStr = _(/* TRANSLATORS: type of dungeon (i.e. Cathedral, Caves)*/ "None");
	}

	int floor = portal.level;
	if (portal.ltype == DTYPE_CATACOMBS)
		floor -= 4;
	else if (portal.ltype == DTYPE_CAVES)
		floor -= 8;
	else if (portal.ltype == DTYPE_HELL)
		floor -= 12;
	else if (portal.ltype == DTYPE_NEST)
		floor -= 16;
	else if (portal.ltype == DTYPE_CRYPT)
		floor -= 20;

	if (floor > 0)
		return fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} is a dungeon name and {:d} is a floor number. */ "Town portal to {:s} {:d}")), dungeonStr, floor);

	return fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} is a dungeon name. */ "Town portal to {:s}")), dungeonStr);
}

struct QuestSetLevelEntrance {
	_setlevels questLevel;
	Point entrancePosition;
	int distance;
};

std::optional<QuestSetLevelEntrance> FindNearestQuestSetLevelEntranceOnCurrentLevel()
{
	if (MyPlayer == nullptr || setlevel)
		return std::nullopt;

	const Point playerPosition = MyPlayer->position.future;
	std::optional<QuestSetLevelEntrance> best;
	int bestDistance = 0;

	for (const Quest &quest : Quests) {
		if (quest._qslvl == SL_NONE)
			continue;
		if (quest._qactive == QUEST_NOTAVAIL)
			continue;
		if (quest._qlevel != currlevel)
			continue;
		if (!InDungeonBounds(quest.position))
			continue;

		const int distance = playerPosition.WalkingDistance(quest.position);
		if (!best || distance < bestDistance) {
			best = QuestSetLevelEntrance {
				.questLevel = quest._qslvl,
				.entrancePosition = quest.position,
				.distance = distance,
			};
			bestDistance = distance;
		}
	}

	return best;
}

void SpeakNearestStairsKeyPressed(int triggerMessage)
{
	if (!CanPlayerTakeAction())
		return;
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (leveltype == DTYPE_TOWN) {
		SpeakText(_("Not in a dungeon."), true);
		return;
	}
	if (MyPlayer == nullptr)
		return;

	const std::optional<int> triggerIndex = FindNearestTriggerIndexWithMessage(triggerMessage);
	if (!triggerIndex) {
		SpeakText(_("No exits found."), true);
		return;
	}

	const TriggerStruct &trigger = trigs[*triggerIndex];
	const Point startPosition = MyPlayer->position.future;
	const Point targetPosition { trigger.position.x, trigger.position.y };

	std::string message;
	const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);
	if (!path) {
		AppendDirectionalFallback(message, targetPosition - startPosition);
	} else {
		AppendKeyboardWalkPathForSpeech(message, *path);
	}

	SpeakText(message, true);
}

void KeyboardWalkKeyPressed(Direction direction)
{
	CancelAutoWalk();
	if (!IsKeyboardWalkAllowed())
		return;

	if (MyPlayer == nullptr)
		return;

	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, MyPlayer->position.future + direction);
}

} // namespace

void SpeakNearestExitKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (MyPlayer == nullptr)
		return;

	const Point startPosition = MyPlayer->position.future;

	const SDL_Keymod modState = SDL_GetModState();
	const bool seekQuestEntrance = (modState & SDL_KMOD_SHIFT) != 0;
	const bool cycleTownDungeon = (modState & SDL_KMOD_CTRL) != 0;

	if (seekQuestEntrance) {
		if (setlevel) {
			const std::optional<int> triggerIndex = FindNearestTriggerIndexWithMessage(WM_DIABRTNLVL);
			if (!triggerIndex) {
				SpeakText(_("No quest exits found."), true);
				return;
			}

			const TriggerStruct &trigger = trigs[*triggerIndex];
			const Point targetPosition { trigger.position.x, trigger.position.y };
			const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);
			std::string message = TriggerLabelForSpeech(trigger);
			if (!message.empty())
				message.append(": ");
			if (!path)
				AppendDirectionalFallback(message, targetPosition - startPosition);
			else
				AppendKeyboardWalkPathForSpeech(message, *path);
			SpeakText(message, true);
			return;
		}

		if (const std::optional<QuestSetLevelEntrance> entrance = FindNearestQuestSetLevelEntranceOnCurrentLevel(); entrance) {
			const Point targetPosition = entrance->entrancePosition;
			const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);

			std::string message { _(QuestLevelNames[entrance->questLevel]) };
			message.append(": ");
			if (!path)
				AppendDirectionalFallback(message, targetPosition - startPosition);
			else
				AppendKeyboardWalkPathForSpeech(message, *path);
			SpeakText(message, true);
			return;
		}

		SpeakText(_("No quest entrances found."), true);
		return;
	}

	if (leveltype == DTYPE_TOWN) {
		const std::vector<int> dungeonCandidates = CollectTownDungeonTriggerIndices();
		if (dungeonCandidates.empty()) {
			SpeakText(_("No exits found."), true);
			return;
		}

		if (cycleTownDungeon) {
			if (dungeonCandidates.size() <= 1) {
				SpeakText(_("No other dungeon entrances found."), true);
				return;
			}

			const int current = LockedTownDungeonTriggerIndex.value_or(-1);
			const std::optional<int> next = FindNextTownDungeonTriggerIndex(dungeonCandidates, current);
			if (!next) {
				SpeakText(_("No other dungeon entrances found."), true);
				return;
			}

			LockedTownDungeonTriggerIndex = *next;
			const std::string label = TriggerLabelForSpeech(trigs[*next]);
			if (!label.empty())
				SpeakText(label, true);
			return;
		}

		const int triggerIndex = FindLockedTownDungeonTriggerIndex(dungeonCandidates)
		                             .value_or(FindDefaultTownDungeonTriggerIndex(dungeonCandidates).value_or(dungeonCandidates.front()));
		LockedTownDungeonTriggerIndex = triggerIndex;

		const TriggerStruct &trigger = trigs[triggerIndex];
		const Point targetPosition { trigger.position.x, trigger.position.y };

		const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);
		std::string message = TriggerLabelForSpeech(trigger);
		if (!message.empty())
			message.append(": ");
		if (!path)
			AppendDirectionalFallback(message, targetPosition - startPosition);
		else
			AppendKeyboardWalkPathForSpeech(message, *path);

		SpeakText(message, true);
		return;
	}

	if (const std::optional<Point> portalPosition = FindNearestTownPortalOnCurrentLevel(); portalPosition) {
		const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, *portalPosition);
		std::string message { _("Return to town") };
		message.append(": ");
		if (!path)
			AppendDirectionalFallback(message, *portalPosition - startPosition);
		else
			AppendKeyboardWalkPathForSpeech(message, *path);
		SpeakText(message, true);
		return;
	}

	const std::optional<int> triggerIndex = FindNearestTriggerIndexWithMessage(WM_DIABPREVLVL);
	if (!triggerIndex) {
		SpeakText(_("No exits found."), true);
		return;
	}

	const TriggerStruct &trigger = trigs[*triggerIndex];
	const Point targetPosition { trigger.position.x, trigger.position.y };
	const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);
	std::string message = TriggerLabelForSpeech(trigger);
	if (!message.empty())
		message.append(": ");
	if (!path)
		AppendDirectionalFallback(message, targetPosition - startPosition);
	else
		AppendKeyboardWalkPathForSpeech(message, *path);
	SpeakText(message, true);
}

void SpeakNearestTownPortalInTownKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (leveltype != DTYPE_TOWN) {
		SpeakText(_("Not in town."), true);
		return;
	}
	if (MyPlayer == nullptr)
		return;

	const std::optional<TownPortalInTown> portal = FindNearestTownPortalInTown();
	if (!portal) {
		SpeakText(_("No town portals found."), true);
		return;
	}

	const Point startPosition = MyPlayer->position.future;
	const Point targetPosition = portal->position;

	const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, targetPosition);

	std::string message = TownPortalLabelForSpeech(Portals[portal->portalIndex]);
	message.append(": ");
	if (!path)
		AppendDirectionalFallback(message, targetPosition - startPosition);
	else
		AppendKeyboardWalkPathForSpeech(message, *path);

	SpeakText(message, true);
}

void SpeakNearestStairsDownKeyPressed()
{
	SpeakNearestStairsKeyPressed(WM_DIABNEXTLVL);
}

void SpeakNearestStairsUpKeyPressed()
{
	SpeakNearestStairsKeyPressed(WM_DIABPREVLVL);
}

bool IsKeyboardWalkAllowed()
{
	return CanPlayerTakeAction()
	    && !InGameMenu()
	    && !IsPlayerInStore()
	    && !QuestLogIsOpen
	    && !HelpFlag
	    && !ChatLogFlag
	    && !ChatFlag
	    && !DropGoldFlag
	    && !IsStashOpen
	    && !IsWithdrawGoldOpen
	    && !AutomapActive
	    && !invflag
	    && !CharFlag
	    && !SpellbookFlag
	    && !SpellSelectFlag
	    && !qtextflag;
}

void KeyboardWalkNorthKeyPressed()
{
	KeyboardWalkKeyPressed(Direction::NorthEast);
}

void KeyboardWalkSouthKeyPressed()
{
	KeyboardWalkKeyPressed(Direction::SouthWest);
}

void KeyboardWalkEastKeyPressed()
{
	KeyboardWalkKeyPressed(Direction::SouthEast);
}

void KeyboardWalkWestKeyPressed()
{
	KeyboardWalkKeyPressed(Direction::NorthWest);
}

void SpeakNearestUnexploredTileKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (leveltype == DTYPE_TOWN) {
		SpeakText(_("Not in a dungeon."), true);
		return;
	}
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (MyPlayer == nullptr)
		return;

	const Point startPosition = MyPlayer->position.future;
	const std::optional<Point> target = FindNearestUnexploredTile(startPosition);
	if (!target) {
		SpeakText(_("No unexplored areas found."), true);
		return;
	}
	const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(*MyPlayer, startPosition, *target);
	std::string message;
	if (!path)
		AppendDirectionalFallback(message, *target - startPosition);
	else
		AppendKeyboardWalkPathForSpeech(message, *path);

	SpeakText(message, true);
}

} // namespace devilution
