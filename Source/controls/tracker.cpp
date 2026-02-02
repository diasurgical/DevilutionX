/**
 * @file controls/tracker.cpp
 *
 * Tracker system for accessibility: target cycling, pathfinding, and auto-walk.
 */
#include "controls/tracker.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#ifdef USE_SDL3
#include <SDL3/SDL_keycode.h>
#else
#include <SDL.h>
#endif

#include "appfat.h"
#include "automap.h"
#include "controls/accessibility_keys.hpp"
#include "controls/plrctrls.h"
#include "diablo.h"
#include "engine/path.h"
#include "gamemenu.h"
#include "help.h"
#include "items.h"
#include "levels/gendung.h"
#include "levels/tile_properties.hpp"
#include "monster.h"
#include "multi.h"
#include "objects.h"
#include "player.h"
#include "qol/chatlog.h"
#include "stores.h"
#include "utils/accessibility_announcements.hpp"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/screen_reader.hpp"
#include "utils/sdl_compat.h"
#include "utils/str_cat.hpp"
#include "utils/string_or_view.hpp"
#include "utils/walk_path_speech.hpp"

namespace devilution {

namespace {

TrackerTargetCategory SelectedTrackerTargetCategory = TrackerTargetCategory::Items;

TrackerTargetCategory AutoWalkTrackerTargetCategory = TrackerTargetCategory::Items; ///< Category of the active auto-walk target.
int AutoWalkTrackerTargetId = -1;                                                   ///< ID of the target being auto-walked to, or -1 if inactive.

/// Maximum Chebyshev distance (in tiles) at which the player is considered
/// close enough to interact with a tracker target.
constexpr int TrackerInteractDistanceTiles = 1;
constexpr int TrackerCycleDistanceTiles = 12;

int LockedTrackerItemId = -1;
int LockedTrackerChestId = -1;
int LockedTrackerDoorId = -1;
int LockedTrackerShrineId = -1;
int LockedTrackerObjectId = -1;
int LockedTrackerBreakableId = -1;
int LockedTrackerMonsterId = -1;
int LockedTrackerDeadBodyId = -1;

struct TrackerLevelKey {
	dungeon_type levelType;
	int currLevel;
	bool isSetLevel;
	int setLevelNum;
};

std::optional<TrackerLevelKey> LockedTrackerLevelKey;

void ClearTrackerLocks()
{
	LockedTrackerItemId = -1;
	LockedTrackerChestId = -1;
	LockedTrackerDoorId = -1;
	LockedTrackerShrineId = -1;
	LockedTrackerObjectId = -1;
	LockedTrackerBreakableId = -1;
	LockedTrackerMonsterId = -1;
	LockedTrackerDeadBodyId = -1;
}

void EnsureTrackerLocksMatchCurrentLevel()
{
	const TrackerLevelKey current {
		.levelType = leveltype,
		.currLevel = currlevel,
		.isSetLevel = setlevel,
		.setLevelNum = setlvlnum,
	};

	if (!LockedTrackerLevelKey || LockedTrackerLevelKey->levelType != current.levelType || LockedTrackerLevelKey->currLevel != current.currLevel
	    || LockedTrackerLevelKey->isSetLevel != current.isSetLevel || LockedTrackerLevelKey->setLevelNum != current.setLevelNum) {
		ClearTrackerLocks();
		LockedTrackerLevelKey = current;
	}
}

int &LockedTrackerTargetId(TrackerTargetCategory category)
{
	switch (category) {
	case TrackerTargetCategory::Items:
		return LockedTrackerItemId;
	case TrackerTargetCategory::Chests:
		return LockedTrackerChestId;
	case TrackerTargetCategory::Doors:
		return LockedTrackerDoorId;
	case TrackerTargetCategory::Shrines:
		return LockedTrackerShrineId;
	case TrackerTargetCategory::Objects:
		return LockedTrackerObjectId;
	case TrackerTargetCategory::Breakables:
		return LockedTrackerBreakableId;
	case TrackerTargetCategory::Monsters:
		return LockedTrackerMonsterId;
	case TrackerTargetCategory::DeadBodies:
		return LockedTrackerDeadBodyId;
	}
	app_fatal("Invalid TrackerTargetCategory");
}

std::string_view TrackerTargetCategoryLabel(TrackerTargetCategory category)
{
	switch (category) {
	case TrackerTargetCategory::Items:
		return _("items");
	case TrackerTargetCategory::Chests:
		return _("chests");
	case TrackerTargetCategory::Doors:
		return _("doors");
	case TrackerTargetCategory::Shrines:
		return _("shrines");
	case TrackerTargetCategory::Objects:
		return _("objects");
	case TrackerTargetCategory::Breakables:
		return _("breakables");
	case TrackerTargetCategory::Monsters:
		return _("monsters");
	case TrackerTargetCategory::DeadBodies:
		return _("dead bodies");
	default:
		return _("items");
	}
}

void SpeakTrackerTargetCategory()
{
	std::string message;
	StrAppend(message, _("Tracker target: "), TrackerTargetCategoryLabel(SelectedTrackerTargetCategory));
	SpeakText(message, true);
}

std::optional<int> FindNearestGroundItemId(Point playerPosition)
{
	std::optional<int> bestId;
	int bestDistance = 0;

	for (int y = 0; y < MAXDUNY; ++y) {
		for (int x = 0; x < MAXDUNX; ++x) {
			const int itemId = std::abs(dItem[x][y]) - 1;
			if (itemId < 0 || itemId > MAXITEMS)
				continue;

			const Item &item = Items[itemId];
			if (item.isEmpty() || item._iClass == ICLASS_NONE)
				continue;

			const int distance = playerPosition.WalkingDistance(Point { x, y });
			if (!bestId || distance < bestDistance) {
				bestId = itemId;
				bestDistance = distance;
			}
		}
	}

	return bestId;
}

[[nodiscard]] constexpr int CorpseTrackerIdForPosition(Point position)
{
	return position.x + position.y * MAXDUNX;
}

[[nodiscard]] constexpr Point CorpsePositionForTrackerId(int corpseId)
{
	return { corpseId % MAXDUNX, corpseId / MAXDUNX };
}

std::optional<int> FindNearestCorpseId(Point playerPosition)
{
	std::optional<int> bestId;
	int bestDistance = 0;

	for (int y = 0; y < MAXDUNY; ++y) {
		for (int x = 0; x < MAXDUNX; ++x) {
			if (dCorpse[x][y] == 0)
				continue;

			const Point position { x, y };
			const int distance = playerPosition.WalkingDistance(position);
			if (!bestId || distance < bestDistance) {
				bestId = CorpseTrackerIdForPosition(position);
				bestDistance = distance;
			}
		}
	}

	return bestId;
}

struct TrackerCandidate {
	int id;
	int distance;
	StringOrView name;
};

[[nodiscard]] bool IsBetterTrackerCandidate(const TrackerCandidate &a, const TrackerCandidate &b)
{
	if (a.distance != b.distance)
		return a.distance < b.distance;
	return a.id < b.id;
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyItemTrackerCandidates(Point playerPosition, int maxDistance)
{
	std::vector<TrackerCandidate> result;
	result.reserve(ActiveItemCount);

	const int minX = std::max(0, playerPosition.x - maxDistance);
	const int minY = std::max(0, playerPosition.y - maxDistance);
	const int maxX = std::min(MAXDUNX - 1, playerPosition.x + maxDistance);
	const int maxY = std::min(MAXDUNY - 1, playerPosition.y + maxDistance);

	std::array<bool, MAXITEMS + 1> seen {};

	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			const int itemId = std::abs(dItem[x][y]) - 1;
			if (itemId < 0 || itemId > MAXITEMS)
				continue;
			if (seen[itemId])
				continue;
			seen[itemId] = true;

			const Item &item = Items[itemId];
			if (item.isEmpty() || item._iClass == ICLASS_NONE)
				continue;

			const int distance = playerPosition.WalkingDistance(Point { x, y });
			if (distance > maxDistance)
				continue;

			result.push_back(TrackerCandidate {
			    .id = itemId,
			    .distance = distance,
			    .name = item.getName(),
			});
		}
	}

	std::sort(result.begin(), result.end(), [](const TrackerCandidate &a, const TrackerCandidate &b) { return IsBetterTrackerCandidate(a, b); });
	return result;
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyCorpseTrackerCandidates(Point playerPosition, int maxDistance)
{
	std::vector<TrackerCandidate> result;

	const int minX = std::max(0, playerPosition.x - maxDistance);
	const int minY = std::max(0, playerPosition.y - maxDistance);
	const int maxX = std::min(MAXDUNX - 1, playerPosition.x + maxDistance);
	const int maxY = std::min(MAXDUNY - 1, playerPosition.y + maxDistance);

	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			if (dCorpse[x][y] == 0)
				continue;

			const Point position { x, y };
			const int distance = playerPosition.WalkingDistance(position);
			if (distance > maxDistance)
				continue;

			result.push_back(TrackerCandidate {
			    .id = CorpseTrackerIdForPosition(position),
			    .distance = distance,
			    .name = _("Dead body"),
			});
		}
	}

	std::sort(result.begin(), result.end(), [](const TrackerCandidate &a, const TrackerCandidate &b) { return IsBetterTrackerCandidate(a, b); });
	return result;
}

[[nodiscard]] constexpr bool IsTrackedChestObject(const Object &object)
{
	return object.canInteractWith() && (object.IsChest() || object._otype == _object_id::OBJ_SIGNCHEST);
}

[[nodiscard]] constexpr bool IsTrackedDoorObject(const Object &object)
{
	return object.isDoor() && object.canInteractWith();
}

[[nodiscard]] constexpr bool IsShrineLikeObject(const Object &object)
{
	return object.canInteractWith()
	    && (object.IsShrine()
	        || IsAnyOf(object._otype, _object_id::OBJ_BLOODFTN, _object_id::OBJ_PURIFYINGFTN, _object_id::OBJ_GOATSHRINE, _object_id::OBJ_CAULDRON,
	            _object_id::OBJ_MURKYFTN, _object_id::OBJ_TEARFTN));
}

[[nodiscard]] constexpr bool IsTrackedBreakableObject(const Object &object)
{
	return object.IsBreakable();
}

[[nodiscard]] constexpr bool IsTrackedMiscInteractableObject(const Object &object)
{
	if (!object.canInteractWith())
		return false;
	if (object.IsChest() || object._otype == _object_id::OBJ_SIGNCHEST)
		return false;
	if (object.isDoor())
		return false;
	if (IsShrineLikeObject(object))
		return false;
	if (object.IsBreakable())
		return false;
	return true;
}

[[nodiscard]] bool IsTrackedMonster(const Monster &monster)
{
	return !monster.isInvalid
	    && (monster.flags & MFLAG_HIDDEN) == 0
	    && monster.hitPoints > 0;
}

template <typename Predicate>
[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyObjectTrackerCandidates(Point playerPosition, int maxDistance, Predicate predicate)
{
	std::vector<TrackerCandidate> result;
	result.reserve(ActiveObjectCount);

	const int minX = std::max(0, playerPosition.x - maxDistance);
	const int minY = std::max(0, playerPosition.y - maxDistance);
	const int maxX = std::min(MAXDUNX - 1, playerPosition.x + maxDistance);
	const int maxY = std::min(MAXDUNY - 1, playerPosition.y + maxDistance);

	std::array<int, MAXOBJECTS> bestDistanceById {};
	bestDistanceById.fill(std::numeric_limits<int>::max());

	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			const int objectId = std::abs(dObject[x][y]) - 1;
			if (objectId < 0 || objectId >= MAXOBJECTS)
				continue;

			const Object &object = Objects[objectId];
			if (object._otype == OBJ_NULL)
				continue;
			if (!predicate(object))
				continue;

			const int distance = playerPosition.WalkingDistance(Point { x, y });
			if (distance > maxDistance)
				continue;

			int &bestDistance = bestDistanceById[objectId];
			if (distance < bestDistance)
				bestDistance = distance;
		}
	}

	for (int objectId = 0; objectId < MAXOBJECTS; ++objectId) {
		const int distance = bestDistanceById[objectId];
		if (distance == std::numeric_limits<int>::max())
			continue;

		const Object &object = Objects[objectId];
		result.push_back(TrackerCandidate {
		    .id = objectId,
		    .distance = distance,
		    .name = object.name(),
		});
	}

	std::sort(result.begin(), result.end(), [](const TrackerCandidate &a, const TrackerCandidate &b) { return IsBetterTrackerCandidate(a, b); });
	return result;
}

template <typename Predicate>
[[nodiscard]] std::optional<int> FindNearestObjectId(Point playerPosition, Predicate predicate)
{
	std::array<int, MAXOBJECTS> bestDistanceById {};
	bestDistanceById.fill(std::numeric_limits<int>::max());

	for (int y = 0; y < MAXDUNY; ++y) {
		for (int x = 0; x < MAXDUNX; ++x) {
			const int objectId = std::abs(dObject[x][y]) - 1;
			if (objectId < 0 || objectId >= MAXOBJECTS)
				continue;

			const Object &object = Objects[objectId];
			if (object._otype == OBJ_NULL)
				continue;
			if (!predicate(object))
				continue;

			const int distance = playerPosition.WalkingDistance(Point { x, y });
			int &bestDistance = bestDistanceById[objectId];
			if (distance < bestDistance)
				bestDistance = distance;
		}
	}

	std::optional<int> bestId;
	int bestDistance = 0;
	for (int objectId = 0; objectId < MAXOBJECTS; ++objectId) {
		const int distance = bestDistanceById[objectId];
		if (distance == std::numeric_limits<int>::max())
			continue;

		if (!bestId || distance < bestDistance) {
			bestId = objectId;
			bestDistance = distance;
		}
	}

	return bestId;
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyChestTrackerCandidates(Point playerPosition, int maxDistance)
{
	return CollectNearbyObjectTrackerCandidates(playerPosition, maxDistance, IsTrackedChestObject);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyDoorTrackerCandidates(Point playerPosition, int maxDistance)
{
	return CollectNearbyObjectTrackerCandidates(playerPosition, maxDistance, IsTrackedDoorObject);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyShrineTrackerCandidates(Point playerPosition, int maxDistance)
{
	return CollectNearbyObjectTrackerCandidates(playerPosition, maxDistance, IsShrineLikeObject);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyBreakableTrackerCandidates(Point playerPosition, int maxDistance)
{
	return CollectNearbyObjectTrackerCandidates(playerPosition, maxDistance, IsTrackedBreakableObject);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyObjectInteractableTrackerCandidates(Point playerPosition, int maxDistance)
{
	return CollectNearbyObjectTrackerCandidates(playerPosition, maxDistance, IsTrackedMiscInteractableObject);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNearbyMonsterTrackerCandidates(Point playerPosition, int maxDistance)
{
	std::vector<TrackerCandidate> result;
	result.reserve(ActiveMonsterCount);

	for (size_t i = 0; i < ActiveMonsterCount; ++i) {
		const int monsterId = static_cast<int>(ActiveMonsters[i]);
		const Monster &monster = Monsters[monsterId];

		if (monster.isInvalid)
			continue;
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			continue;
		if (monster.hitPoints <= 0)
			continue;

		const Point monsterDistancePosition { monster.position.future };
		const int distance = playerPosition.ApproxDistance(monsterDistancePosition);
		if (distance > maxDistance)
			continue;

		result.push_back(TrackerCandidate {
		    .id = monsterId,
		    .distance = distance,
		    .name = monster.name(),
		});
	}

	std::sort(result.begin(), result.end(), [](const TrackerCandidate &a, const TrackerCandidate &b) { return IsBetterTrackerCandidate(a, b); });
	return result;
}

[[nodiscard]] std::optional<int> FindNextTrackerCandidateId(const std::vector<TrackerCandidate> &candidates, int currentId)
{
	if (candidates.empty())
		return std::nullopt;
	if (currentId < 0)
		return candidates.front().id;

	const auto it = std::find_if(candidates.begin(), candidates.end(), [currentId](const TrackerCandidate &c) { return c.id == currentId; });
	if (it == candidates.end())
		return candidates.front().id;

	if (candidates.size() <= 1)
		return std::nullopt;

	const size_t idx = static_cast<size_t>(it - candidates.begin());
	const size_t nextIdx = (idx + 1) % candidates.size();
	return candidates[nextIdx].id;
}

void DecorateTrackerTargetNameWithOrdinalIfNeeded(int targetId, StringOrView &targetName, const std::vector<TrackerCandidate> &candidates)
{
	if (targetName.empty())
		return;

	const std::string_view baseName = targetName.str();
	int total = 0;
	for (const TrackerCandidate &c : candidates) {
		if (c.name.str() == baseName)
			++total;
	}
	if (total <= 1)
		return;

	int ordinal = 0;
	int seen = 0;
	for (const TrackerCandidate &c : candidates) {
		if (c.name.str() != baseName)
			continue;
		++seen;
		if (c.id == targetId) {
			ordinal = seen;
			break;
		}
	}
	if (ordinal <= 0)
		return;

	std::string decorated;
	StrAppend(decorated, baseName, " ", ordinal);
	targetName = std::move(decorated);
}

[[nodiscard]] bool IsGroundItemPresent(int itemId)
{
	if (itemId < 0 || itemId > MAXITEMS)
		return false;

	for (uint8_t i = 0; i < ActiveItemCount; ++i) {
		if (ActiveItems[i] == itemId)
			return true;
	}

	return false;
}

[[nodiscard]] bool IsCorpsePresent(int corpseId)
{
	if (corpseId < 0 || corpseId >= MAXDUNX * MAXDUNY)
		return false;

	const Point position = CorpsePositionForTrackerId(corpseId);
	return InDungeonBounds(position) && dCorpse[position.x][position.y] != 0;
}

std::optional<int> FindNearestUnopenedChestObjectId(Point playerPosition)
{
	return FindNearestObjectId(playerPosition, IsTrackedChestObject);
}

std::optional<int> FindNearestDoorObjectId(Point playerPosition)
{
	return FindNearestObjectId(playerPosition, IsTrackedDoorObject);
}

std::optional<int> FindNearestShrineObjectId(Point playerPosition)
{
	return FindNearestObjectId(playerPosition, IsShrineLikeObject);
}

std::optional<int> FindNearestBreakableObjectId(Point playerPosition)
{
	return FindNearestObjectId(playerPosition, IsTrackedBreakableObject);
}

std::optional<int> FindNearestMiscInteractableObjectId(Point playerPosition)
{
	return FindNearestObjectId(playerPosition, IsTrackedMiscInteractableObject);
}

std::optional<int> FindNearestMonsterId(Point playerPosition)
{
	std::optional<int> bestId;
	int bestDistance = 0;

	for (size_t i = 0; i < ActiveMonsterCount; ++i) {
		const int monsterId = static_cast<int>(ActiveMonsters[i]);
		const Monster &monster = Monsters[monsterId];

		if (monster.isInvalid)
			continue;
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			continue;
		if (monster.hitPoints <= 0)
			continue;

		const Point monsterDistancePosition { monster.position.future };
		const int distance = playerPosition.ApproxDistance(monsterDistancePosition);
		if (!bestId || distance < bestDistance) {
			bestId = monsterId;
			bestDistance = distance;
		}
	}

	return bestId;
}

std::optional<Point> FindBestAdjacentApproachTile(const Player &player, Point playerPosition, Point targetPosition)
{
	std::optional<Point> best;
	size_t bestPathLength = 0;
	int bestDistance = 0;

	std::optional<Point> bestFallback;
	int bestFallbackDistance = 0;

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0)
				continue;

			const Point tile { targetPosition.x + dx, targetPosition.y + dy };
			if (!PosOkPlayer(player, tile))
				continue;

			const int distance = playerPosition.WalkingDistance(tile);

			if (!bestFallback || distance < bestFallbackDistance) {
				bestFallback = tile;
				bestFallbackDistance = distance;
			}

			const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(player, playerPosition, tile);
			if (!path)
				continue;

			const size_t pathLength = path->size();
			if (!best || pathLength < bestPathLength || (pathLength == bestPathLength && distance < bestDistance)) {
				best = tile;
				bestPathLength = pathLength;
				bestDistance = distance;
			}
		}
	}

	if (best)
		return best;

	return bestFallback;
}

std::optional<Point> FindBestApproachTileForObject(const Player &player, Point playerPosition, const Object &object)
{
	if (!object._oSolidFlag && PosOkPlayer(player, object.position))
		return object.position;

	std::optional<Point> best;
	size_t bestPathLength = 0;
	int bestDistance = 0;

	std::optional<Point> bestFallback;
	int bestFallbackDistance = 0;

	const auto considerTile = [&](Point tile) {
		if (!PosOkPlayerIgnoreDoors(player, tile))
			return;

		const int distance = playerPosition.WalkingDistance(tile);
		if (!bestFallback || distance < bestFallbackDistance) {
			bestFallback = tile;
			bestFallbackDistance = distance;
		}

		const std::optional<std::vector<int8_t>> path = FindKeyboardWalkPathForSpeech(player, playerPosition, tile);
		if (!path)
			return;

		const size_t pathLength = path->size();
		if (!best || pathLength < bestPathLength || (pathLength == bestPathLength && distance < bestDistance)) {
			best = tile;
			bestPathLength = pathLength;
			bestDistance = distance;
		}
	};

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0)
				continue;
			considerTile(object.position + Displacement { dx, dy });
		}
	}

	if (FindObjectAtPosition(object.position + Direction::NorthEast) == &object) {
		for (int dx = -1; dx <= 1; ++dx) {
			considerTile(object.position + Displacement { dx, -2 });
		}
	}

	if (best)
		return best;

	return bestFallback;
}

struct DoorBlockInfo {
	Point beforeDoor;
	Point doorPosition;
};

std::optional<DoorBlockInfo> FindFirstClosedDoorOnWalkPath(Point startPosition, const int8_t *path, int steps)
{
	Point position = startPosition;
	for (int i = 0; i < steps; ++i) {
		const Point next = NextPositionForWalkDirection(position, path[i]);
		Object *object = FindObjectAtPosition(next);
		if (object != nullptr && object->isDoor() && object->_oVar4 == DOOR_CLOSED) {
			return DoorBlockInfo { .beforeDoor = position, .doorPosition = object->position };
		}
		position = next;
	}
	return std::nullopt;
}

enum class TrackerPathBlockType : uint8_t {
	Door,
	Monster,
	Breakable,
};

struct TrackerPathBlockInfo {
	TrackerPathBlockType type;
	size_t stepIndex;
	Point beforeBlock;
	Point blockPosition;
};

[[nodiscard]] std::optional<TrackerPathBlockInfo> FindFirstTrackerPathBlock(Point startPosition, const int8_t *path, size_t steps, bool considerDoors, bool considerMonsters, bool considerBreakables, Point targetPosition)
{
	Point position = startPosition;
	for (size_t i = 0; i < steps; ++i) {
		const Point next = NextPositionForWalkDirection(position, path[i]);
		if (next == targetPosition) {
			position = next;
			continue;
		}

		Object *object = FindObjectAtPosition(next);
		if (considerDoors && object != nullptr && object->isDoor() && object->_oVar4 == DOOR_CLOSED) {
			return TrackerPathBlockInfo {
				.type = TrackerPathBlockType::Door,
				.stepIndex = i,
				.beforeBlock = position,
				.blockPosition = object->position,
			};
		}
		if (considerBreakables && object != nullptr && object->_oSolidFlag && object->IsBreakable()) {
			return TrackerPathBlockInfo {
				.type = TrackerPathBlockType::Breakable,
				.stepIndex = i,
				.beforeBlock = position,
				.blockPosition = next,
			};
		}

		if (considerMonsters && leveltype != DTYPE_TOWN && dMonster[next.x][next.y] != 0) {
			const int monsterRef = dMonster[next.x][next.y];
			const int monsterId = std::abs(monsterRef) - 1;
			const bool blocks = monsterRef <= 0 || (monsterId >= 0 && monsterId < static_cast<int>(MaxMonsters) && !Monsters[monsterId].hasNoLife());
			if (blocks) {
				return TrackerPathBlockInfo {
					.type = TrackerPathBlockType::Monster,
					.stepIndex = i,
					.beforeBlock = position,
					.blockPosition = next,
				};
			}
		}

		position = next;
	}

	return std::nullopt;
}

/**
 * Validates an object-category auto-walk target and computes the walk destination.
 */
template <typename Predicate>
bool ValidateAutoWalkObjectTarget(
    const Player &myPlayer, Point playerPosition,
    Predicate isValid, const char *goneMessage, const char *inRangeMessage,
    std::optional<Point> &destination)
{
	const int objectId = AutoWalkTrackerTargetId;
	if (objectId < 0 || objectId >= MAXOBJECTS) {
		AutoWalkTrackerTargetId = -1;
		SpeakText(_(goneMessage), true);
		return false;
	}
	const Object &object = Objects[objectId];
	if (!isValid(object)) {
		AutoWalkTrackerTargetId = -1;
		SpeakText(_(goneMessage), true);
		return false;
	}
	if (playerPosition.WalkingDistance(object.position) <= TrackerInteractDistanceTiles) {
		AutoWalkTrackerTargetId = -1;
		SpeakText(_(inRangeMessage), true);
		return false;
	}
	destination = FindBestAdjacentApproachTile(myPlayer, playerPosition, object.position);
	return true;
}

/**
 * Resolves which object to walk toward for the given tracker category.
 */
template <typename Predicate, typename FindNearest, typename GetName>
std::optional<int> ResolveObjectTrackerTarget(
    int &lockedTargetId, Point playerPosition,
    Predicate isValid, FindNearest findNearest, GetName getName,
    const char *notFoundMessage, StringOrView &targetName)
{
	std::optional<int> targetId;
	if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
		targetId = lockedTargetId;
	} else {
		targetId = findNearest(playerPosition);
	}
	if (!targetId) {
		SpeakText(_(notFoundMessage), true);
		return std::nullopt;
	}
	if (!isValid(Objects[*targetId])) {
		lockedTargetId = -1;
		targetId = findNearest(playerPosition);
		if (!targetId) {
			SpeakText(_(notFoundMessage), true);
			return std::nullopt;
		}
		if (!isValid(Objects[*targetId])) {
			SpeakText(_(notFoundMessage), true);
			return std::nullopt;
		}
	}
	lockedTargetId = *targetId;
	targetName = getName(*targetId);
	return targetId;
}

} // namespace

void CycleTrackerTargetKeyPressed()
{
	if (!CanPlayerTakeAction() || InGameMenu())
		return;

	AutoWalkTrackerTargetId = -1;

	const SDL_Keymod modState = SDL_GetModState();
	const bool cyclePrevious = (modState & SDL_KMOD_SHIFT) != 0;

	if (cyclePrevious) {
		switch (SelectedTrackerTargetCategory) {
		case TrackerTargetCategory::Items:
			SelectedTrackerTargetCategory = TrackerTargetCategory::DeadBodies;
			break;
		case TrackerTargetCategory::Chests:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Items;
			break;
		case TrackerTargetCategory::Doors:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Chests;
			break;
		case TrackerTargetCategory::Shrines:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Doors;
			break;
		case TrackerTargetCategory::Objects:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Shrines;
			break;
		case TrackerTargetCategory::Breakables:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Objects;
			break;
		case TrackerTargetCategory::Monsters:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Breakables;
			break;
		case TrackerTargetCategory::DeadBodies:
		default:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Monsters;
			break;
		}
	} else {
		switch (SelectedTrackerTargetCategory) {
		case TrackerTargetCategory::Items:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Chests;
			break;
		case TrackerTargetCategory::Chests:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Doors;
			break;
		case TrackerTargetCategory::Doors:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Shrines;
			break;
		case TrackerTargetCategory::Shrines:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Objects;
			break;
		case TrackerTargetCategory::Objects:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Breakables;
			break;
		case TrackerTargetCategory::Breakables:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Monsters;
			break;
		case TrackerTargetCategory::Monsters:
			SelectedTrackerTargetCategory = TrackerTargetCategory::DeadBodies;
			break;
		case TrackerTargetCategory::DeadBodies:
		default:
			SelectedTrackerTargetCategory = TrackerTargetCategory::Items;
			break;
		}
	}

	SpeakTrackerTargetCategory();
}

void NavigateToTrackerTargetKeyPressed()
{
	if (!CanPlayerTakeAction() || InGameMenu())
		return;
	if (leveltype == DTYPE_TOWN && IsNoneOf(SelectedTrackerTargetCategory, TrackerTargetCategory::Items, TrackerTargetCategory::DeadBodies)) {
		SpeakText(_("Not in a dungeon."), true);
		return;
	}
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (MyPlayer == nullptr)
		return;

	EnsureTrackerLocksMatchCurrentLevel();

	const SDL_Keymod modState = SDL_GetModState();
	const bool cycleTarget = (modState & SDL_KMOD_SHIFT) != 0;
	const bool clearTarget = (modState & SDL_KMOD_CTRL) != 0;

	const Point playerPosition = MyPlayer->position.future;
	AutoWalkTrackerTargetId = -1;

	int &lockedTargetId = LockedTrackerTargetId(SelectedTrackerTargetCategory);
	if (clearTarget) {
		lockedTargetId = -1;
		SpeakText(_("Tracker target cleared."), true);
		return;
	}

	std::optional<int> targetId;
	std::optional<Point> targetPosition;
	std::optional<Point> alternateTargetPosition;
	StringOrView targetName;

	switch (SelectedTrackerTargetCategory) {
	case TrackerTargetCategory::Items: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyItemTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No items found."), true);
				else
					SpeakText(_("No next item."), true);
				return;
			}
		} else if (IsGroundItemPresent(lockedTargetId)) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestGroundItemId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No items found."), true);
			return;
		}

		if (!IsGroundItemPresent(*targetId)) {
			lockedTargetId = -1;
			SpeakText(_("No items found."), true);
			return;
		}

		lockedTargetId = *targetId;
		const Item &tracked = Items[*targetId];

		targetName = tracked.getName();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		targetPosition = tracked.position;
		break;
	}
	case TrackerTargetCategory::Chests: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyChestTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No chests found."), true);
				else
					SpeakText(_("No next chest."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestUnopenedChestObjectId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No chests found."), true);
			return;
		}

		const Object &object = Objects[*targetId];
		if (!IsTrackedChestObject(object)) {
			lockedTargetId = -1;
			targetId = FindNearestUnopenedChestObjectId(playerPosition);
			if (!targetId) {
				SpeakText(_("No chests found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Object &tracked = Objects[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position;
			if (FindObjectAtPosition(tracked.position + Direction::NorthEast) == &tracked)
				alternateTargetPosition = tracked.position + Direction::NorthEast;
		}
		break;
	}
	case TrackerTargetCategory::Doors: {
		std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyDoorTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		for (TrackerCandidate &c : nearbyCandidates) {
			if (c.id < 0 || c.id >= MAXOBJECTS)
				continue;
			c.name = DoorLabelForSpeech(Objects[c.id]);
		}
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No doors found."), true);
				else
					SpeakText(_("No next door."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestDoorObjectId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No doors found."), true);
			return;
		}

		const Object &object = Objects[*targetId];
		if (!IsTrackedDoorObject(object)) {
			lockedTargetId = -1;
			targetId = FindNearestDoorObjectId(playerPosition);
			if (!targetId) {
				SpeakText(_("No doors found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Object &tracked = Objects[*targetId];

		targetName = DoorLabelForSpeech(tracked);
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position;
			if (FindObjectAtPosition(tracked.position + Direction::NorthEast) == &tracked)
				alternateTargetPosition = tracked.position + Direction::NorthEast;
		}
		break;
	}
	case TrackerTargetCategory::Shrines: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyShrineTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No shrines found."), true);
				else
					SpeakText(_("No next shrine."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestShrineObjectId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No shrines found."), true);
			return;
		}

		const Object &object = Objects[*targetId];
		if (!IsShrineLikeObject(object)) {
			lockedTargetId = -1;
			targetId = FindNearestShrineObjectId(playerPosition);
			if (!targetId) {
				SpeakText(_("No shrines found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Object &tracked = Objects[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position;
			if (FindObjectAtPosition(tracked.position + Direction::NorthEast) == &tracked)
				alternateTargetPosition = tracked.position + Direction::NorthEast;
		}
		break;
	}
	case TrackerTargetCategory::Objects: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyObjectInteractableTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No objects found."), true);
				else
					SpeakText(_("No next object."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestMiscInteractableObjectId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No objects found."), true);
			return;
		}

		const Object &object = Objects[*targetId];
		if (!IsTrackedMiscInteractableObject(object)) {
			lockedTargetId = -1;
			targetId = FindNearestMiscInteractableObjectId(playerPosition);
			if (!targetId) {
				SpeakText(_("No objects found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Object &tracked = Objects[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position;
			if (FindObjectAtPosition(tracked.position + Direction::NorthEast) == &tracked)
				alternateTargetPosition = tracked.position + Direction::NorthEast;
		}
		break;
	}
	case TrackerTargetCategory::Breakables: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyBreakableTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No breakables found."), true);
				else
					SpeakText(_("No next breakable."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXOBJECTS) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestBreakableObjectId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No breakables found."), true);
			return;
		}

		const Object &object = Objects[*targetId];
		if (!IsTrackedBreakableObject(object)) {
			lockedTargetId = -1;
			targetId = FindNearestBreakableObjectId(playerPosition);
			if (!targetId) {
				SpeakText(_("No breakables found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Object &tracked = Objects[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position;
			if (FindObjectAtPosition(tracked.position + Direction::NorthEast) == &tracked)
				alternateTargetPosition = tracked.position + Direction::NorthEast;
		}
		break;
	}
	case TrackerTargetCategory::Monsters: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyMonsterTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No monsters found."), true);
				else
					SpeakText(_("No next monster."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < static_cast<int>(MaxMonsters)) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestMonsterId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No monsters found."), true);
			return;
		}

		const Monster &monster = Monsters[*targetId];
		if (!IsTrackedMonster(monster)) {
			lockedTargetId = -1;
			targetId = FindNearestMonsterId(playerPosition);
			if (!targetId) {
				SpeakText(_("No monsters found."), true);
				return;
			}
		}

		lockedTargetId = *targetId;
		const Monster &tracked = Monsters[*targetId];

		targetName = tracked.name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = tracked.position.tile;
		}
		break;
	}
	case TrackerTargetCategory::DeadBodies: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNearbyCorpseTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No dead bodies found."), true);
				else
					SpeakText(_("No next dead body."), true);
				return;
			}
		} else if (IsCorpsePresent(lockedTargetId)) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestCorpseId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No dead bodies found."), true);
			return;
		}

		if (!IsCorpsePresent(*targetId)) {
			lockedTargetId = -1;
			SpeakText(_("No dead bodies found."), true);
			return;
		}

		lockedTargetId = *targetId;
		targetName = _("Dead body");
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = CorpsePositionForTrackerId(*targetId);
		}
		break;
	}
	}

	if (cycleTarget) {
		SpeakText(targetName.str(), /*force=*/true);
		return;
	}

	if (!targetPosition) {
		SpeakText(_("Can't find a nearby tile to walk to."), true);
		return;
	}

	Point chosenTargetPosition = *targetPosition;
	enum class TrackerPathMode : uint8_t {
		RespectDoorsAndMonsters,
		IgnoreDoors,
		IgnoreMonsters,
		IgnoreDoorsAndMonsters,
		Lenient,
	};

	auto findPathToTarget = [&](Point destination, TrackerPathMode mode) -> std::optional<std::vector<int8_t>> {
		const bool allowDestinationNonWalkable = !PosOkPlayer(*MyPlayer, destination);
		switch (mode) {
		case TrackerPathMode::RespectDoorsAndMonsters:
			return FindKeyboardWalkPathForSpeechRespectingDoors(*MyPlayer, playerPosition, destination, allowDestinationNonWalkable);
		case TrackerPathMode::IgnoreDoors:
			return FindKeyboardWalkPathForSpeech(*MyPlayer, playerPosition, destination, allowDestinationNonWalkable);
		case TrackerPathMode::IgnoreMonsters:
			return FindKeyboardWalkPathForSpeechRespectingDoorsIgnoringMonsters(*MyPlayer, playerPosition, destination, allowDestinationNonWalkable);
		case TrackerPathMode::IgnoreDoorsAndMonsters:
			return FindKeyboardWalkPathForSpeechIgnoringMonsters(*MyPlayer, playerPosition, destination, allowDestinationNonWalkable);
		case TrackerPathMode::Lenient:
			return FindKeyboardWalkPathForSpeechLenient(*MyPlayer, playerPosition, destination, allowDestinationNonWalkable);
		default:
			return std::nullopt;
		}
	};

	std::optional<std::vector<int8_t>> spokenPath;
	bool pathIgnoresDoors = false;
	bool pathIgnoresMonsters = false;
	bool pathIgnoresBreakables = false;

	const auto considerDestination = [&](Point destination, TrackerPathMode mode) {
		const std::optional<std::vector<int8_t>> candidate = findPathToTarget(destination, mode);
		if (!candidate)
			return;
		if (!spokenPath || candidate->size() < spokenPath->size()) {
			spokenPath = *candidate;
			chosenTargetPosition = destination;

			pathIgnoresDoors = mode == TrackerPathMode::IgnoreDoors || mode == TrackerPathMode::IgnoreDoorsAndMonsters || mode == TrackerPathMode::Lenient;
			pathIgnoresMonsters = mode == TrackerPathMode::IgnoreMonsters || mode == TrackerPathMode::IgnoreDoorsAndMonsters || mode == TrackerPathMode::Lenient;
			pathIgnoresBreakables = mode == TrackerPathMode::Lenient;
		}
	};

	considerDestination(*targetPosition, TrackerPathMode::RespectDoorsAndMonsters);
	if (alternateTargetPosition)
		considerDestination(*alternateTargetPosition, TrackerPathMode::RespectDoorsAndMonsters);

	if (!spokenPath) {
		considerDestination(*targetPosition, TrackerPathMode::IgnoreDoors);
		if (alternateTargetPosition)
			considerDestination(*alternateTargetPosition, TrackerPathMode::IgnoreDoors);
	}

	if (!spokenPath) {
		considerDestination(*targetPosition, TrackerPathMode::IgnoreMonsters);
		if (alternateTargetPosition)
			considerDestination(*alternateTargetPosition, TrackerPathMode::IgnoreMonsters);
	}

	if (!spokenPath) {
		considerDestination(*targetPosition, TrackerPathMode::IgnoreDoorsAndMonsters);
		if (alternateTargetPosition)
			considerDestination(*alternateTargetPosition, TrackerPathMode::IgnoreDoorsAndMonsters);
	}

	if (!spokenPath) {
		considerDestination(*targetPosition, TrackerPathMode::Lenient);
		if (alternateTargetPosition)
			considerDestination(*alternateTargetPosition, TrackerPathMode::Lenient);
	}

	bool showUnreachableWarning = false;
	if (!spokenPath) {
		showUnreachableWarning = true;
		Point closestPosition;
		spokenPath = FindKeyboardWalkPathToClosestReachableForSpeech(*MyPlayer, playerPosition, chosenTargetPosition, closestPosition);
		pathIgnoresDoors = true;
		pathIgnoresMonsters = false;
		pathIgnoresBreakables = false;
	}

	if (spokenPath && !showUnreachableWarning && !PosOkPlayer(*MyPlayer, chosenTargetPosition)) {
		if (!spokenPath->empty())
			spokenPath->pop_back();
	}

	if (spokenPath && (pathIgnoresDoors || pathIgnoresMonsters || pathIgnoresBreakables)) {
		const std::optional<TrackerPathBlockInfo> block = FindFirstTrackerPathBlock(playerPosition, spokenPath->data(), spokenPath->size(), pathIgnoresDoors, pathIgnoresMonsters, pathIgnoresBreakables, chosenTargetPosition);
		if (block) {
			if (playerPosition.WalkingDistance(block->blockPosition) <= TrackerInteractDistanceTiles) {
				switch (block->type) {
				case TrackerPathBlockType::Door:
					SpeakText(_("A door is blocking the path. Open it and try again."), true);
					return;
				case TrackerPathBlockType::Monster:
					SpeakText(_("A monster is blocking the path. Clear it and try again."), true);
					return;
				case TrackerPathBlockType::Breakable:
					SpeakText(_("A breakable object is blocking the path. Destroy it and try again."), true);
					return;
				}
			}

			spokenPath = std::vector<int8_t>(spokenPath->begin(), spokenPath->begin() + block->stepIndex);
		}
	}

	std::string message;
	if (!targetName.empty())
		StrAppend(message, targetName, "\n");
	if (showUnreachableWarning) {
		message.append(_("Can't find a path to the target."));
		if (spokenPath && !spokenPath->empty())
			message.append("\n");
	}
	if (spokenPath) {
		if (!showUnreachableWarning || !spokenPath->empty())
			AppendKeyboardWalkPathForSpeech(message, *spokenPath);
	}

	SpeakText(message, true);
}

void AutoWalkToTrackerTargetKeyPressed()
{
	if (AutoWalkTrackerTargetId >= 0) {
		CancelAutoWalk();
		SpeakText(_("Walk cancelled."), true);
		return;
	}

	if (!CanPlayerTakeAction() || InGameMenu())
		return;

	if (leveltype == DTYPE_TOWN) {
		SpeakText(_("Not in a dungeon."), true);
		return;
	}
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}
	if (MyPlayer == nullptr) {
		AutoWalkTrackerTargetId = -1;
		SpeakText(_("Cannot walk right now."), true);
		return;
	}

	EnsureTrackerLocksMatchCurrentLevel();

	const Point playerPosition = MyPlayer->position.future;
	int &lockedTargetId = LockedTrackerTargetId(SelectedTrackerTargetCategory);

	std::optional<int> targetId;
	StringOrView targetName;

	switch (SelectedTrackerTargetCategory) {
	case TrackerTargetCategory::Items: {
		if (IsGroundItemPresent(lockedTargetId)) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestGroundItemId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No items found."), true);
			return;
		}
		if (!IsGroundItemPresent(*targetId)) {
			lockedTargetId = -1;
			SpeakText(_("No items found."), true);
			return;
		}
		lockedTargetId = *targetId;
		targetName = Items[*targetId].getName();
		break;
	}
	case TrackerTargetCategory::Chests:
		targetId = ResolveObjectTrackerTarget(lockedTargetId, playerPosition, IsTrackedChestObject, FindNearestUnopenedChestObjectId, [](int id) -> StringOrView { return Objects[id].name(); }, N_("No chests found."), targetName);
		if (!targetId)
			return;
		break;
	case TrackerTargetCategory::Doors:
		targetId = ResolveObjectTrackerTarget(lockedTargetId, playerPosition, IsTrackedDoorObject, FindNearestDoorObjectId, [](int id) -> StringOrView { return DoorLabelForSpeech(Objects[id]); }, N_("No doors found."), targetName);
		if (!targetId)
			return;
		break;
	case TrackerTargetCategory::Shrines:
		targetId = ResolveObjectTrackerTarget(lockedTargetId, playerPosition, IsShrineLikeObject, FindNearestShrineObjectId, [](int id) -> StringOrView { return Objects[id].name(); }, N_("No shrines found."), targetName);
		if (!targetId)
			return;
		break;
	case TrackerTargetCategory::Objects:
		targetId = ResolveObjectTrackerTarget(lockedTargetId, playerPosition, IsTrackedMiscInteractableObject, FindNearestMiscInteractableObjectId, [](int id) -> StringOrView { return Objects[id].name(); }, N_("No objects found."), targetName);
		if (!targetId)
			return;
		break;
	case TrackerTargetCategory::Breakables:
		targetId = ResolveObjectTrackerTarget(lockedTargetId, playerPosition, IsTrackedBreakableObject, FindNearestBreakableObjectId, [](int id) -> StringOrView { return Objects[id].name(); }, N_("No breakables found."), targetName);
		if (!targetId)
			return;
		break;
	case TrackerTargetCategory::Monsters: {
		if (lockedTargetId >= 0 && lockedTargetId < static_cast<int>(MaxMonsters)) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestMonsterId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No monsters found."), true);
			return;
		}
		const Monster &monster = Monsters[*targetId];
		if (!IsTrackedMonster(monster)) {
			lockedTargetId = -1;
			targetId = FindNearestMonsterId(playerPosition);
			if (!targetId) {
				SpeakText(_("No monsters found."), true);
				return;
			}
		}
		lockedTargetId = *targetId;
		targetName = Monsters[*targetId].name();
		break;
	}
	case TrackerTargetCategory::DeadBodies: {
		if (IsCorpsePresent(lockedTargetId)) {
			targetId = lockedTargetId;
		} else {
			targetId = FindNearestCorpseId(playerPosition);
		}
		if (!targetId) {
			SpeakText(_("No dead bodies found."), true);
			return;
		}
		if (!IsCorpsePresent(*targetId)) {
			lockedTargetId = -1;
			SpeakText(_("No dead bodies found."), true);
			return;
		}
		lockedTargetId = *targetId;
		targetName = _("Dead body");
		break;
	}
	}

	std::string msg;
	StrAppend(msg, _("Going to: "), targetName);
	SpeakText(msg, true);

	AutoWalkTrackerTargetId = *targetId;
	AutoWalkTrackerTargetCategory = SelectedTrackerTargetCategory;
	UpdateAutoWalkTracker();
}

void UpdateAutoWalkTracker()
{
	if (AutoWalkTrackerTargetId < 0)
		return;
	if (leveltype == DTYPE_TOWN || IsPlayerInStore() || ChatLogFlag || HelpFlag || InGameMenu()) {
		AutoWalkTrackerTargetId = -1;
		return;
	}
	if (!CanPlayerTakeAction())
		return;

	if (MyPlayer == nullptr) {
		AutoWalkTrackerTargetId = -1;
		SpeakText(_("Cannot walk right now."), true);
		return;
	}
	if (MyPlayer->_pmode != PM_STAND)
		return;
	if (MyPlayer->walkpath[0] != WALK_NONE)
		return;
	if (MyPlayer->destAction != ACTION_NONE)
		return;

	Player &myPlayer = *MyPlayer;
	const Point playerPosition = myPlayer.position.future;

	std::optional<Point> destination;

	switch (AutoWalkTrackerTargetCategory) {
	case TrackerTargetCategory::Items: {
		const int itemId = AutoWalkTrackerTargetId;
		if (itemId < 0 || itemId > MAXITEMS) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target item is gone."), true);
			return;
		}
		if (!IsGroundItemPresent(itemId)) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target item is gone."), true);
			return;
		}
		const Item &item = Items[itemId];
		if (playerPosition.WalkingDistance(item.position) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Item in range."), true);
			return;
		}
		destination = item.position;
		break;
	}
	case TrackerTargetCategory::Chests:
		if (!ValidateAutoWalkObjectTarget(myPlayer, playerPosition,
		        IsTrackedChestObject, N_("Target chest is gone."), N_("Chest in range."), destination))
			return;
		break;
	case TrackerTargetCategory::Doors:
		if (!ValidateAutoWalkObjectTarget(myPlayer, playerPosition, IsTrackedDoorObject, N_("Target door is gone."), N_("Door in range."), destination))
			return;
		break;
	case TrackerTargetCategory::Shrines:
		if (!ValidateAutoWalkObjectTarget(myPlayer, playerPosition, IsShrineLikeObject, N_("Target shrine is gone."), N_("Shrine in range."), destination))
			return;
		break;
	case TrackerTargetCategory::Objects:
		if (!ValidateAutoWalkObjectTarget(myPlayer, playerPosition, IsTrackedMiscInteractableObject, N_("Target object is gone."), N_("Object in range."), destination))
			return;
		break;
	case TrackerTargetCategory::Breakables:
		if (!ValidateAutoWalkObjectTarget(myPlayer, playerPosition, IsTrackedBreakableObject, N_("Target breakable is gone."), N_("Breakable in range."), destination))
			return;
		break;
	case TrackerTargetCategory::Monsters: {
		const int monsterId = AutoWalkTrackerTargetId;
		if (monsterId < 0 || monsterId >= static_cast<int>(MaxMonsters)) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target monster is gone."), true);
			return;
		}
		const Monster &monster = Monsters[monsterId];
		if (!IsTrackedMonster(monster)) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target monster is gone."), true);
			return;
		}
		const Point monsterPosition { monster.position.tile };
		if (playerPosition.WalkingDistance(monsterPosition) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Monster in range."), true);
			return;
		}
		destination = FindBestAdjacentApproachTile(myPlayer, playerPosition, monsterPosition);
		break;
	}
	case TrackerTargetCategory::DeadBodies: {
		const int corpseId = AutoWalkTrackerTargetId;
		if (!IsCorpsePresent(corpseId)) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target dead body is gone."), true);
			return;
		}

		const Point corpsePosition = CorpsePositionForTrackerId(corpseId);
		if (playerPosition.WalkingDistance(corpsePosition) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Dead body in range."), true);
			return;
		}

		destination = corpsePosition;
		break;
	}
	}

	if (!destination) {
		AutoWalkTrackerTargetId = -1;
		SpeakText(_("Can't find a nearby tile to walk to."), true);
		return;
	}

	constexpr size_t MaxAutoWalkPathLength = 512;
	std::array<int8_t, MaxAutoWalkPathLength> path;
	path.fill(WALK_NONE);

	int steps = FindPath(CanStep, [&myPlayer](Point position) { return PosOkPlayer(myPlayer, position); }, playerPosition, *destination, path.data(), path.size());
	if (steps == 0) {
		std::array<int8_t, MaxAutoWalkPathLength> ignoreDoorPath;
		ignoreDoorPath.fill(WALK_NONE);

		const int ignoreDoorSteps = FindPath(CanStep, [&myPlayer](Point position) { return PosOkPlayerIgnoreDoors(myPlayer, position); }, playerPosition, *destination, ignoreDoorPath.data(), ignoreDoorPath.size());
		if (ignoreDoorSteps != 0) {
			const std::optional<DoorBlockInfo> block = FindFirstClosedDoorOnWalkPath(playerPosition, ignoreDoorPath.data(), ignoreDoorSteps);
			if (block) {
				if (playerPosition.WalkingDistance(block->doorPosition) <= TrackerInteractDistanceTiles) {
					AutoWalkTrackerTargetId = -1;
					SpeakText(_("A door is blocking the path. Open it and try again."), true);
					return;
				}

				*destination = block->beforeDoor;
				path.fill(WALK_NONE);
				steps = FindPath(CanStep, [&myPlayer](Point position) { return PosOkPlayer(myPlayer, position); }, playerPosition, *destination, path.data(), path.size());
			}
		}

		if (steps == 0) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Can't find a path to the target."), true);
			return;
		}
	}

	if (steps < static_cast<int>(MaxPathLengthPlayer)) {
		NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, *destination);
		return;
	}

	const int segmentSteps = std::min(steps - 1, static_cast<int>(MaxPathLengthPlayer - 1));
	const Point waypoint = PositionAfterWalkPathSteps(playerPosition, path.data(), segmentSteps);
	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, waypoint);
}

void ResetAutoWalkTracker()
{
	AutoWalkTrackerTargetId = -1;
}

} // namespace devilution
