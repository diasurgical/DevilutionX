#include "accessibility/location_speech.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_keycode.h>
#else
#include <SDL.h>
#endif

#include <fmt/format.h>

#include "accessibility/tracker.hpp"
#include "automap.h"
#include "control/control.hpp"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "diablo.h"
#include "engine/path.h"
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
#include "tables/playerdat.hpp"
#include "utils/format_int.hpp"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/screen_reader.hpp"
#include "utils/sdl_compat.h"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

// Walk direction helpers (duplicated locally to avoid exposing them as public API).
Point NextPositionForWalkDirection(Point position, int8_t walkDir)
{
	switch (walkDir) {
	case WALK_NE:
		return { position.x, position.y - 1 };
	case WALK_NW:
		return { position.x - 1, position.y };
	case WALK_SE:
		return { position.x + 1, position.y };
	case WALK_SW:
		return { position.x, position.y + 1 };
	case WALK_N:
		return { position.x - 1, position.y - 1 };
	case WALK_E:
		return { position.x + 1, position.y - 1 };
	case WALK_S:
		return { position.x + 1, position.y + 1 };
	case WALK_W:
		return { position.x - 1, position.y + 1 };
	default:
		return position;
	}
}

int8_t OppositeWalkDirection(int8_t walkDir)
{
	switch (walkDir) {
	case WALK_NE:
		return WALK_SW;
	case WALK_SW:
		return WALK_NE;
	case WALK_NW:
		return WALK_SE;
	case WALK_SE:
		return WALK_NW;
	case WALK_N:
		return WALK_S;
	case WALK_S:
		return WALK_N;
	case WALK_E:
		return WALK_W;
	case WALK_W:
		return WALK_E;
	default:
		return WALK_NONE;
	}
}

using PosOkForSpeechFn = bool (*)(const Player &, Point);

template <size_t NumDirections>
std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechBfs(const Player &player, Point startPosition, Point destinationPosition, PosOkForSpeechFn posOk, const std::array<int8_t, NumDirections> &walkDirections, bool allowDiagonalSteps, bool allowDestinationNonWalkable)
{
	if (!InDungeonBounds(startPosition) || !InDungeonBounds(destinationPosition))
		return std::nullopt;

	if (startPosition == destinationPosition)
		return std::vector<int8_t> {};

	std::array<bool, MAXDUNX * MAXDUNY> visited {};
	std::array<int8_t, MAXDUNX * MAXDUNY> parentDir {};
	parentDir.fill(WALK_NONE);

	std::queue<Point> queue;

	const auto indexOf = [](Point position) -> size_t {
		return static_cast<size_t>(position.x) + static_cast<size_t>(position.y) * MAXDUNX;
	};

	const auto enqueue = [&](Point current, int8_t dir) {
		const Point next = NextPositionForWalkDirection(current, dir);
		if (!InDungeonBounds(next))
			return;

		const size_t idx = indexOf(next);
		if (visited[idx])
			return;

		const bool ok = posOk(player, next);
		if (ok) {
			if (!CanStep(current, next))
				return;
		} else {
			if (!allowDestinationNonWalkable || next != destinationPosition)
				return;
		}

		visited[idx] = true;
		parentDir[idx] = dir;
		queue.push(next);
	};

	visited[indexOf(startPosition)] = true;
	queue.push(startPosition);

	const auto hasReachedDestination = [&]() -> bool {
		return visited[indexOf(destinationPosition)];
	};

	while (!queue.empty() && !hasReachedDestination()) {
		const Point current = queue.front();
		queue.pop();

		const Displacement delta = destinationPosition - current;
		const int deltaAbsX = delta.deltaX >= 0 ? delta.deltaX : -delta.deltaX;
		const int deltaAbsY = delta.deltaY >= 0 ? delta.deltaY : -delta.deltaY;

		std::array<int8_t, 8> prioritizedDirs;
		size_t prioritizedCount = 0;

		const auto addUniqueDir = [&](int8_t dir) {
			if (dir == WALK_NONE)
				return;
			for (size_t i = 0; i < prioritizedCount; ++i) {
				if (prioritizedDirs[i] == dir)
					return;
			}
			prioritizedDirs[prioritizedCount++] = dir;
		};

		const int8_t xDir = delta.deltaX > 0 ? WALK_SE : (delta.deltaX < 0 ? WALK_NW : WALK_NONE);
		const int8_t yDir = delta.deltaY > 0 ? WALK_SW : (delta.deltaY < 0 ? WALK_NE : WALK_NONE);

		if (allowDiagonalSteps && delta.deltaX != 0 && delta.deltaY != 0) {
			const int8_t diagDir = delta.deltaX > 0 ? (delta.deltaY > 0 ? WALK_S : WALK_E) : (delta.deltaY > 0 ? WALK_W : WALK_N);
			addUniqueDir(diagDir);
		}

		if (deltaAbsX >= deltaAbsY) {
			addUniqueDir(xDir);
			addUniqueDir(yDir);
		} else {
			addUniqueDir(yDir);
			addUniqueDir(xDir);
		}
		for (const int8_t dir : walkDirections) {
			addUniqueDir(dir);
		}

		for (size_t i = 0; i < prioritizedCount; ++i) {
			enqueue(current, prioritizedDirs[i]);
		}
	}

	if (!hasReachedDestination())
		return std::nullopt;

	std::vector<int8_t> path;
	Point position = destinationPosition;
	while (position != startPosition) {
		const int8_t dir = parentDir[indexOf(position)];
		if (dir == WALK_NONE)
			return std::nullopt;

		path.push_back(dir);
		position = NextPositionForWalkDirection(position, OppositeWalkDirection(dir));
	}

	std::reverse(path.begin(), path.end());
	return path;
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechWithPosOk(const Player &player, Point startPosition, Point destinationPosition, PosOkForSpeechFn posOk, bool allowDestinationNonWalkable)
{
	constexpr std::array<int8_t, 4> AxisDirections = {
		WALK_NE,
		WALK_SW,
		WALK_SE,
		WALK_NW,
	};

	constexpr std::array<int8_t, 8> AllDirections = {
		WALK_NE,
		WALK_SW,
		WALK_SE,
		WALK_NW,
		WALK_N,
		WALK_E,
		WALK_S,
		WALK_W,
	};

	if (const std::optional<std::vector<int8_t>> axisPath = FindKeyboardWalkPathForSpeechBfs(player, startPosition, destinationPosition, posOk, AxisDirections, /*allowDiagonalSteps=*/false, allowDestinationNonWalkable); axisPath) {
		return axisPath;
	}

	return FindKeyboardWalkPathForSpeechBfs(player, startPosition, destinationPosition, posOk, AllDirections, /*allowDiagonalSteps=*/true, allowDestinationNonWalkable);
}

template <size_t NumDirections>
std::optional<std::vector<int8_t>> FindKeyboardWalkPathToClosestReachableForSpeechBfs(const Player &player, Point startPosition, Point destinationPosition, PosOkForSpeechFn posOk, const std::array<int8_t, NumDirections> &walkDirections, bool allowDiagonalSteps, Point &closestPosition)
{
	if (!InDungeonBounds(startPosition) || !InDungeonBounds(destinationPosition))
		return std::nullopt;

	if (startPosition == destinationPosition) {
		closestPosition = destinationPosition;
		return std::vector<int8_t> {};
	}

	std::array<bool, MAXDUNX * MAXDUNY> visited {};
	std::array<int8_t, MAXDUNX * MAXDUNY> parentDir {};
	std::array<uint16_t, MAXDUNX * MAXDUNY> depth {};
	parentDir.fill(WALK_NONE);
	depth.fill(0);

	std::queue<Point> queue;

	const auto indexOf = [](Point position) -> size_t {
		return static_cast<size_t>(position.x) + static_cast<size_t>(position.y) * MAXDUNX;
	};

	const auto enqueue = [&](Point current, int8_t dir) {
		const Point next = NextPositionForWalkDirection(current, dir);
		if (!InDungeonBounds(next))
			return;

		const size_t nextIdx = indexOf(next);
		if (visited[nextIdx])
			return;

		if (!posOk(player, next))
			return;
		if (!CanStep(current, next))
			return;

		const size_t currentIdx = indexOf(current);
		visited[nextIdx] = true;
		parentDir[nextIdx] = dir;
		depth[nextIdx] = static_cast<uint16_t>(depth[currentIdx] + 1);
		queue.push(next);
	};

	const size_t startIdx = indexOf(startPosition);
	visited[startIdx] = true;
	queue.push(startPosition);

	Point best = startPosition;
	int bestDistance = startPosition.WalkingDistance(destinationPosition);
	uint16_t bestDepth = 0;

	const auto considerBest = [&](Point position) {
		const int distance = position.WalkingDistance(destinationPosition);
		const uint16_t posDepth = depth[indexOf(position)];
		if (distance < bestDistance || (distance == bestDistance && posDepth < bestDepth)) {
			best = position;
			bestDistance = distance;
			bestDepth = posDepth;
		}
	};

	while (!queue.empty()) {
		const Point current = queue.front();
		queue.pop();

		considerBest(current);

		const Displacement delta = destinationPosition - current;
		const int deltaAbsX = delta.deltaX >= 0 ? delta.deltaX : -delta.deltaX;
		const int deltaAbsY = delta.deltaY >= 0 ? delta.deltaY : -delta.deltaY;

		std::array<int8_t, 8> prioritizedDirs;
		size_t prioritizedCount = 0;

		const auto addUniqueDir = [&](int8_t dir) {
			if (dir == WALK_NONE)
				return;
			for (size_t i = 0; i < prioritizedCount; ++i) {
				if (prioritizedDirs[i] == dir)
					return;
			}
			prioritizedDirs[prioritizedCount++] = dir;
		};

		const int8_t xDir = delta.deltaX > 0 ? WALK_SE : (delta.deltaX < 0 ? WALK_NW : WALK_NONE);
		const int8_t yDir = delta.deltaY > 0 ? WALK_SW : (delta.deltaY < 0 ? WALK_NE : WALK_NONE);

		if (allowDiagonalSteps && delta.deltaX != 0 && delta.deltaY != 0) {
			const int8_t diagDir = delta.deltaX > 0 ? (delta.deltaY > 0 ? WALK_S : WALK_E) : (delta.deltaY > 0 ? WALK_W : WALK_N);
			addUniqueDir(diagDir);
		}

		if (deltaAbsX >= deltaAbsY) {
			addUniqueDir(xDir);
			addUniqueDir(yDir);
		} else {
			addUniqueDir(yDir);
			addUniqueDir(xDir);
		}
		for (const int8_t dir : walkDirections) {
			addUniqueDir(dir);
		}

		for (size_t i = 0; i < prioritizedCount; ++i) {
			enqueue(current, prioritizedDirs[i]);
		}
	}

	closestPosition = best;
	if (best == startPosition)
		return std::vector<int8_t> {};

	std::vector<int8_t> path;
	Point position = best;
	while (position != startPosition) {
		const int8_t dir = parentDir[indexOf(position)];
		if (dir == WALK_NONE)
			return std::nullopt;

		path.push_back(dir);
		position = NextPositionForWalkDirection(position, OppositeWalkDirection(dir));
	}

	std::reverse(path.begin(), path.end());
	return path;
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

std::optional<int> FindPreferredExitTriggerIndex()
{
	if (numtrigs <= 0)
		return std::nullopt;

	if (leveltype == DTYPE_TOWN && MyPlayer != nullptr) {
		const Point playerPosition = MyPlayer->position.future;
		std::optional<int> bestIndex;
		int bestDistance = 0;

		for (int i = 0; i < numtrigs; ++i) {
			if (!IsAnyOf(trigs[i]._tmsg, WM_DIABNEXTLVL, WM_DIABTOWNWARP))
				continue;

			const Point triggerPosition { trigs[i].position.x, trigs[i].position.y };
			const int distance = playerPosition.WalkingDistance(triggerPosition);
			if (!bestIndex || distance < bestDistance) {
				bestIndex = i;
				bestDistance = distance;
			}
		}

		if (bestIndex)
			return bestIndex;
	}

	const Point playerPosition = MyPlayer->position.future;
	std::optional<int> bestIndex;
	int bestDistance = 0;

	for (int i = 0; i < numtrigs; ++i) {
		const Point triggerPosition { trigs[i].position.x, trigs[i].position.y };
		const int distance = playerPosition.WalkingDistance(triggerPosition);
		if (!bestIndex || distance < bestDistance) {
			bestIndex = i;
			bestDistance = distance;
		}
	}

	return bestIndex;
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

} // namespace

std::string BuildCurrentLocationForSpeech()
{
	// Quest Level Name
	if (setlevel) {
		const char *const questLevelName = QuestLevelNames[setlvlnum];
		if (questLevelName == nullptr || questLevelName[0] == '\0')
			return std::string { _("Set level") };

		return fmt::format("{:s}: {:s}", _("Set level"), _(questLevelName));
	}

	// Dungeon Name
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
	if (leveltype >= DTYPE_TOWN && leveltype <= DTYPE_LAST) {
		dungeonStr = _(DungeonStrs[static_cast<size_t>(leveltype)]);
	} else {
		dungeonStr = _(/* TRANSLATORS: type of dungeon (i.e. Cathedral, Caves)*/ "None");
	}

	if (leveltype == DTYPE_TOWN || currlevel <= 0)
		return dungeonStr;

	// Dungeon Level
	int level = currlevel;
	if (leveltype == DTYPE_CATACOMBS)
		level -= 4;
	else if (leveltype == DTYPE_CAVES)
		level -= 8;
	else if (leveltype == DTYPE_HELL)
		level -= 12;
	else if (leveltype == DTYPE_NEST)
		level -= 16;
	else if (leveltype == DTYPE_CRYPT)
		level -= 20;

	if (level <= 0)
		return dungeonStr;

	return fmt::format(fmt::runtime(_(/* TRANSLATORS: dungeon type and floor number i.e. "Cathedral 3"*/ "{} {}")), dungeonStr, level);
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeech(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable)
{
	return FindKeyboardWalkPathForSpeechWithPosOk(player, startPosition, destinationPosition, PosOkPlayerIgnoreDoors, allowDestinationNonWalkable);
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechRespectingDoors(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable)
{
	return FindKeyboardWalkPathForSpeechWithPosOk(player, startPosition, destinationPosition, PosOkPlayer, allowDestinationNonWalkable);
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechIgnoringMonsters(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable)
{
	return FindKeyboardWalkPathForSpeechWithPosOk(player, startPosition, destinationPosition, PosOkPlayerIgnoreDoorsAndMonsters, allowDestinationNonWalkable);
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechRespectingDoorsIgnoringMonsters(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable)
{
	return FindKeyboardWalkPathForSpeechWithPosOk(player, startPosition, destinationPosition, PosOkPlayerIgnoreMonsters, allowDestinationNonWalkable);
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathForSpeechLenient(const Player &player, Point startPosition, Point destinationPosition, bool allowDestinationNonWalkable)
{
	return FindKeyboardWalkPathForSpeechWithPosOk(player, startPosition, destinationPosition, PosOkPlayerIgnoreDoorsMonstersAndBreakables, allowDestinationNonWalkable);
}

std::optional<std::vector<int8_t>> FindKeyboardWalkPathToClosestReachableForSpeech(const Player &player, Point startPosition, Point destinationPosition, Point &closestPosition)
{
	constexpr std::array<int8_t, 4> AxisDirections = {
		WALK_NE,
		WALK_SW,
		WALK_SE,
		WALK_NW,
	};

	constexpr std::array<int8_t, 8> AllDirections = {
		WALK_NE,
		WALK_SW,
		WALK_SE,
		WALK_NW,
		WALK_N,
		WALK_E,
		WALK_S,
		WALK_W,
	};

	Point axisClosest;
	const std::optional<std::vector<int8_t>> axisPath = FindKeyboardWalkPathToClosestReachableForSpeechBfs(player, startPosition, destinationPosition, PosOkPlayerIgnoreDoors, AxisDirections, /*allowDiagonalSteps=*/false, axisClosest);

	Point diagClosest;
	const std::optional<std::vector<int8_t>> diagPath = FindKeyboardWalkPathToClosestReachableForSpeechBfs(player, startPosition, destinationPosition, PosOkPlayerIgnoreDoors, AllDirections, /*allowDiagonalSteps=*/true, diagClosest);

	if (!axisPath && !diagPath)
		return std::nullopt;
	if (!axisPath) {
		closestPosition = diagClosest;
		return diagPath;
	}
	if (!diagPath) {
		closestPosition = axisClosest;
		return axisPath;
	}

	const int axisDistance = axisClosest.WalkingDistance(destinationPosition);
	const int diagDistance = diagClosest.WalkingDistance(destinationPosition);
	if (diagDistance < axisDistance) {
		closestPosition = diagClosest;
		return diagPath;
	}

	closestPosition = axisClosest;
	return axisPath;
}

void AppendKeyboardWalkPathForSpeech(std::string &message, const std::vector<int8_t> &path)
{
	if (path.empty()) {
		message.append(_("here"));
		return;
	}

	bool any = false;
	const auto appendPart = [&](std::string_view label, int distance) {
		if (distance == 0)
			return;
		if (any)
			message.append(", ");
		StrAppend(message, label, " ", distance);
		any = true;
	};

	const auto labelForWalkDirection = [](int8_t dir) -> std::string_view {
		switch (dir) {
		case WALK_NE:
			return _("north");
		case WALK_SW:
			return _("south");
		case WALK_SE:
			return _("east");
		case WALK_NW:
			return _("west");
		case WALK_N:
			return _("northwest");
		case WALK_E:
			return _("northeast");
		case WALK_S:
			return _("southeast");
		case WALK_W:
			return _("southwest");
		default:
			return {};
		}
	};

	int8_t currentDir = path.front();
	int runLength = 1;
	for (size_t i = 1; i < path.size(); ++i) {
		if (path[i] == currentDir) {
			++runLength;
			continue;
		}

		const std::string_view label = labelForWalkDirection(currentDir);
		if (!label.empty())
			appendPart(label, runLength);

		currentDir = path[i];
		runLength = 1;
	}

	const std::string_view label = labelForWalkDirection(currentDir);
	if (!label.empty())
		appendPart(label, runLength);

	if (!any)
		message.append(_("here"));
}

void AppendDirectionalFallback(std::string &message, const Displacement &delta)
{
	bool any = false;
	const auto appendPart = [&](std::string_view label, int distance) {
		if (distance == 0)
			return;
		if (any)
			message.append(", ");
		StrAppend(message, label, " ", distance);
		any = true;
	};

	if (delta.deltaY < 0)
		appendPart(_("north"), -delta.deltaY);
	else if (delta.deltaY > 0)
		appendPart(_("south"), delta.deltaY);

	if (delta.deltaX > 0)
		appendPart(_("east"), delta.deltaX);
	else if (delta.deltaX < 0)
		appendPart(_("west"), -delta.deltaX);

	if (!any)
		message.append(_("here"));
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

void SpeakPlayerHealthPercentageKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (MyPlayer == nullptr)
		return;

	const int maxHp = MyPlayer->_pMaxHP;
	if (maxHp <= 0)
		return;

	const int currentHp = std::max(MyPlayer->_pHitPoints, 0);
	int hpPercent = static_cast<int>((static_cast<int64_t>(currentHp) * 100 + maxHp / 2) / maxHp);
	hpPercent = std::clamp(hpPercent, 0, 100);
	SpeakText(fmt::format("{:d}%", hpPercent), /*force=*/true);
}

void SpeakExperienceToNextLevelKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (MyPlayer == nullptr)
		return;

	const Player &myPlayer = *MyPlayer;
	if (myPlayer.isMaxCharacterLevel()) {
		SpeakText(_("Max level."), /*force=*/true);
		return;
	}

	const uint32_t nextExperienceThreshold = myPlayer.getNextExperienceThreshold();
	const uint32_t currentExperience = myPlayer._pExperience;
	const uint32_t remainingExperience = currentExperience >= nextExperienceThreshold ? 0 : nextExperienceThreshold - currentExperience;
	const int nextLevel = myPlayer.getCharacterLevel() + 1;
	SpeakText(
	    fmt::format(fmt::runtime(_("{:s} to Level {:d}")), FormatInteger(remainingExperience), nextLevel),
	    /*force=*/true);
}

void SpeakCurrentLocationKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;

	SpeakText(BuildCurrentLocationForSpeech(), /*force=*/true);
}

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

			std::string message = std::string(_(QuestLevelNames[entrance->questLevel]));
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

	if (leveltype != DTYPE_TOWN) {
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
		return;
	}

	const std::optional<int> triggerIndex = FindPreferredExitTriggerIndex();
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

void KeyboardWalkKeyPressed(Direction direction)
{
	CancelAutoWalk();
	if (!IsKeyboardWalkAllowed())
		return;

	if (MyPlayer == nullptr)
		return;

	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, MyPlayer->position.future + direction);
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

} // namespace devilution
