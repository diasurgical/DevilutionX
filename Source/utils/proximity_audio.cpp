#include "utils/proximity_audio.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "controls/plrctrls.h"
#include "engine/assets.hpp"
#include "engine/path.h"
#include "engine/sound.h"
#include "engine/sound_position.hpp"
#include "inv.h"
#include "items.h"
#include "levels/gendung.h"
#include "levels/tile_properties.hpp"
#include "monster.h"
#include "objects.h"
#include "player.h"
#include "utils/is_of.hpp"
#include "utils/math.h"
#include "utils/screen_reader.hpp"
#include "utils/stdcompat/shared_ptr_array.hpp"

namespace devilution {

#ifdef NOSOUND

void UpdateProximityAudioCues()
{
}

#else

namespace {

constexpr int MaxCueDistanceTiles = 12;
constexpr int InteractDistanceTiles = 1;

// Pitch shifting via resampling caused audible glitches on some setups; keep cues at normal pitch for stability.
constexpr size_t PitchLevels = 1;

constexpr uint32_t MinIntervalMs = 250;
constexpr uint32_t MaxIntervalMs = 1000;

// Extra attenuation applied on top of CalculateSoundPosition().
// Kept at 0 because stronger attenuation makes distant proximity cues too quiet and feel "glitchy"/missing.
constexpr int ExtraAttenuationMax = 0;

struct CueSound {
	std::array<std::unique_ptr<TSnd>, PitchLevels> variants;

	[[nodiscard]] bool IsLoaded() const
	{
		for (const auto &variant : variants) {
			if (variant != nullptr && variant->DSB.IsLoaded())
				return true;
		}
		return false;
	}

	[[nodiscard]] bool IsAnyPlaying() const
	{
		for (const auto &variant : variants) {
			if (variant != nullptr && variant->DSB.IsLoaded() && variant->DSB.IsPlaying())
				return true;
		}
		return false;
	}
};

std::optional<CueSound> WeaponItemCue;
std::optional<CueSound> ArmorItemCue;
std::optional<CueSound> GoldItemCue;
std::optional<CueSound> ChestCue;
std::optional<CueSound> DoorCue;
std::optional<CueSound> MonsterCue;
std::optional<CueSound> InteractCue;

std::array<uint32_t, MAXOBJECTS> LastObjectCueTimeMs {};
uint32_t LastMonsterCueTimeMs = 0;
std::optional<uint32_t> LastInteractableId;
uint32_t LastWeaponItemCueTimeMs = 0;
uint32_t LastArmorItemCueTimeMs = 0;
uint32_t LastGoldItemCueTimeMs = 0;

enum class InteractTargetType : uint8_t {
	Item,
	Object,
};

struct InteractTarget {
	InteractTargetType type;
	int id;
	Point position;
};

[[nodiscard]] bool EndsWithCaseInsensitive(std::string_view str, std::string_view suffix)
{
	if (str.size() < suffix.size())
		return false;
	const std::string_view tail { str.data() + (str.size() - suffix.size()), suffix.size() };
	return std::equal(tail.begin(), tail.end(), suffix.begin(), suffix.end(), [](char a, char b) {
		return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
	});
}

[[nodiscard]] bool IsMp3Path(std::string_view path)
{
	return EndsWithCaseInsensitive(path, ".mp3");
}

[[nodiscard]] float PlaybackRateForPitchLevel(size_t level)
{
	(void)level;
	return 1.0F;
}

[[nodiscard]] size_t PitchLevelForDistance(int distance, int maxDistance)
{
	(void)distance;
	(void)maxDistance;
	return 0;
}

[[nodiscard]] uint32_t IntervalMsForDistance(int distance, int maxDistance)
{
	if (maxDistance <= 0)
		return MinIntervalMs;

	const float t = std::clamp(static_cast<float>(distance) / static_cast<float>(maxDistance), 0.0F, 1.0F);
	const float closeness = 1.0F - t;
	const float interval = static_cast<float>(MaxIntervalMs) - closeness * static_cast<float>(MaxIntervalMs - MinIntervalMs);
	return static_cast<uint32_t>(std::lround(interval));
}

void StopCueSound(const CueSound &cue)
{
	for (const auto &variant : cue.variants) {
		if (variant != nullptr && variant->DSB.IsLoaded())
			variant->DSB.Stop();
	}
}

void StopAllCuesExcept(const CueSound *cueToKeep)
{
	const auto stopIfOther = [cueToKeep](const std::optional<CueSound> &cue) {
		if (cue && &*cue != cueToKeep)
			StopCueSound(*cue);
	};

	stopIfOther(WeaponItemCue);
	stopIfOther(ArmorItemCue);
	stopIfOther(GoldItemCue);
	stopIfOther(ChestCue);
	stopIfOther(DoorCue);
	stopIfOther(MonsterCue);
	stopIfOther(InteractCue);
}

[[nodiscard]] bool IsAnyOtherCuePlaying(const CueSound *cueToIgnore)
{
	const auto isOtherPlaying = [cueToIgnore](const std::optional<CueSound> &cue) {
		return cue && cue->IsAnyPlaying() && &*cue != cueToIgnore;
	};

	return isOtherPlaying(WeaponItemCue) || isOtherPlaying(ArmorItemCue) || isOtherPlaying(GoldItemCue) || isOtherPlaying(ChestCue) || isOtherPlaying(DoorCue)
	    || isOtherPlaying(MonsterCue) || isOtherPlaying(InteractCue);
}

std::optional<CueSound> TryLoadCueSound(std::initializer_list<std::string_view> candidatePaths)
{
	if (!gbSndInited)
		return std::nullopt;

	for (std::string_view path : candidatePaths) {
		AssetRef ref = FindAsset(path);
		if (!ref.ok())
			continue;

		const size_t size = ref.size();
		if (size == 0)
			continue;

		AssetHandle handle = OpenAsset(std::move(ref), /*threadsafe=*/true);
		if (!handle.ok())
			continue;

		auto fileData = MakeArraySharedPtr<std::uint8_t>(size);
		if (!handle.read(fileData.get(), size))
			continue;

		CueSound cue {};
		bool ok = true;

		for (size_t i = 0; i < PitchLevels; ++i) {
			auto snd = std::make_unique<TSnd>();
			snd->start_tc = SDL_GetTicks() - 80 - 1;
#ifndef NOSOUND
			const bool isMp3 = IsMp3Path(path);
			if (snd->DSB.SetChunk(fileData, size, isMp3, PlaybackRateForPitchLevel(i)) != 0) {
				ok = false;
				break;
			}
#endif
			cue.variants[i] = std::move(snd);
		}

		if (ok)
			return cue;
	}

	return std::nullopt;
}

void EnsureCuesLoaded()
{
	static bool loaded = false;
	if (loaded)
		return;

	WeaponItemCue = TryLoadCueSound({ "audio\\weapon.ogg", "..\\audio\\weapon.ogg", "audio\\weapon.wav", "..\\audio\\weapon.wav", "audio\\weapon.mp3", "..\\audio\\weapon.mp3" });
	ArmorItemCue = TryLoadCueSound({ "audio\\armor.ogg", "..\\audio\\armor.ogg", "audio\\armor.wav", "..\\audio\\armor.wav", "audio\\armor.mp3", "..\\audio\\armor.mp3" });
	GoldItemCue = TryLoadCueSound({ "audio\\coin.ogg", "..\\audio\\coin.ogg", "audio\\coin.wav", "..\\audio\\coin.wav", "audio\\coin.mp3", "..\\audio\\coin.mp3" });

	ChestCue = TryLoadCueSound({ "audio\\chest.ogg", "..\\audio\\chest.ogg", "audio\\chest.wav", "..\\audio\\chest.wav", "audio\\chest.mp3", "..\\audio\\chest.mp3" });
	DoorCue = TryLoadCueSound({ "audio\\door.ogg", "..\\audio\\door.ogg", "audio\\door.wav", "..\\audio\\door.wav", "audio\\Door.wav", "..\\audio\\Door.wav", "audio\\door.mp3", "..\\audio\\door.mp3" });

	MonsterCue = TryLoadCueSound({ "audio\\monster.ogg", "..\\audio\\monster.ogg", "audio\\monster.wav", "..\\audio\\monster.wav", "audio\\monster.mp3", "..\\audio\\monster.mp3" });

	InteractCue = TryLoadCueSound({
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

	loaded = true;
}

[[nodiscard]] bool IsAnyCuePlaying()
{
	const auto isAnyPlaying = [](const std::optional<CueSound> &cue) {
		return cue && cue->IsAnyPlaying();
	};
	return isAnyPlaying(WeaponItemCue) || isAnyPlaying(ArmorItemCue) || isAnyPlaying(GoldItemCue) || isAnyPlaying(ChestCue) || isAnyPlaying(DoorCue)
	    || isAnyPlaying(MonsterCue) || isAnyPlaying(InteractCue);
}

[[nodiscard]] bool PlayCueAt(const CueSound &cue, Point position, int distance, int maxDistance)
{
	if (!gbSndInited || !gbSoundOn)
		return false;

	// Allow the same cue to restart (for tempo control), but don't overlap different cue types.
	if (IsAnyOtherCuePlaying(&cue))
		return false;

	int logVolume = 0;
	int logPan = 0;
	if (!CalculateSoundPosition(position, &logVolume, &logPan))
		return false;

	const int extraAttenuation = static_cast<int>(std::lround(math::Remap(0, maxDistance, 0, ExtraAttenuationMax, distance)));
	logVolume = std::max(ATTENUATION_MIN, logVolume - extraAttenuation);
	if (logVolume <= ATTENUATION_MIN)
		return false;

	const size_t pitchLevel = std::min(PitchLevels - 1, PitchLevelForDistance(distance, maxDistance));
	TSnd *snd = cue.variants[pitchLevel].get();
	if (snd == nullptr || !snd->DSB.IsLoaded())
		return false;

	// Restart the cue if it's already playing so we can control tempo based on distance.
	// Stop all variants to avoid overlaps when pitch levels are enabled.
	if (cue.IsAnyPlaying()) {
		for (const auto &variant : cue.variants) {
			if (variant != nullptr && variant->DSB.IsLoaded())
				variant->DSB.Stop();
		}
	}

	snd_play_snd(snd, logVolume, logPan);
	return true;
}

[[nodiscard]] bool UpdateItemCues(const Point playerPosition, uint32_t now)
{
	struct Candidate {
		item_class itemClass;
		int distance;
		Point position;
	};

	std::optional<Candidate> nearest;

	for (uint8_t i = 0; i < ActiveItemCount; i++) {
		const int itemId = ActiveItems[i];
		const Item &item = Items[itemId];

		switch (item._iClass) {
		case ICLASS_WEAPON:
			break;
		case ICLASS_ARMOR:
			break;
		case ICLASS_GOLD:
			break;
		default:
			continue;
		}

		const int distance = playerPosition.ApproxDistance(item.position);
		if (distance > MaxCueDistanceTiles)
			continue;

		if (!nearest || distance < nearest->distance)
			nearest = Candidate { item._iClass, distance, item.position };
	}

	if (!nearest)
		return false;

	const CueSound *cue = nullptr;
	uint32_t *lastTimeMs = nullptr;
	switch (nearest->itemClass) {
	case ICLASS_WEAPON:
		if (WeaponItemCue && WeaponItemCue->IsLoaded())
			cue = &*WeaponItemCue;
		lastTimeMs = &LastWeaponItemCueTimeMs;
		break;
	case ICLASS_ARMOR:
		if (ArmorItemCue && ArmorItemCue->IsLoaded())
			cue = &*ArmorItemCue;
		lastTimeMs = &LastArmorItemCueTimeMs;
		break;
	case ICLASS_GOLD:
		if (GoldItemCue && GoldItemCue->IsLoaded())
			cue = &*GoldItemCue;
		lastTimeMs = &LastGoldItemCueTimeMs;
		break;
	default:
		return false;
	}

	if (cue == nullptr || lastTimeMs == nullptr)
		return false;

	const int distance = nearest->distance;
	const uint32_t intervalMs = IntervalMsForDistance(distance, MaxCueDistanceTiles);
	if (now - *lastTimeMs < intervalMs)
		return false;

	if (PlayCueAt(*cue, nearest->position, distance, MaxCueDistanceTiles)) {
		*lastTimeMs = now;
		return true;
	}

	return false;
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

[[nodiscard]] bool UpdateObjectCues(const Point playerPosition, uint32_t now)
{
	struct Candidate {
		int objectId;
		int distance;
		const CueSound *cue;
	};

	std::optional<Candidate> nearest;

	for (int i = 0; i < ActiveObjectCount; i++) {
		const int objectId = ActiveObjects[i];
		const Object &object = Objects[objectId];
		if (!object.canInteractWith())
			continue;
		if (!object.isDoor() && !object.IsChest())
			continue;

		const int distance = playerPosition.ApproxDistance(object.position);
		if (distance > MaxCueDistanceTiles)
			continue;

		const CueSound *cue = nullptr;
		if (object.IsChest()) {
			if (ChestCue && ChestCue->IsLoaded())
				cue = &*ChestCue;
		} else if (object.isDoor()) {
			if (DoorCue && DoorCue->IsLoaded())
				cue = &*DoorCue;
		}

		if (cue == nullptr)
			continue;

		if (!nearest || distance < nearest->distance)
			nearest = Candidate { objectId, distance, cue };
	}

	if (!nearest)
		return false;

	const int objectId = nearest->objectId;
	const int distance = nearest->distance;
	const uint32_t intervalMs = IntervalMsForDistance(distance, MaxCueDistanceTiles);
	if (now - LastObjectCueTimeMs[objectId] < intervalMs)
		return false;

	if (PlayCueAt(*nearest->cue, Objects[objectId].position, distance, MaxCueDistanceTiles)) {
		LastObjectCueTimeMs[objectId] = now;
		return true;
	}

	return false;
}

[[nodiscard]] bool UpdateMonsterCue(const Point playerPosition, uint32_t now)
{
	if (!MonsterCue || !MonsterCue->IsLoaded())
		return false;

	std::optional<std::pair<int, Point>> nearest;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int monsterId = static_cast<int>(ActiveMonsters[i]);
		const Monster &monster = Monsters[monsterId];

		if (monster.isInvalid)
			continue;
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			continue;
		if (monster.hitPoints <= 0)
			continue;

		// Use the future position for distance/tempo so cues react immediately when a monster starts moving
		// towards or away from the player (tile position updates later).
		const Point monsterSoundPosition { monster.position.tile };
		const Point monsterDistancePosition { monster.position.future };
		const int distance = playerPosition.ApproxDistance(monsterDistancePosition);
		if (distance > MaxCueDistanceTiles)
			continue;

		if (!nearest || distance < nearest->first) {
			nearest = { distance, monsterSoundPosition };
		}
	}

	if (!nearest)
		return false;

	const int distance = nearest->first;
	const uint32_t intervalMs = IntervalMsForDistance(distance, MaxCueDistanceTiles);
	if (now - LastMonsterCueTimeMs < intervalMs)
		return false;

	if (!PlayCueAt(*MonsterCue, nearest->second, distance, MaxCueDistanceTiles))
		return false;

	LastMonsterCueTimeMs = now;
	return true;
}

[[nodiscard]] bool UpdateInteractCue(const Point playerPosition, uint32_t now)
{
	if (!InteractCue || !InteractCue->IsLoaded())
		return false;

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

	// Make the interact cue reliably audible even when another proximity cue is playing.
	// (The new distance-based tempo restarts cues frequently.)
	StopAllCuesExcept(InteractCue ? &*InteractCue : nullptr);

	if (!InteractCue || !InteractCue->IsLoaded())
		return true;

	if (!PlayCueAt(*InteractCue, target->position, /*distance=*/0, /*maxDistance=*/1))
		return true;
	(void)now;
	return true;
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

	EnsureCuesLoaded();

	const uint32_t now = SDL_GetTicks();
	const Point playerPosition { MyPlayer->position.future };

	// Keep cues readable and reduce overlap/glitches by playing at most one per tick (priority order).
	if (UpdateInteractCue(playerPosition, now))
		return;
	if (UpdateMonsterCue(playerPosition, now))
		return;
	if (UpdateItemCues(playerPosition, now))
		return;
	(void)UpdateObjectCues(playerPosition, now);
}

#endif // NOSOUND

} // namespace devilution
