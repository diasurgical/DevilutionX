/**
 * @file utils/walk_path_speech.cpp
 *
 * Walk-path helpers, PosOk variants, and BFS pathfinding for accessibility speech.
 */
#include "utils/walk_path_speech.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <vector>

#include "engine/path.h"
#include "levels/gendung.h"
#include "levels/tile_properties.hpp"
#include "monster.h"
#include "objects.h"
#include "player.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution {

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

Point PositionAfterWalkPathSteps(Point start, const int8_t *path, int steps)
{
	Point position = start;
	for (int i = 0; i < steps; ++i) {
		position = NextPositionForWalkDirection(position, path[i]);
	}
	return position;
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

bool PosOkPlayerIgnoreDoors(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (!IsTileWalkable(position, /*ignoreDoors=*/true))
		return false;

	Player *otherPlayer = PlayerAtPosition(position);
	if (otherPlayer != nullptr && otherPlayer != &player && !otherPlayer->hasNoLife())
		return false;

	if (dMonster[position.x][position.y] != 0) {
		if (leveltype == DTYPE_TOWN)
			return false;
		if (dMonster[position.x][position.y] <= 0)
			return false;
		if (!Monsters[dMonster[position.x][position.y] - 1].hasNoLife())
			return false;
	}

	return true;
}

bool IsTileWalkableForTrackerPath(Point position, bool ignoreDoors, bool ignoreBreakables)
{
	Object *object = FindObjectAtPosition(position);
	if (object != nullptr) {
		if (ignoreDoors && object->isDoor()) {
			return true;
		}
		if (ignoreBreakables && object->_oSolidFlag && object->IsBreakable()) {
			return true;
		}
		if (object->_oSolidFlag) {
			return false;
		}
	}

	return IsTileNotSolid(position);
}

bool PosOkPlayerIgnoreMonsters(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (!IsTileWalkableForTrackerPath(position, /*ignoreDoors=*/false, /*ignoreBreakables=*/false))
		return false;

	Player *otherPlayer = PlayerAtPosition(position);
	if (otherPlayer != nullptr && otherPlayer != &player && !otherPlayer->hasNoLife())
		return false;

	return true;
}

bool PosOkPlayerIgnoreDoorsAndMonsters(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (!IsTileWalkableForTrackerPath(position, /*ignoreDoors=*/true, /*ignoreBreakables=*/false))
		return false;

	Player *otherPlayer = PlayerAtPosition(position);
	if (otherPlayer != nullptr && otherPlayer != &player && !otherPlayer->hasNoLife())
		return false;

	return true;
}

bool PosOkPlayerIgnoreDoorsMonstersAndBreakables(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (!IsTileWalkableForTrackerPath(position, /*ignoreDoors=*/true, /*ignoreBreakables=*/true))
		return false;

	Player *otherPlayer = PlayerAtPosition(position);
	if (otherPlayer != nullptr && otherPlayer != &player && !otherPlayer->hasNoLife())
		return false;

	return true;
}

namespace {

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

} // namespace

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

} // namespace devilution
