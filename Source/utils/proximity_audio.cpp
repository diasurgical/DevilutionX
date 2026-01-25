#include "utils/proximity_audio.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "controls/plrctrls.h"
#include "engine/path.h"
#include "engine/sound.h"
#include "engine/sound_pool.hpp"
#include "inv.h"
#include "items.h"
#include "levels/gendung.h"
#include "levels/tile_properties.hpp"
#include "monster.h"
#include "objects.h"
#include "player.h"
#include "utils/screen_reader.hpp"

namespace devilution {

#ifdef NOSOUND

void UpdateProximityAudioCues()
{
}

#else

namespace {

constexpr int MaxCueDistanceTiles = 12;
constexpr int InteractDistanceTiles = 1;
constexpr size_t MaxEmitters = 3;

constexpr uint32_t MinIntervalMs = 250;
constexpr uint32_t MaxIntervalMs = 1000;
// Monster movement already provides a lot of information; keep the tempo a bit faster.
constexpr uint32_t MinMonsterIntervalMs = 100;
constexpr uint32_t MaxMonsterIntervalMs = 1000;

std::optional<uint32_t> LastInteractableId;

enum class InteractTargetType : uint8_t {
	Item,
	Object,
};

struct InteractTarget {
	InteractTargetType type;
	int id;
	Point position;
};

enum class EmitterType : uint8_t {
	Item = 1,
	Object = 2,
	Monster = 3,
};

[[nodiscard]] constexpr uint32_t MakeEmitterId(EmitterType type, uint32_t id)
{
	return (static_cast<uint32_t>(type) << 24) | (id & 0x00FFFFFF);
}

[[nodiscard]] uint32_t IntervalMsForDistance(int distance, int maxDistance, uint32_t minIntervalMs, uint32_t maxIntervalMs)
{
	if (maxDistance <= 0)
		return minIntervalMs;

	const float t = std::clamp(static_cast<float>(distance) / static_cast<float>(maxDistance), 0.0F, 1.0F);
	const float closeness = 1.0F - t;
	const float interval = static_cast<float>(maxIntervalMs) - closeness * static_cast<float>(maxIntervalMs - minIntervalMs);
	return static_cast<uint32_t>(std::lround(interval));
}

[[nodiscard]] int GetRotaryDistanceForInteractTarget(const Player &player, Point destination)
{
	if (player.position.future == destination)
		return -1;

	const int d1 = static_cast<int>(player._pdir);
	const int d2 = static_cast<int>(GetDirection(player.position.future, destination));

	const int d = std::abs(d1 - d2);
	if (d > 4)
		return 4 - (d % 4);

	return d;
}

[[nodiscard]] bool IsReachableWithinSteps(const Player &player, Point start, Point destination, size_t maxSteps)
{
	if (maxSteps == 0)
		return start == destination;

	if (start == destination)
		return true;

	if (start.WalkingDistance(destination) > static_cast<int>(maxSteps))
		return false;

	std::array<int8_t, InteractDistanceTiles> path;
	path.fill(WALK_NONE);

	const int steps = FindPath(CanStep, [&player](Point position) { return PosOkPlayer(player, position); }, start, destination, path.data(), path.size());
	return steps != 0 && steps <= static_cast<int>(maxSteps);
}

std::optional<InteractTarget> FindInteractTargetInRange(const Player &player, Point playerPosition)
{
	int rotations = 5;
	std::optional<InteractTarget> best;

	for (int dx = -1; dx <= 1; ++dx) {
		for (int dy = -1; dy <= 1; ++dy) {
			const Point targetPosition { playerPosition.x + dx, playerPosition.y + dy };
			if (!InDungeonBounds(targetPosition))
				continue;

			const int itemId = dItem[targetPosition.x][targetPosition.y] - 1;
			if (itemId < 0)
				continue;

			const Item &item = Items[itemId];
			if (item.isEmpty() || item.selectionRegion == SelectionRegion::None)
				continue;

			const int newRotations = GetRotaryDistanceForInteractTarget(player, targetPosition);
			if (rotations < newRotations)
				continue;
			if (targetPosition != playerPosition && !IsReachableWithinSteps(player, playerPosition, targetPosition, InteractDistanceTiles))
				continue;

			rotations = newRotations;
			best = InteractTarget { .type = InteractTargetType::Item, .id = itemId, .position = targetPosition };
		}
	}

	if (best)
		return best;

	rotations = 5;

	for (int dx = -1; dx <= 1; ++dx) {
		for (int dy = -1; dy <= 1; ++dy) {
			const Point targetPosition { playerPosition.x + dx, playerPosition.y + dy };
			if (!InDungeonBounds(targetPosition))
				continue;

			Object *object = FindObjectAtPosition(targetPosition);
			if (object == nullptr || !object->canInteractWith())
				continue;
			if (!object->isDoor() && !object->IsChest())
				continue;
			if (object->IsDisabled())
				continue;
			if (targetPosition == playerPosition && object->_oDoorFlag)
				continue;

			const int newRotations = GetRotaryDistanceForInteractTarget(player, targetPosition);
			if (rotations < newRotations)
				continue;
			if (targetPosition != playerPosition && !IsReachableWithinSteps(player, playerPosition, targetPosition, InteractDistanceTiles))
				continue;

			const int objectId = static_cast<int>(object - Objects);
			rotations = newRotations;
			best = InteractTarget { .type = InteractTargetType::Object, .id = objectId, .position = targetPosition };
		}
	}

	return best;
}

[[nodiscard]] bool UpdateInteractCue(const Point playerPosition, uint32_t now)
{
	if (MyPlayer == nullptr)
		return false;

	const Player &player = *MyPlayer;
	const std::optional<InteractTarget> target = FindInteractTargetInRange(player, playerPosition);
	if (!target) {
		LastInteractableId = std::nullopt;
		return false;
	}

	const uint32_t id = target->type == InteractTargetType::Item
	    ? (1U << 16) | static_cast<uint32_t>(target->id)
	    : (2U << 16) | static_cast<uint32_t>(target->id);
	if (LastInteractableId && *LastInteractableId == id)
		return false;

	LastInteractableId = id;

	if (!invflag) {
		if (target->type == InteractTargetType::Item) {
			const Item &item = Items[target->id];
			const StringOrView name = item.getName();
			if (!name.empty())
				SpeakText(name.str(), /*force=*/true);
		} else {
			const Object &object = Objects[target->id];
			const StringOrView name = object.name();
			if (!name.empty())
				SpeakText(name.str(), /*force=*/true);
		}
	}

	SoundPool &pool = SoundPool::Get();
	if (pool.IsLoaded(SoundPool::SoundId::Interact))
		pool.PlayOneShot(SoundPool::SoundId::Interact, target->position, /*stopEmitters=*/true, now);

	return true;
}

void EnsureNavigationSoundsLoaded(SoundPool &pool)
{
	(void)pool.EnsureLoaded(SoundPool::SoundId::WeaponItem, { "audio\\weapon.ogg", "..\\audio\\weapon.ogg", "audio\\weapon.wav", "..\\audio\\weapon.wav", "audio\\weapon.mp3", "..\\audio\\weapon.mp3" });
	(void)pool.EnsureLoaded(SoundPool::SoundId::ArmorItem, { "audio\\armor.ogg", "..\\audio\\armor.ogg", "audio\\armor.wav", "..\\audio\\armor.wav", "audio\\armor.mp3", "..\\audio\\armor.mp3" });
	(void)pool.EnsureLoaded(SoundPool::SoundId::GoldItem, { "audio\\coin.ogg", "..\\audio\\coin.ogg", "audio\\coin.wav", "..\\audio\\coin.wav", "audio\\coin.mp3", "..\\audio\\coin.mp3" });

	(void)pool.EnsureLoaded(SoundPool::SoundId::Chest, { "audio\\chest.ogg", "..\\audio\\chest.ogg", "audio\\chest.wav", "..\\audio\\chest.wav", "audio\\chest.mp3", "..\\audio\\chest.mp3" });
	(void)pool.EnsureLoaded(SoundPool::SoundId::Door, { "audio\\door.ogg", "..\\audio\\door.ogg", "audio\\door.wav", "..\\audio\\door.wav", "audio\\Door.wav", "..\\audio\\Door.wav", "audio\\door.mp3", "..\\audio\\door.mp3" });

	(void)pool.EnsureLoaded(SoundPool::SoundId::Monster, { "audio\\monster.ogg", "..\\audio\\monster.ogg", "audio\\monster.wav", "..\\audio\\monster.wav", "audio\\monster.mp3", "..\\audio\\monster.mp3" });

	(void)pool.EnsureLoaded(SoundPool::SoundId::Interact, {
	                                            "audio\\interactispossible.ogg",
	                                            "audio\\interactionispossible.ogg",
	                                            "..\\audio\\interactispossible.ogg",
	                                            "..\\audio\\interactionispossible.ogg",
	                                            "audio\\interactispossible.wav",
	                                            "audio\\interactionispossible.wav",
	                                            "..\\audio\\interactispossible.wav",
	                                            "..\\audio\\interactionispossible.wav",
	                                            "audio\\interactispossible.mp3",
	                                            "audio\\interactionispossible.mp3",
	                                            "..\\audio\\interactispossible.mp3",
	                                            "..\\audio\\interactionispossible.mp3",
	                                        });
}

struct CandidateEmitter {
	uint32_t emitterId;
	SoundPool::SoundId sound;
	Point position;
	int distance;
	uint32_t intervalMs;
};

[[nodiscard]] bool IsBetterCandidate(const CandidateEmitter &a, const CandidateEmitter &b)
{
	if (a.distance != b.distance)
		return a.distance < b.distance;
	return a.emitterId < b.emitterId;
}

void ConsiderCandidate(std::array<std::optional<CandidateEmitter>, MaxEmitters> &best, CandidateEmitter candidate)
{
	for (size_t i = 0; i < best.size(); ++i) {
		if (!best[i] || IsBetterCandidate(candidate, *best[i])) {
			for (size_t j = best.size() - 1; j > i; --j)
				best[j] = best[j - 1];
			best[i] = std::move(candidate);
			return;
		}
	}
}

} // namespace

void UpdateProximityAudioCues()
{
	if (!gbSndInited || !gbSoundOn)
		return;
	if (leveltype == DTYPE_TOWN)
		return;
	if (MyPlayer == nullptr || MyPlayerIsDead || MyPlayer->_pmode == PM_DEATH)
		return;
	if (InGameMenu())
		return;

	SoundPool &pool = SoundPool::Get();
	EnsureNavigationSoundsLoaded(pool);

	const uint32_t now = SDL_GetTicks();
	const Point playerPosition { MyPlayer->position.future };

	// Interact cue is a one-shot that should be clearly audible and not counted in the 3-emitter limit.
	if (UpdateInteractCue(playerPosition, now))
		return;

	std::array<std::optional<CandidateEmitter>, MaxEmitters> best;
	best.fill(std::nullopt);

	for (uint8_t i = 0; i < ActiveItemCount; i++) {
		const int itemId = ActiveItems[i];
		const Item &item = Items[itemId];

		SoundPool::SoundId soundId;
		switch (item._iClass) {
		case ICLASS_WEAPON:
			soundId = SoundPool::SoundId::WeaponItem;
			break;
		case ICLASS_ARMOR:
			soundId = SoundPool::SoundId::ArmorItem;
			break;
		case ICLASS_GOLD:
			soundId = SoundPool::SoundId::GoldItem;
			break;
		default:
			continue;
		}

		if (!pool.IsLoaded(soundId))
			continue;

		const int distance = playerPosition.ApproxDistance(item.position);
		if (distance > MaxCueDistanceTiles)
			continue;

		ConsiderCandidate(best, CandidateEmitter {
		                        .emitterId = MakeEmitterId(EmitterType::Item, static_cast<uint32_t>(itemId)),
		                        .sound = soundId,
		                        .position = item.position,
		                        .distance = distance,
		                        .intervalMs = IntervalMsForDistance(distance, MaxCueDistanceTiles, MinIntervalMs, MaxIntervalMs),
		                    });
	}

	for (int i = 0; i < ActiveObjectCount; i++) {
		const int objectId = ActiveObjects[i];
		const Object &object = Objects[objectId];
		if (!object.canInteractWith())
			continue;
		if (!object.isDoor() && !object.IsChest())
			continue;

		SoundPool::SoundId soundId;
		if (object.IsChest()) {
			soundId = SoundPool::SoundId::Chest;
		} else {
			soundId = SoundPool::SoundId::Door;
		}

		if (!pool.IsLoaded(soundId))
			continue;

		const int distance = playerPosition.ApproxDistance(object.position);
		if (distance > MaxCueDistanceTiles)
			continue;

		ConsiderCandidate(best, CandidateEmitter {
		                        .emitterId = MakeEmitterId(EmitterType::Object, static_cast<uint32_t>(objectId)),
		                        .sound = soundId,
		                        .position = object.position,
		                        .distance = distance,
		                        .intervalMs = IntervalMsForDistance(distance, MaxCueDistanceTiles, MinIntervalMs, MaxIntervalMs),
		                    });
	}

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int monsterId = static_cast<int>(ActiveMonsters[i]);
		const Monster &monster = Monsters[monsterId];

		if (monster.isInvalid)
			continue;
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			continue;
		if (monster.hitPoints <= 0)
			continue;

		if (!pool.IsLoaded(SoundPool::SoundId::Monster))
			continue;

		// Use the future position for distance/tempo so cues react immediately when a monster starts moving.
		const Point monsterSoundPosition { monster.position.tile };
		const Point monsterDistancePosition { monster.position.future };
		const int distance = playerPosition.ApproxDistance(monsterDistancePosition);
		if (distance > MaxCueDistanceTiles)
			continue;

		ConsiderCandidate(best, CandidateEmitter {
		                        .emitterId = MakeEmitterId(EmitterType::Monster, static_cast<uint32_t>(monsterId)),
		                        .sound = SoundPool::SoundId::Monster,
		                        .position = monsterSoundPosition,
		                        .distance = distance,
		                        .intervalMs = IntervalMsForDistance(distance, MaxCueDistanceTiles, MinMonsterIntervalMs, MaxMonsterIntervalMs),
		                    });
	}

	std::array<SoundPool::EmitterRequest, MaxEmitters> requests;
	size_t requestCount = 0;
	for (const auto &entry : best) {
		if (!entry)
			continue;
		requests[requestCount++] = SoundPool::EmitterRequest {
			.emitterId = entry->emitterId,
			.sound = entry->sound,
			.position = entry->position,
			.intervalMs = entry->intervalMs,
		};
	}

	pool.UpdateEmitters(std::span<const SoundPool::EmitterRequest>(requests.data(), requestCount), now);
}

#endif // NOSOUND

} // namespace devilution
