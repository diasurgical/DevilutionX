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
#include "levels/setmaps.h"
#include "levels/tile_properties.hpp"
#include "levels/trigs.h"
#include "missiles.h"
#include "monster.h"
#include "multi.h"
#include "objects.h"
#include "player.h"
#include "portal.h"
#include "qol/chatlog.h"
#include "quests.h"
#include "stores.h"
#include "towners.h"
#include "utils/accessibility_announcements.hpp"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/navigation_speech.hpp"
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
// Selection list range for PageUp/PageDown. Use a value larger than the maximum
// possible distance across the 112x112 dungeon grid so the list includes all
// eligible targets on the current level.
constexpr int TrackerCycleDistanceTiles = MAXDUNX + MAXDUNY;

int LockedTrackerItemId = -1;
int LockedTrackerChestId = -1;
int LockedTrackerDoorId = -1;
int LockedTrackerShrineId = -1;
int LockedTrackerObjectId = -1;
int LockedTrackerBreakableId = -1;
int LockedTrackerMonsterId = -1;
int LockedTrackerDeadBodyId = -1;
int LockedTrackerNpcId = -1;
int LockedTrackerPlayerId = -1;
int LockedTrackerDungeonEntranceId = -1;
int LockedTrackerStairsId = -1;
int LockedTrackerQuestLocationId = -1;
int LockedTrackerPortalId = -1;

struct TrackerLevelKey {
	dungeon_type levelType;
	int currLevel;
	bool isSetLevel;
	int setLevelNum;

	friend bool operator==(const TrackerLevelKey &lhs, const TrackerLevelKey &rhs)
	{
		return lhs.levelType == rhs.levelType && lhs.currLevel == rhs.currLevel
		    && lhs.isSetLevel == rhs.isSetLevel && lhs.setLevelNum == rhs.setLevelNum;
	}
	friend bool operator!=(const TrackerLevelKey &lhs, const TrackerLevelKey &rhs)
	{
		return !(lhs == rhs);
	}
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
	LockedTrackerNpcId = -1;
	LockedTrackerPlayerId = -1;
	LockedTrackerDungeonEntranceId = -1;
	LockedTrackerStairsId = -1;
	LockedTrackerQuestLocationId = -1;
	LockedTrackerPortalId = -1;
}

void EnsureTrackerLocksMatchCurrentLevel()
{
	const TrackerLevelKey current {
		.levelType = leveltype,
		.currLevel = currlevel,
		.isSetLevel = setlevel,
		.setLevelNum = setlvlnum,
	};

	if (!LockedTrackerLevelKey || *LockedTrackerLevelKey != current) {
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
	case TrackerTargetCategory::Npcs:
		return LockedTrackerNpcId;
	case TrackerTargetCategory::Players:
		return LockedTrackerPlayerId;
	case TrackerTargetCategory::DungeonEntrances:
		return LockedTrackerDungeonEntranceId;
	case TrackerTargetCategory::Stairs:
		return LockedTrackerStairsId;
	case TrackerTargetCategory::QuestLocations:
		return LockedTrackerQuestLocationId;
	case TrackerTargetCategory::Portals:
		return LockedTrackerPortalId;
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
	case TrackerTargetCategory::Npcs:
		return _("NPCs");
	case TrackerTargetCategory::Players:
		return _("players");
	case TrackerTargetCategory::DungeonEntrances:
		if (leveltype != DTYPE_TOWN)
			return _("exits");
		return _("dungeon entrances");
	case TrackerTargetCategory::Stairs:
		return _("stairs");
	case TrackerTargetCategory::QuestLocations:
		return _("quest locations");
	case TrackerTargetCategory::Portals:
		return _("portals");
	}
	app_fatal("Invalid TrackerTargetCategory");
}

void SpeakTrackerTargetCategory()
{
	SpeakText(TrackerTargetCategoryLabel(SelectedTrackerTargetCategory), true);
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

	std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
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

	std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
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

	std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
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
		if (!IsTrackedMonster(monster))
			continue;

		const int distance = playerPosition.ApproxDistance(monster.position.future);
		if (distance > maxDistance)
			continue;

		result.push_back(TrackerCandidate {
		    .id = monsterId,
		    .distance = distance,
		    .name = monster.name(),
		});
	}

	std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
	return result;
}

[[nodiscard]] std::vector<TrackerCandidate> CollectNpcTrackerCandidates(Point playerPosition)
{
	std::vector<TrackerCandidate> result;
	if (leveltype != DTYPE_TOWN)
		return result;

	result.reserve(GetNumTowners());
	for (size_t i = 0; i < GetNumTowners(); ++i) {
		const Towner &towner = Towners[i];
		if (!IsTownerPresent(towner._ttype))
			continue;

		const int distance = playerPosition.WalkingDistance(towner.position);
		result.push_back(TrackerCandidate {
		    .id = static_cast<int>(i),
		    .distance = distance,
		    .name = towner.name,
		});
	}

	std::sort(result.begin(), result.end(), [](const TrackerCandidate &a, const TrackerCandidate &b) {
		if (a.distance != b.distance)
			return a.distance < b.distance;
		return a.name.str() < b.name.str();
	});
	return result;
}

[[nodiscard]] std::vector<TrackerCandidate> CollectPlayerTrackerCandidates(Point playerPosition)
{
	std::vector<TrackerCandidate> result;
	if (!gbIsMultiplayer || MyPlayer == nullptr)
		return result;

	result.reserve(MAX_PLRS);

	const uint8_t currentLevel = MyPlayer->plrlevel;
	const bool currentIsSetLevel = setlevel;

	for (int i = 0; i < MAX_PLRS; ++i) {
		if (i == MyPlayerId)
			continue;
		const Player &player = Players[i];
		if (!player.plractive)
			continue;
		if (player._pLvlChanging)
			continue;
		if (player.plrlevel != currentLevel)
			continue;
		if (player.plrIsOnSetLevel != currentIsSetLevel)
			continue;

		const Point otherPosition = player.position.future;
		if (!InDungeonBounds(otherPosition))
			continue;

		const int distance = playerPosition.WalkingDistance(otherPosition);
		result.push_back(TrackerCandidate {
		    .id = i,
		    .distance = distance,
		    .name = player.name(),
		});
	}

	std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
	return result;
}

[[nodiscard]] std::vector<TrackerCandidate> CollectDungeonEntranceTrackerCandidates(Point playerPosition)
{
	std::vector<TrackerCandidate> result;
	if (MyPlayer == nullptr)
		return result;

	if (leveltype == DTYPE_TOWN) {
		const std::vector<int> candidates = CollectTownDungeonTriggerIndices();
		result.reserve(candidates.size());

		for (const int triggerIndex : candidates) {
			if (triggerIndex < 0 || triggerIndex >= numtrigs)
				continue;
			const TriggerStruct &trigger = trigs[triggerIndex];
			const Point triggerPosition { trigger.position.x, trigger.position.y };
			const int distance = playerPosition.WalkingDistance(triggerPosition);
			result.push_back(TrackerCandidate {
			    .id = triggerIndex,
			    .distance = distance,
			    .name = TriggerLabelForSpeech(trigger),
			});
		}

		std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
		return result;
	}

	for (int i = 0; i < numtrigs; ++i) {
		const TriggerStruct &trigger = trigs[i];
		if (setlevel) {
			if (trigger._tmsg != WM_DIABRTNLVL)
				continue;
		} else {
			if (!IsAnyOf(trigger._tmsg, WM_DIABPREVLVL, WM_DIABTWARPUP))
				continue;
		}

		const Point triggerPosition { trigger.position.x, trigger.position.y };
		const int distance = playerPosition.WalkingDistance(triggerPosition);
		result.push_back(TrackerCandidate {
		    .id = i,
		    .distance = distance,
		    .name = TriggerLabelForSpeech(trigger),
		});
	}

	std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
	return result;
}

[[nodiscard]] std::optional<Point> FindTownPortalPositionInTownByPortalIndex(int portalIndex)
{
	if (portalIndex < 0 || portalIndex >= MAXPORTAL)
		return std::nullopt;

	for (const Missile &missile : Missiles) {
		if (missile._mitype != MissileID::TownPortal)
			continue;
		if (missile._misource != portalIndex)
			continue;
		return missile.position.tile;
	}

	return std::nullopt;
}

[[nodiscard]] bool IsTownPortalOpenOnCurrentLevel(int portalIndex)
{
	if (portalIndex < 0 || portalIndex >= MAXPORTAL)
		return false;
	const Portal &portal = Portals[portalIndex];
	if (!portal.open)
		return false;
	if (portal.setlvl != setlevel)
		return false;
	if (portal.level != currlevel)
		return false;
	if (portal.ltype != leveltype)
		return false;
	return InDungeonBounds(portal.position);
}

[[nodiscard]] std::vector<TrackerCandidate> CollectPortalTrackerCandidates(Point playerPosition)
{
	std::vector<TrackerCandidate> result;
	if (MyPlayer == nullptr)
		return result;

	if (leveltype == DTYPE_TOWN) {
		std::array<bool, MAXPORTAL> seen {};
		for (const Missile &missile : Missiles) {
			if (missile._mitype != MissileID::TownPortal)
				continue;
			const int portalIndex = missile._misource;
			if (portalIndex < 0 || portalIndex >= MAXPORTAL)
				continue;
			if (seen[portalIndex])
				continue;
			seen[portalIndex] = true;

			const Point portalPosition = missile.position.tile;
			const int distance = playerPosition.WalkingDistance(portalPosition);
			result.push_back(TrackerCandidate {
			    .id = portalIndex,
			    .distance = distance,
			    .name = TownPortalLabelForSpeech(Portals[portalIndex]),
			});
		}
		std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
		return result;
	}

	for (int i = 0; i < MAXPORTAL; ++i) {
		if (!IsTownPortalOpenOnCurrentLevel(i))
			continue;
		const Portal &portal = Portals[i];
		const int distance = playerPosition.WalkingDistance(portal.position);
		result.push_back(TrackerCandidate {
		    .id = i,
		    .distance = distance,
		    .name = TownPortalLabelForSpeech(portal),
		});
	}
	std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
	return result;
}

[[nodiscard]] std::vector<TrackerCandidate> CollectStairsTrackerCandidates(Point playerPosition)
{
	std::vector<TrackerCandidate> result;
	if (MyPlayer == nullptr || leveltype == DTYPE_TOWN)
		return result;

	for (int i = 0; i < numtrigs; ++i) {
		const TriggerStruct &trigger = trigs[i];
		if (!IsAnyOf(trigger._tmsg, WM_DIABNEXTLVL, WM_DIABPREVLVL, WM_DIABTWARPUP))
			continue;

		const Point triggerPosition { trigger.position.x, trigger.position.y };
		const int distance = playerPosition.WalkingDistance(triggerPosition);
		result.push_back(TrackerCandidate {
		    .id = i,
		    .distance = distance,
		    .name = TriggerLabelForSpeech(trigger),
		});
	}

	std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
	return result;
}

[[nodiscard]] std::vector<TrackerCandidate> CollectQuestLocationTrackerCandidates(Point playerPosition)
{
	std::vector<TrackerCandidate> result;
	if (MyPlayer == nullptr || leveltype == DTYPE_TOWN)
		return result;

	if (setlevel) {
		for (int i = 0; i < numtrigs; ++i) {
			const TriggerStruct &trigger = trigs[i];
			if (trigger._tmsg != WM_DIABRTNLVL)
				continue;

			const Point triggerPosition { trigger.position.x, trigger.position.y };
			const int distance = playerPosition.WalkingDistance(triggerPosition);
			result.push_back(TrackerCandidate {
			    .id = i,
			    .distance = distance,
			    .name = TriggerLabelForSpeech(trigger),
			});
		}

		std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
		return result;
	}

	constexpr size_t NumQuests = sizeof(Quests) / sizeof(Quests[0]);
	result.reserve(NumQuests);
	for (size_t questIndex = 0; questIndex < NumQuests; ++questIndex) {
		const Quest &quest = Quests[questIndex];
		if (quest._qslvl == SL_NONE)
			continue;
		if (quest._qactive == QUEST_NOTAVAIL)
			continue;
		if (quest._qlevel != currlevel)
			continue;
		if (!InDungeonBounds(quest.position))
			continue;

		const char *questLevelName = QuestLevelNames[quest._qslvl];
		if (questLevelName == nullptr || questLevelName[0] == '\0')
			questLevelName = N_("Set level");

		const int distance = playerPosition.WalkingDistance(quest.position);
		result.push_back(TrackerCandidate {
		    .id = static_cast<int>(questIndex),
		    .distance = distance,
		    .name = _(questLevelName),
		});
	}

	std::sort(result.begin(), result.end(), IsBetterTrackerCandidate);
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

[[nodiscard]] std::optional<int> FindPreviousTrackerCandidateId(const std::vector<TrackerCandidate> &candidates, int currentId)
{
	if (candidates.empty())
		return std::nullopt;
	if (currentId < 0)
		return candidates.back().id;

	const auto it = std::find_if(candidates.begin(), candidates.end(), [currentId](const TrackerCandidate &c) { return c.id == currentId; });
	if (it == candidates.end())
		return candidates.back().id;

	if (candidates.size() <= 1)
		return std::nullopt;

	const size_t idx = static_cast<size_t>(it - candidates.begin());
	const size_t prevIdx = (idx + candidates.size() - 1) % candidates.size();
	return candidates[prevIdx].id;
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
		if (!IsTrackedMonster(monster))
			continue;

		const int distance = playerPosition.ApproxDistance(monster.position.future);
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
	destination = FindBestApproachTileForObject(myPlayer, playerPosition, object);
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

[[nodiscard]] std::vector<TrackerTargetCategory> TrackerTargetCategoriesForCurrentLevel()
{
	if (leveltype == DTYPE_TOWN) {
		return {
			TrackerTargetCategory::Items,
			TrackerTargetCategory::DeadBodies,
			TrackerTargetCategory::Npcs,
			TrackerTargetCategory::Players,
			TrackerTargetCategory::DungeonEntrances,
			TrackerTargetCategory::Portals,
		};
	}

	return {
		TrackerTargetCategory::Items,
		TrackerTargetCategory::Chests,
		TrackerTargetCategory::Doors,
		TrackerTargetCategory::Shrines,
		TrackerTargetCategory::Objects,
		TrackerTargetCategory::Breakables,
		TrackerTargetCategory::Monsters,
		TrackerTargetCategory::DeadBodies,
		TrackerTargetCategory::DungeonEntrances,
		TrackerTargetCategory::Stairs,
		TrackerTargetCategory::QuestLocations,
		TrackerTargetCategory::Players,
		TrackerTargetCategory::Portals,
	};
}

void SelectTrackerTargetCategoryRelative(int delta)
{
	if (!CanPlayerTakeAction() || InGameMenu())
		return;

	AutoWalkTrackerTargetId = -1;

	const std::vector<TrackerTargetCategory> categories = TrackerTargetCategoriesForCurrentLevel();
	if (categories.empty())
		return;

	auto it = std::find(categories.begin(), categories.end(), SelectedTrackerTargetCategory);
	int currentIndex = 0;
	if (it == categories.end()) {
		currentIndex = delta > 0 ? -1 : 0;
	} else {
		currentIndex = static_cast<int>(it - categories.begin());
	}

	const int count = static_cast<int>(categories.size());
	int newIndex = (currentIndex + delta) % count;
	if (newIndex < 0)
		newIndex += count;

	SelectedTrackerTargetCategory = categories[static_cast<size_t>(newIndex)];
	SpeakTrackerTargetCategory();
}

[[nodiscard]] std::vector<TrackerCandidate> CollectTrackerCandidatesForSelection(TrackerTargetCategory category, Point playerPosition)
{
	switch (category) {
	case TrackerTargetCategory::Items:
		return CollectNearbyItemTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
	case TrackerTargetCategory::Chests:
		return CollectNearbyChestTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
	case TrackerTargetCategory::Doors: {
		std::vector<TrackerCandidate> candidates = CollectNearbyDoorTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
		for (TrackerCandidate &c : candidates) {
			if (c.id < 0 || c.id >= MAXOBJECTS)
				continue;
			c.name = DoorLabelForSpeech(Objects[c.id]);
		}
		return candidates;
	}
	case TrackerTargetCategory::Shrines:
		return CollectNearbyShrineTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
	case TrackerTargetCategory::Objects:
		return CollectNearbyObjectInteractableTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
	case TrackerTargetCategory::Breakables:
		return CollectNearbyBreakableTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
	case TrackerTargetCategory::Monsters:
		return CollectNearbyMonsterTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
	case TrackerTargetCategory::DeadBodies:
		return CollectNearbyCorpseTrackerCandidates(playerPosition, TrackerCycleDistanceTiles);
	case TrackerTargetCategory::Npcs:
		return CollectNpcTrackerCandidates(playerPosition);
	case TrackerTargetCategory::Players:
		return CollectPlayerTrackerCandidates(playerPosition);
	case TrackerTargetCategory::DungeonEntrances:
		return CollectDungeonEntranceTrackerCandidates(playerPosition);
	case TrackerTargetCategory::Stairs:
		return CollectStairsTrackerCandidates(playerPosition);
	case TrackerTargetCategory::QuestLocations:
		return CollectQuestLocationTrackerCandidates(playerPosition);
	case TrackerTargetCategory::Portals:
		return CollectPortalTrackerCandidates(playerPosition);
	}
	app_fatal("Invalid TrackerTargetCategory");
}

[[nodiscard]] std::string_view TrackerCategoryNoCandidatesFoundMessage(TrackerTargetCategory category)
{
	switch (category) {
	case TrackerTargetCategory::Items:
		return _("No items found.");
	case TrackerTargetCategory::Chests:
		return _("No chests found.");
	case TrackerTargetCategory::Doors:
		return _("No doors found.");
	case TrackerTargetCategory::Shrines:
		return _("No shrines found.");
	case TrackerTargetCategory::Objects:
		return _("No objects found.");
	case TrackerTargetCategory::Breakables:
		return _("No breakables found.");
	case TrackerTargetCategory::Monsters:
		return _("No monsters found.");
	case TrackerTargetCategory::DeadBodies:
		return _("No dead bodies found.");
	case TrackerTargetCategory::Npcs:
		return _("No NPCs found.");
	case TrackerTargetCategory::Players:
		return _("No players found.");
	case TrackerTargetCategory::DungeonEntrances:
		if (leveltype != DTYPE_TOWN)
			return _("No exits found.");
		return _("No dungeon entrances found.");
	case TrackerTargetCategory::Stairs:
		return _("No stairs found.");
	case TrackerTargetCategory::QuestLocations:
		return _("No quest locations found.");
	case TrackerTargetCategory::Portals:
		return _("No portals found.");
	}
	app_fatal("Invalid TrackerTargetCategory");
}

[[nodiscard]] constexpr bool TrackerCategorySelectionIsProximityLimited(TrackerTargetCategory category)
{
	return IsAnyOf(category, TrackerTargetCategory::Items, TrackerTargetCategory::Chests, TrackerTargetCategory::Doors, TrackerTargetCategory::Shrines, TrackerTargetCategory::Objects,
	    TrackerTargetCategory::Breakables, TrackerTargetCategory::Monsters, TrackerTargetCategory::DeadBodies);
}

[[nodiscard]] bool TrackerCategoryHasAnyTargets(TrackerTargetCategory category, Point playerPosition)
{
	switch (category) {
	case TrackerTargetCategory::Items:
		return FindNearestGroundItemId(playerPosition).has_value();
	case TrackerTargetCategory::Chests:
		return FindNearestUnopenedChestObjectId(playerPosition).has_value();
	case TrackerTargetCategory::Doors:
		return FindNearestDoorObjectId(playerPosition).has_value();
	case TrackerTargetCategory::Shrines:
		return FindNearestShrineObjectId(playerPosition).has_value();
	case TrackerTargetCategory::Objects:
		return FindNearestMiscInteractableObjectId(playerPosition).has_value();
	case TrackerTargetCategory::Breakables:
		return FindNearestBreakableObjectId(playerPosition).has_value();
	case TrackerTargetCategory::Monsters:
		return FindNearestMonsterId(playerPosition).has_value();
	case TrackerTargetCategory::DeadBodies:
		return FindNearestCorpseId(playerPosition).has_value();
	default:
		return false;
	}
}

[[nodiscard]] std::string_view TrackerCategoryNoNearbyCandidatesFoundMessage(TrackerTargetCategory category)
{
	switch (category) {
	case TrackerTargetCategory::Items:
		return _("No nearby items found.");
	case TrackerTargetCategory::Chests:
		return _("No nearby chests found.");
	case TrackerTargetCategory::Doors:
		return _("No nearby doors found.");
	case TrackerTargetCategory::Shrines:
		return _("No nearby shrines found.");
	case TrackerTargetCategory::Objects:
		return _("No nearby objects found.");
	case TrackerTargetCategory::Breakables:
		return _("No nearby breakables found.");
	case TrackerTargetCategory::Monsters:
		return _("No nearby monsters found.");
	case TrackerTargetCategory::DeadBodies:
		return _("No nearby dead bodies found.");
	default:
		return TrackerCategoryNoCandidatesFoundMessage(category);
	}
}

[[nodiscard]] std::string_view TrackerCategoryNoNextMessage(TrackerTargetCategory category)
{
	switch (category) {
	case TrackerTargetCategory::Items:
		return _("No next item.");
	case TrackerTargetCategory::Chests:
		return _("No next chest.");
	case TrackerTargetCategory::Doors:
		return _("No next door.");
	case TrackerTargetCategory::Shrines:
		return _("No next shrine.");
	case TrackerTargetCategory::Objects:
		return _("No next object.");
	case TrackerTargetCategory::Breakables:
		return _("No next breakable.");
	case TrackerTargetCategory::Monsters:
		return _("No next monster.");
	case TrackerTargetCategory::DeadBodies:
		return _("No next dead body.");
	case TrackerTargetCategory::Npcs:
		return _("No next NPC.");
	case TrackerTargetCategory::Players:
		return _("No next player.");
	case TrackerTargetCategory::DungeonEntrances:
		return _("No next dungeon entrance.");
	case TrackerTargetCategory::Stairs:
		return _("No next stairs.");
	case TrackerTargetCategory::QuestLocations:
		return _("No next quest location.");
	case TrackerTargetCategory::Portals:
		return _("No next portal.");
	}
	app_fatal("Invalid TrackerTargetCategory");
}

[[nodiscard]] std::string_view TrackerCategoryNoPreviousMessage(TrackerTargetCategory category)
{
	switch (category) {
	case TrackerTargetCategory::Items:
		return _("No previous item.");
	case TrackerTargetCategory::Chests:
		return _("No previous chest.");
	case TrackerTargetCategory::Doors:
		return _("No previous door.");
	case TrackerTargetCategory::Shrines:
		return _("No previous shrine.");
	case TrackerTargetCategory::Objects:
		return _("No previous object.");
	case TrackerTargetCategory::Breakables:
		return _("No previous breakable.");
	case TrackerTargetCategory::Monsters:
		return _("No previous monster.");
	case TrackerTargetCategory::DeadBodies:
		return _("No previous dead body.");
	case TrackerTargetCategory::Npcs:
		return _("No previous NPC.");
	case TrackerTargetCategory::Players:
		return _("No previous player.");
	case TrackerTargetCategory::DungeonEntrances:
		return _("No previous dungeon entrance.");
	case TrackerTargetCategory::Stairs:
		return _("No previous stairs.");
	case TrackerTargetCategory::QuestLocations:
		return _("No previous quest location.");
	case TrackerTargetCategory::Portals:
		return _("No previous portal.");
	}
	app_fatal("Invalid TrackerTargetCategory");
}

/**
 * Returns true if the given tracker category requires a dungeon (i.e. is not
 * available in town).
 */
[[nodiscard]] bool IsDungeonOnlyTrackerCategory(TrackerTargetCategory category)
{
	return IsNoneOf(category, TrackerTargetCategory::Items, TrackerTargetCategory::DeadBodies,
	    TrackerTargetCategory::Npcs, TrackerTargetCategory::Players,
	    TrackerTargetCategory::DungeonEntrances, TrackerTargetCategory::Portals);
}

void SelectTrackerTargetRelative(int delta)
{
	if (!CanPlayerTakeAction() || InGameMenu())
		return;
	if (MyPlayer == nullptr)
		return;

	if (leveltype == DTYPE_TOWN && IsDungeonOnlyTrackerCategory(SelectedTrackerTargetCategory)) {
		SpeakText(_("Not in a dungeon."), true);
		return;
	}
	if (AutomapActive) {
		SpeakText(_("Close the map first."), true);
		return;
	}

	EnsureTrackerLocksMatchCurrentLevel();

	const Point playerPosition = MyPlayer->position.future;
	AutoWalkTrackerTargetId = -1;

	const std::vector<TrackerCandidate> candidates = CollectTrackerCandidatesForSelection(SelectedTrackerTargetCategory, playerPosition);
	if (candidates.empty()) {
		LockedTrackerTargetId(SelectedTrackerTargetCategory) = -1;
		if (TrackerCategorySelectionIsProximityLimited(SelectedTrackerTargetCategory) && TrackerCategoryHasAnyTargets(SelectedTrackerTargetCategory, playerPosition))
			SpeakText(TrackerCategoryNoNearbyCandidatesFoundMessage(SelectedTrackerTargetCategory), true);
		else
			SpeakText(TrackerCategoryNoCandidatesFoundMessage(SelectedTrackerTargetCategory), true);
		return;
	}

	int &lockedTargetId = LockedTrackerTargetId(SelectedTrackerTargetCategory);
	if (candidates.size() == 1) {
		lockedTargetId = candidates.front().id;
		SpeakText(candidates.front().name.str(), /*force=*/true);
		return;
	}
	const std::optional<int> targetId = delta > 0 ? FindNextTrackerCandidateId(candidates, lockedTargetId) : FindPreviousTrackerCandidateId(candidates, lockedTargetId);
	if (!targetId) {
		SpeakText(delta > 0 ? TrackerCategoryNoNextMessage(SelectedTrackerTargetCategory) : TrackerCategoryNoPreviousMessage(SelectedTrackerTargetCategory), true);
		return;
	}

	const auto it = std::find_if(candidates.begin(), candidates.end(), [id = *targetId](const TrackerCandidate &c) { return c.id == id; });
	if (it == candidates.end()) {
		lockedTargetId = -1;
		SpeakText(TrackerCategoryNoCandidatesFoundMessage(SelectedTrackerTargetCategory), true);
		return;
	}

	lockedTargetId = *targetId;
	StringOrView targetName = it->name.str();
	DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, candidates);
	SpeakText(targetName.str(), /*force=*/true);
}

} // namespace

namespace {

void NavigateToTrackerTargetKeyPressed()
{
	if (!CanPlayerTakeAction() || InGameMenu())
		return;
	if (leveltype == DTYPE_TOWN && IsDungeonOnlyTrackerCategory(SelectedTrackerTargetCategory)) {
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
	case TrackerTargetCategory::Npcs: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectNpcTrackerCandidates(playerPosition);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No NPCs found."), true);
				else
					SpeakText(_("No next NPC."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < static_cast<int>(GetNumTowners())) {
			targetId = lockedTargetId;
		} else if (!nearbyCandidates.empty()) {
			targetId = nearbyCandidates.front().id;
		}
		if (!targetId) {
			SpeakText(_("No NPCs found."), true);
			return;
		}

		const auto it = std::find_if(nearbyCandidates.begin(), nearbyCandidates.end(), [id = *targetId](const TrackerCandidate &c) { return c.id == id; });
		if (it == nearbyCandidates.end()) {
			lockedTargetId = -1;
			SpeakText(_("No NPCs found."), true);
			return;
		}

		lockedTargetId = *targetId;
		targetName = Towners[*targetId].name;
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = Towners[*targetId].position;
		}
		break;
	}
	case TrackerTargetCategory::Players: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectPlayerTrackerCandidates(playerPosition);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No players found."), true);
				else
					SpeakText(_("No next player."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAX_PLRS) {
			targetId = lockedTargetId;
		} else if (!nearbyCandidates.empty()) {
			targetId = nearbyCandidates.front().id;
		}
		if (!targetId) {
			SpeakText(_("No players found."), true);
			return;
		}

		const auto it = std::find_if(nearbyCandidates.begin(), nearbyCandidates.end(), [id = *targetId](const TrackerCandidate &c) { return c.id == id; });
		if (it == nearbyCandidates.end()) {
			lockedTargetId = -1;
			SpeakText(_("No players found."), true);
			return;
		}

		lockedTargetId = *targetId;
		targetName = Players[*targetId].name();
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = Players[*targetId].position.future;
		}
		break;
	}
	case TrackerTargetCategory::DungeonEntrances: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectDungeonEntranceTrackerCandidates(playerPosition);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No dungeon entrances found."), true);
				else
					SpeakText(_("No next dungeon entrance."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < numtrigs) {
			targetId = lockedTargetId;
		} else if (!nearbyCandidates.empty()) {
			targetId = nearbyCandidates.front().id;
		}
		if (!targetId) {
			SpeakText(_("No dungeon entrances found."), true);
			return;
		}

		const auto it = std::find_if(nearbyCandidates.begin(), nearbyCandidates.end(), [id = *targetId](const TrackerCandidate &c) { return c.id == id; });
		if (it == nearbyCandidates.end()) {
			lockedTargetId = -1;
			SpeakText(_("No dungeon entrances found."), true);
			return;
		}

		lockedTargetId = *targetId;
		targetName = TriggerLabelForSpeech(trigs[*targetId]);
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			const TriggerStruct &trigger = trigs[*targetId];
			targetPosition = Point { trigger.position.x, trigger.position.y };
		}
		break;
	}
	case TrackerTargetCategory::Stairs: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectStairsTrackerCandidates(playerPosition);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No stairs found."), true);
				else
					SpeakText(_("No next stairs."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < numtrigs) {
			targetId = lockedTargetId;
		} else if (!nearbyCandidates.empty()) {
			targetId = nearbyCandidates.front().id;
		}
		if (!targetId) {
			SpeakText(_("No stairs found."), true);
			return;
		}

		const auto it = std::find_if(nearbyCandidates.begin(), nearbyCandidates.end(), [id = *targetId](const TrackerCandidate &c) { return c.id == id; });
		if (it == nearbyCandidates.end()) {
			lockedTargetId = -1;
			SpeakText(_("No stairs found."), true);
			return;
		}

		lockedTargetId = *targetId;
		targetName = std::string(it->name.str());
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			const TriggerStruct &trigger = trigs[*targetId];
			targetPosition = Point { trigger.position.x, trigger.position.y };
		}
		break;
	}
	case TrackerTargetCategory::QuestLocations: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectQuestLocationTrackerCandidates(playerPosition);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No quest locations found."), true);
				else
					SpeakText(_("No next quest location."), true);
				return;
			}
		} else if ((setlevel && lockedTargetId >= 0 && lockedTargetId < numtrigs) || (!setlevel && lockedTargetId >= 0 && lockedTargetId < static_cast<int>(sizeof(Quests) / sizeof(Quests[0])))) {
			targetId = lockedTargetId;
		} else if (!nearbyCandidates.empty()) {
			targetId = nearbyCandidates.front().id;
		}
		if (!targetId) {
			SpeakText(_("No quest locations found."), true);
			return;
		}

		const auto it = std::find_if(nearbyCandidates.begin(), nearbyCandidates.end(), [id = *targetId](const TrackerCandidate &c) { return c.id == id; });
		if (it == nearbyCandidates.end()) {
			lockedTargetId = -1;
			SpeakText(_("No quest locations found."), true);
			return;
		}

		lockedTargetId = *targetId;
		targetName = std::string(it->name.str());
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			if (setlevel) {
				const TriggerStruct &trigger = trigs[*targetId];
				targetPosition = Point { trigger.position.x, trigger.position.y };
			} else {
				const Quest &quest = Quests[static_cast<size_t>(*targetId)];
				targetPosition = quest.position;
			}
		}
		break;
	}
	case TrackerTargetCategory::Portals: {
		const std::vector<TrackerCandidate> nearbyCandidates = CollectPortalTrackerCandidates(playerPosition);
		if (cycleTarget) {
			targetId = FindNextTrackerCandidateId(nearbyCandidates, lockedTargetId);
			if (!targetId) {
				if (nearbyCandidates.empty())
					SpeakText(_("No portals found."), true);
				else
					SpeakText(_("No next portal."), true);
				return;
			}
		} else if (lockedTargetId >= 0 && lockedTargetId < MAXPORTAL) {
			targetId = lockedTargetId;
		} else if (!nearbyCandidates.empty()) {
			targetId = nearbyCandidates.front().id;
		}
		if (!targetId) {
			SpeakText(_("No portals found."), true);
			return;
		}

		const auto it = std::find_if(nearbyCandidates.begin(), nearbyCandidates.end(), [id = *targetId](const TrackerCandidate &c) { return c.id == id; });
		if (it == nearbyCandidates.end()) {
			lockedTargetId = -1;
			SpeakText(_("No portals found."), true);
			return;
		}

		Point portalPosition;
		if (leveltype == DTYPE_TOWN) {
			const std::optional<Point> townPos = FindTownPortalPositionInTownByPortalIndex(*targetId);
			if (!townPos) {
				lockedTargetId = -1;
				SpeakText(_("No portals found."), true);
				return;
			}
			portalPosition = *townPos;
		} else {
			if (!IsTownPortalOpenOnCurrentLevel(*targetId)) {
				lockedTargetId = -1;
				SpeakText(_("No portals found."), true);
				return;
			}
			portalPosition = Portals[*targetId].position;
		}

		lockedTargetId = *targetId;
		targetName = TownPortalLabelForSpeech(Portals[*targetId]);
		DecorateTrackerTargetNameWithOrdinalIfNeeded(*targetId, targetName, nearbyCandidates);
		if (!cycleTarget) {
			targetPosition = portalPosition;
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

	if (leveltype == DTYPE_TOWN && IsDungeonOnlyTrackerCategory(SelectedTrackerTargetCategory)) {
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
	case TrackerTargetCategory::Npcs: {
		const std::vector<TrackerCandidate> candidates = CollectNpcTrackerCandidates(playerPosition);
		if (candidates.empty()) {
			SpeakText(_("No NPCs found."), true);
			return;
		}

		if (lockedTargetId >= 0 && lockedTargetId < static_cast<int>(GetNumTowners())) {
			const auto it = std::find_if(candidates.begin(), candidates.end(), [id = lockedTargetId](const TrackerCandidate &c) { return c.id == id; });
			if (it != candidates.end())
				targetId = lockedTargetId;
		}
		if (!targetId)
			targetId = candidates.front().id;

		lockedTargetId = *targetId;
		targetName = Towners[*targetId].name;
		break;
	}
	case TrackerTargetCategory::Players: {
		const std::vector<TrackerCandidate> candidates = CollectPlayerTrackerCandidates(playerPosition);
		if (candidates.empty()) {
			SpeakText(_("No players found."), true);
			return;
		}

		if (lockedTargetId >= 0 && lockedTargetId < MAX_PLRS) {
			const auto it = std::find_if(candidates.begin(), candidates.end(), [id = lockedTargetId](const TrackerCandidate &c) { return c.id == id; });
			if (it != candidates.end())
				targetId = lockedTargetId;
		}
		if (!targetId)
			targetId = candidates.front().id;

		lockedTargetId = *targetId;
		targetName = Players[*targetId].name();
		break;
	}
	case TrackerTargetCategory::DungeonEntrances: {
		const std::vector<TrackerCandidate> candidates = CollectDungeonEntranceTrackerCandidates(playerPosition);
		if (candidates.empty()) {
			SpeakText(_("No dungeon entrances found."), true);
			return;
		}

		if (lockedTargetId >= 0 && lockedTargetId < numtrigs) {
			const auto it = std::find_if(candidates.begin(), candidates.end(), [id = lockedTargetId](const TrackerCandidate &c) { return c.id == id; });
			if (it != candidates.end())
				targetId = lockedTargetId;
		}
		if (!targetId)
			targetId = candidates.front().id;

		lockedTargetId = *targetId;
		targetName = TriggerLabelForSpeech(trigs[*targetId]);
		break;
	}
	case TrackerTargetCategory::Stairs: {
		const std::vector<TrackerCandidate> candidates = CollectStairsTrackerCandidates(playerPosition);
		if (candidates.empty()) {
			SpeakText(_("No stairs found."), true);
			return;
		}

		if (lockedTargetId >= 0 && lockedTargetId < numtrigs) {
			const auto it = std::find_if(candidates.begin(), candidates.end(), [id = lockedTargetId](const TrackerCandidate &c) { return c.id == id; });
			if (it != candidates.end())
				targetId = lockedTargetId;
		}
		if (!targetId)
			targetId = candidates.front().id;

		lockedTargetId = *targetId;
		targetName = TriggerLabelForSpeech(trigs[*targetId]);
		break;
	}
	case TrackerTargetCategory::QuestLocations: {
		const std::vector<TrackerCandidate> candidates = CollectQuestLocationTrackerCandidates(playerPosition);
		if (candidates.empty()) {
			SpeakText(_("No quest locations found."), true);
			return;
		}

		if ((setlevel && lockedTargetId >= 0 && lockedTargetId < numtrigs) || (!setlevel && lockedTargetId >= 0 && lockedTargetId < static_cast<int>(sizeof(Quests) / sizeof(Quests[0])))) {
			const auto it = std::find_if(candidates.begin(), candidates.end(), [id = lockedTargetId](const TrackerCandidate &c) { return c.id == id; });
			if (it != candidates.end())
				targetId = lockedTargetId;
		}
		if (!targetId)
			targetId = candidates.front().id;

		lockedTargetId = *targetId;
		targetName = std::string(candidates.front().name.str());
		if (const auto it = std::find_if(candidates.begin(), candidates.end(), [id = *targetId](const TrackerCandidate &c) { return c.id == id; }); it != candidates.end())
			targetName = std::string(it->name.str());
		break;
	}
	case TrackerTargetCategory::Portals: {
		const std::vector<TrackerCandidate> candidates = CollectPortalTrackerCandidates(playerPosition);
		if (candidates.empty()) {
			SpeakText(_("No portals found."), true);
			return;
		}

		if (lockedTargetId >= 0 && lockedTargetId < MAXPORTAL) {
			const auto it = std::find_if(candidates.begin(), candidates.end(), [id = lockedTargetId](const TrackerCandidate &c) { return c.id == id; });
			if (it != candidates.end())
				targetId = lockedTargetId;
		}
		if (!targetId)
			targetId = candidates.front().id;

		lockedTargetId = *targetId;
		targetName = TownPortalLabelForSpeech(Portals[*targetId]);
		break;
	}
	}

	if (!targetId)
		return;

	std::string msg;
	StrAppend(msg, _("Going to: "), targetName);
	SpeakText(msg, true);

	AutoWalkTrackerTargetId = *targetId;
	AutoWalkTrackerTargetCategory = SelectedTrackerTargetCategory;
	UpdateAutoWalkTracker();
}

} // namespace

void UpdateAutoWalkTracker()
{
	if (AutoWalkTrackerTargetId < 0)
		return;
	if (IsPlayerInStore() || ChatLogFlag || HelpFlag || InGameMenu()) {
		AutoWalkTrackerTargetId = -1;
		return;
	}
	if (leveltype == DTYPE_TOWN
	    && IsDungeonOnlyTrackerCategory(AutoWalkTrackerTargetCategory)) {
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
	case TrackerTargetCategory::Npcs: {
		const int npcId = AutoWalkTrackerTargetId;
		if (leveltype != DTYPE_TOWN || npcId < 0 || npcId >= static_cast<int>(GetNumTowners())) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target NPC is gone."), true);
			return;
		}
		const Towner &towner = Towners[npcId];
		if (!IsTownerPresent(towner._ttype)) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target NPC is gone."), true);
			return;
		}
		if (playerPosition.WalkingDistance(towner.position) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("NPC in range."), true);
			return;
		}
		destination = FindBestAdjacentApproachTile(myPlayer, playerPosition, towner.position);
		break;
	}
	case TrackerTargetCategory::Players: {
		const int playerId = AutoWalkTrackerTargetId;
		if (playerId < 0 || playerId >= MAX_PLRS) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target player is gone."), true);
			return;
		}
		const Player &player = Players[playerId];
		if (!player.plractive || player._pLvlChanging || player.plrIsOnSetLevel != setlevel || player.plrlevel != MyPlayer->plrlevel) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target player is gone."), true);
			return;
		}
		const Point targetPosition = player.position.future;
		if (!InDungeonBounds(targetPosition)) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target player is gone."), true);
			return;
		}
		if (playerPosition.WalkingDistance(targetPosition) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Player in range."), true);
			return;
		}
		destination = FindBestAdjacentApproachTile(myPlayer, playerPosition, targetPosition);
		break;
	}
	case TrackerTargetCategory::DungeonEntrances: {
		const int triggerIndex = AutoWalkTrackerTargetId;
		if (triggerIndex < 0 || triggerIndex >= numtrigs) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target entrance is gone."), true);
			return;
		}
		const TriggerStruct &trigger = trigs[triggerIndex];
		const bool valid = leveltype == DTYPE_TOWN
		    ? IsAnyOf(trigger._tmsg, WM_DIABNEXTLVL, WM_DIABTOWNWARP)
		    : (setlevel ? trigger._tmsg == WM_DIABRTNLVL : IsAnyOf(trigger._tmsg, WM_DIABPREVLVL, WM_DIABTWARPUP));
		if (!valid) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target entrance is gone."), true);
			return;
		}
		const Point triggerPosition { trigger.position.x, trigger.position.y };
		if (playerPosition.WalkingDistance(triggerPosition) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Entrance in range."), true);
			return;
		}
		destination = triggerPosition;
		break;
	}
	case TrackerTargetCategory::Stairs: {
		const int triggerIndex = AutoWalkTrackerTargetId;
		if (leveltype == DTYPE_TOWN || triggerIndex < 0 || triggerIndex >= numtrigs) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target stairs are gone."), true);
			return;
		}
		const TriggerStruct &trigger = trigs[triggerIndex];
		if (!IsAnyOf(trigger._tmsg, WM_DIABNEXTLVL, WM_DIABPREVLVL, WM_DIABTWARPUP)) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target stairs are gone."), true);
			return;
		}
		const Point triggerPosition { trigger.position.x, trigger.position.y };
		if (playerPosition.WalkingDistance(triggerPosition) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Stairs in range."), true);
			return;
		}
		destination = triggerPosition;
		break;
	}
	case TrackerTargetCategory::QuestLocations: {
		if (setlevel) {
			const int triggerIndex = AutoWalkTrackerTargetId;
			if (leveltype == DTYPE_TOWN || triggerIndex < 0 || triggerIndex >= numtrigs) {
				AutoWalkTrackerTargetId = -1;
				SpeakText(_("Target quest location is gone."), true);
				return;
			}
			const TriggerStruct &trigger = trigs[triggerIndex];
			if (trigger._tmsg != WM_DIABRTNLVL) {
				AutoWalkTrackerTargetId = -1;
				SpeakText(_("Target quest location is gone."), true);
				return;
			}
			const Point triggerPosition { trigger.position.x, trigger.position.y };
			if (playerPosition.WalkingDistance(triggerPosition) <= TrackerInteractDistanceTiles) {
				AutoWalkTrackerTargetId = -1;
				SpeakText(_("Quest exit in range."), true);
				return;
			}
			destination = triggerPosition;
			break;
		}

		const int questIndex = AutoWalkTrackerTargetId;
		if (questIndex < 0 || questIndex >= static_cast<int>(sizeof(Quests) / sizeof(Quests[0]))) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target quest location is gone."), true);
			return;
		}
		const Quest &quest = Quests[static_cast<size_t>(questIndex)];
		if (quest._qslvl == SL_NONE || quest._qactive == QUEST_NOTAVAIL || quest._qlevel != currlevel || !InDungeonBounds(quest.position)) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target quest location is gone."), true);
			return;
		}
		if (playerPosition.WalkingDistance(quest.position) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Quest entrance in range."), true);
			return;
		}
		destination = quest.position;
		break;
	}
	case TrackerTargetCategory::Portals: {
		const int portalIndex = AutoWalkTrackerTargetId;
		std::optional<Point> portalPosition;
		if (leveltype == DTYPE_TOWN) {
			portalPosition = FindTownPortalPositionInTownByPortalIndex(portalIndex);
		} else if (IsTownPortalOpenOnCurrentLevel(portalIndex)) {
			portalPosition = Portals[portalIndex].position;
		}

		if (!portalPosition) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Target portal is gone."), true);
			return;
		}
		if (playerPosition.WalkingDistance(*portalPosition) <= TrackerInteractDistanceTiles) {
			AutoWalkTrackerTargetId = -1;
			SpeakText(_("Portal in range."), true);
			return;
		}
		destination = *portalPosition;
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

void TrackerPageUpKeyPressed()
{
	const SDL_Keymod modState = SDL_GetModState();
	const bool cycleCategory = (modState & SDL_KMOD_CTRL) != 0;

	if (cycleCategory) {
		SelectTrackerTargetCategoryRelative(-1);
		if (MyPlayer != nullptr) {
			const Point playerPosition = MyPlayer->position.future;
			if (CollectTrackerCandidatesForSelection(SelectedTrackerTargetCategory, playerPosition).empty()) {
				if (TrackerCategorySelectionIsProximityLimited(SelectedTrackerTargetCategory) && TrackerCategoryHasAnyTargets(SelectedTrackerTargetCategory, playerPosition))
					SpeakText(TrackerCategoryNoNearbyCandidatesFoundMessage(SelectedTrackerTargetCategory), true);
				else
					SpeakText(TrackerCategoryNoCandidatesFoundMessage(SelectedTrackerTargetCategory), true);
			}
		}
		return;
	}

	SelectTrackerTargetRelative(-1);
}

void TrackerPageDownKeyPressed()
{
	const SDL_Keymod modState = SDL_GetModState();
	const bool cycleCategory = (modState & SDL_KMOD_CTRL) != 0;

	if (cycleCategory) {
		SelectTrackerTargetCategoryRelative(+1);
		if (MyPlayer != nullptr) {
			const Point playerPosition = MyPlayer->position.future;
			if (CollectTrackerCandidatesForSelection(SelectedTrackerTargetCategory, playerPosition).empty()) {
				if (TrackerCategorySelectionIsProximityLimited(SelectedTrackerTargetCategory) && TrackerCategoryHasAnyTargets(SelectedTrackerTargetCategory, playerPosition))
					SpeakText(TrackerCategoryNoNearbyCandidatesFoundMessage(SelectedTrackerTargetCategory), true);
				else
					SpeakText(TrackerCategoryNoCandidatesFoundMessage(SelectedTrackerTargetCategory), true);
			}
		}
		return;
	}

	SelectTrackerTargetRelative(+1);
}

void TrackerHomeKeyPressed()
{
	const SDL_Keymod modState = SDL_GetModState();
	const bool autoWalk = (modState & SDL_KMOD_SHIFT) != 0;

	if (autoWalk)
		AutoWalkToTrackerTargetKeyPressed();
	else
		NavigateToTrackerTargetKeyPressed();
}

void ResetAutoWalkTracker()
{
	AutoWalkTrackerTargetId = -1;
}

} // namespace devilution
