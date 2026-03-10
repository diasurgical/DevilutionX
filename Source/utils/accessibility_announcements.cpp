/**
 * @file utils/accessibility_announcements.cpp
 *
 * Periodic accessibility announcements (low HP warning, durability, boss health,
 * attackable monsters, interactable doors).
 */
#include "utils/accessibility_announcements.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "controls/plrctrls.h"
#include "engine/sound.h"
#include "gamemenu.h"
#include "inv.h"
#include "items.h"
#include "levels/gendung.h"
#include "monster.h"
#include "objects.h"
#include "player.h"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/screen_reader.hpp"
#include "utils/str_cat.hpp"
#include "utils/string_or_view.hpp"

namespace devilution {

#ifdef NOSOUND
void UpdatePlayerLowHpWarningSound()
{
}
#else
namespace {

std::unique_ptr<TSnd> PlayerLowHpWarningSound;
bool TriedLoadingPlayerLowHpWarningSound = false;

TSnd *GetPlayerLowHpWarningSound()
{
	if (TriedLoadingPlayerLowHpWarningSound)
		return PlayerLowHpWarningSound.get();
	TriedLoadingPlayerLowHpWarningSound = true;

	if (!gbSndInited)
		return nullptr;

	PlayerLowHpWarningSound = std::make_unique<TSnd>();
	PlayerLowHpWarningSound->start_tc = SDL_GetTicks() - 80 - 1;

	// Support both the new "playerhaslowhp" name and the older underscore version.
	if (PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\playerhaslowhp.ogg", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\playerhaslowhp.ogg", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\player_has_low_hp.ogg", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\player_has_low_hp.ogg", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\playerhaslowhp.mp3", /*isMp3=*/true, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\playerhaslowhp.mp3", /*isMp3=*/true, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\player_has_low_hp.mp3", /*isMp3=*/true, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\player_has_low_hp.mp3", /*isMp3=*/true, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\playerhaslowhp.wav", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\playerhaslowhp.wav", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("audio\\player_has_low_hp.wav", /*isMp3=*/false, /*logErrors=*/false) != 0
	    && PlayerLowHpWarningSound->DSB.SetChunkStream("..\\audio\\player_has_low_hp.wav", /*isMp3=*/false, /*logErrors=*/false) != 0) {
		LogWarn("Failed to load low HP warning sound from any of the expected paths.");
		PlayerLowHpWarningSound = nullptr;
	}

	return PlayerLowHpWarningSound.get();
}

void StopPlayerLowHpWarningSound()
{
	if (PlayerLowHpWarningSound != nullptr)
		PlayerLowHpWarningSound->DSB.Stop();
}

[[nodiscard]] uint32_t LowHpIntervalMs(int hpPercent)
{
	// The sound starts at 50% HP (slow) and speeds up every 10% down to 0%.
	if (hpPercent > 40)
		return 1500;
	if (hpPercent > 30)
		return 1200;
	if (hpPercent > 20)
		return 900;
	if (hpPercent > 10)
		return 600;
	return 300;
}

} // namespace

void UpdatePlayerLowHpWarningSound()
{
	static uint32_t LastWarningStartMs = 0;

	if (!gbSndInited || !gbSoundOn || MyPlayer == nullptr || InGameMenu()) {
		StopPlayerLowHpWarningSound();
		LastWarningStartMs = 0;
		return;
	}

	// Stop immediately when dead.
	if (MyPlayerIsDead || MyPlayer->_pmode == PM_DEATH || MyPlayer->hasNoLife()) {
		StopPlayerLowHpWarningSound();
		LastWarningStartMs = 0;
		return;
	}

	const int maxHp = MyPlayer->_pMaxHP;
	if (maxHp <= 0) {
		StopPlayerLowHpWarningSound();
		LastWarningStartMs = 0;
		return;
	}

	const int hp = std::clamp(MyPlayer->_pHitPoints, 0, maxHp);
	const int hpPercent = std::clamp(hp * 100 / maxHp, 0, 100);

	// Only play below (or equal to) 50% and above 0%.
	if (hpPercent > 50 || hpPercent <= 0) {
		StopPlayerLowHpWarningSound();
		LastWarningStartMs = 0;
		return;
	}

	TSnd *snd = GetPlayerLowHpWarningSound();
	if (snd == nullptr || !snd->DSB.IsLoaded())
		return;

	const uint32_t now = SDL_GetTicks();
	const uint32_t intervalMs = LowHpIntervalMs(hpPercent);
	if (LastWarningStartMs == 0)
		LastWarningStartMs = now - intervalMs;
	if (now - LastWarningStartMs < intervalMs)
		return;

	// Restart the cue even if it's already playing so the "tempo" is controlled by HP.
	snd->DSB.Stop();
	snd_play_snd(snd, /*lVolume=*/0, /*lPan=*/0);
	LastWarningStartMs = now;
}
#endif // NOSOUND

namespace {

[[nodiscard]] bool IsBossMonsterForHpAnnouncement(const Monster &monster)
{
	return monster.isUnique() || monster.ai == MonsterAIID::Diablo;
}

} // namespace

void UpdateLowDurabilityWarnings()
{
	static std::array<uint32_t, NUM_INVLOC> WarnedSeeds {};
	static std::array<bool, NUM_INVLOC> HasWarned {};

	if (MyPlayer == nullptr)
		return;
	if (MyPlayerIsDead || MyPlayer->_pmode == PM_DEATH || MyPlayer->hasNoLife())
		return;

	std::vector<std::string> newlyLow;
	newlyLow.reserve(NUM_INVLOC);

	for (int slot = 0; slot < NUM_INVLOC; ++slot) {
		const Item &item = MyPlayer->InvBody[slot];
		if (item.isEmpty() || item._iMaxDur <= 0 || item._iMaxDur == DUR_INDESTRUCTIBLE || item._iDurability == DUR_INDESTRUCTIBLE) {
			HasWarned[slot] = false;
			continue;
		}

		const int maxDur = item._iMaxDur;
		const int durability = item._iDurability;
		if (durability <= 0) {
			HasWarned[slot] = false;
			continue;
		}

		int threshold = std::max(2, maxDur / 10);
		threshold = std::clamp(threshold, 1, maxDur);

		const bool isLow = durability <= threshold;
		if (!isLow) {
			HasWarned[slot] = false;
			continue;
		}

		if (HasWarned[slot] && WarnedSeeds[slot] == item._iSeed)
			continue;

		HasWarned[slot] = true;
		WarnedSeeds[slot] = item._iSeed;

		const StringOrView name = item.getName();
		if (!name.empty())
			newlyLow.emplace_back(name.str().data(), name.str().size());
	}

	if (newlyLow.empty())
		return;

	// Add ordinal numbers for duplicates (e.g. two rings with the same name).
	for (size_t i = 0; i < newlyLow.size(); ++i) {
		int total = 0;
		for (size_t j = 0; j < newlyLow.size(); ++j) {
			if (newlyLow[j] == newlyLow[i])
				++total;
		}
		if (total <= 1)
			continue;

		int ordinal = 1;
		for (size_t j = 0; j < i; ++j) {
			if (newlyLow[j] == newlyLow[i])
				++ordinal;
		}
		newlyLow[i] = fmt::format("{} {}", newlyLow[i], ordinal);
	}

	std::string joined;
	for (size_t i = 0; i < newlyLow.size(); ++i) {
		if (i != 0)
			joined += ", ";
		joined += newlyLow[i];
	}

	SpeakText(fmt::format(fmt::runtime(_("Low durability: {:s}")), joined), /*force=*/true);
}

void UpdateBossHealthAnnouncements()
{
	static dungeon_type LastLevelType = DTYPE_NONE;
	static int LastCurrLevel = -1;
	static bool LastSetLevel = false;
	static _setlevels LastSetLevelNum = SL_NONE;
	static std::array<int8_t, MaxMonsters> LastAnnouncedBucket {};

	if (MyPlayer == nullptr)
		return;
	if (leveltype == DTYPE_TOWN)
		return;

	const bool levelChanged = LastLevelType != leveltype || LastCurrLevel != currlevel || LastSetLevel != setlevel || LastSetLevelNum != setlvlnum;
	if (levelChanged) {
		LastAnnouncedBucket.fill(-1);
		LastLevelType = leveltype;
		LastCurrLevel = currlevel;
		LastSetLevel = setlevel;
		LastSetLevelNum = setlvlnum;
	}

	for (size_t monsterId = 0; monsterId < MaxMonsters; ++monsterId) {
		if (LastAnnouncedBucket[monsterId] < 0)
			continue;

		const Monster &monster = Monsters[monsterId];
		if (monster.isInvalid || monster.hitPoints <= 0 || !IsBossMonsterForHpAnnouncement(monster))
			LastAnnouncedBucket[monsterId] = -1;
	}

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int monsterId = static_cast<int>(ActiveMonsters[i]);
		const Monster &monster = Monsters[monsterId];

		if (monster.isInvalid)
			continue;
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			continue;
		if (!IsBossMonsterForHpAnnouncement(monster))
			continue;
		if (monster.hitPoints <= 0 || monster.maxHitPoints <= 0)
			continue;

		const int64_t hp = std::clamp<int64_t>(monster.hitPoints, 0, monster.maxHitPoints);
		const int64_t maxHp = monster.maxHitPoints;
		const int hpPercent = static_cast<int>(std::clamp<int64_t>(hp * 100 / maxHp, 0, 100));
		const int bucket = ((hpPercent + 9) / 10) * 10;

		int8_t &lastBucket = LastAnnouncedBucket[monsterId];
		if (lastBucket < 0) {
			lastBucket = static_cast<int8_t>(((hpPercent + 9) / 10) * 10);
			continue;
		}

		if (bucket >= lastBucket)
			continue;

		lastBucket = static_cast<int8_t>(bucket);
		SpeakText(fmt::format(fmt::runtime(_("{:s} health: {:d}%")), monster.name(), bucket), /*force=*/false);
	}
}

void UpdateAttackableMonsterAnnouncements()
{
	static std::optional<int> LastAttackableMonsterId;

	if (MyPlayer == nullptr) {
		LastAttackableMonsterId = std::nullopt;
		return;
	}
	if (leveltype == DTYPE_TOWN) {
		LastAttackableMonsterId = std::nullopt;
		return;
	}
	if (MyPlayerIsDead || MyPlayer->_pmode == PM_DEATH || MyPlayer->hasNoLife()) {
		LastAttackableMonsterId = std::nullopt;
		return;
	}
	if (InGameMenu() || invflag) {
		LastAttackableMonsterId = std::nullopt;
		return;
	}

	const Player &player = *MyPlayer;
	const Point playerPosition = player.position.tile;

	int bestRotations = 5;
	std::optional<int> bestId;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const int monsterId = static_cast<int>(ActiveMonsters[i]);
		const Monster &monster = Monsters[monsterId];

		if (monster.isInvalid)
			continue;
		if ((monster.flags & MFLAG_HIDDEN) != 0)
			continue;
		if (monster.hitPoints <= 0)
			continue;
		if (monster.isPlayerMinion())
			continue;
		if (!monster.isPossibleToHit())
			continue;

		const Point monsterPosition = monster.position.tile;
		if (playerPosition.WalkingDistance(monsterPosition) > 1)
			continue;

		const int d1 = static_cast<int>(player._pdir);
		const int d2 = static_cast<int>(GetDirection(playerPosition, monsterPosition));

		int rotations = std::abs(d1 - d2);
		if (rotations > 4)
			rotations = 4 - (rotations % 4);

		if (!bestId || rotations < bestRotations || (rotations == bestRotations && monsterId < *bestId)) {
			bestRotations = rotations;
			bestId = monsterId;
		}
	}

	if (!bestId) {
		LastAttackableMonsterId = std::nullopt;
		return;
	}

	if (LastAttackableMonsterId && *LastAttackableMonsterId == *bestId)
		return;

	LastAttackableMonsterId = *bestId;

	const StringOrView label = MonsterLabelForSpeech(Monsters[*bestId]);
	if (!label.empty())
		SpeakText(label.str(), /*force=*/true);
}

StringOrView MonsterLabelForSpeech(const Monster &monster)
{
	const std::string_view name = monster.name();
	if (name.empty())
		return name;

	std::string_view type;
	switch (monster.data().monsterClass) {
	case MonsterClass::Animal:
		type = _("Animal");
		break;
	case MonsterClass::Demon:
		type = _("Demon");
		break;
	case MonsterClass::Undead:
		type = _("Undead");
		break;
	}

	if (type.empty())
		return name;
	return StrCat(name, ", ", type);
}

StringOrView DoorLabelForSpeech(const Object &door)
{
	if (!door.isDoor())
		return door.name();

	// Catacombs doors are grates, so differentiate them for the screen reader / tracker.
	if (IsAnyOf(door._otype, _object_id::OBJ_L2LDOOR, _object_id::OBJ_L2RDOOR)) {
		if (door._oVar4 == DOOR_OPEN)
			return _("Open Grate Door");
		if (door._oVar4 == DOOR_CLOSED)
			return _("Closed Grate Door");
		if (door._oVar4 == DOOR_BLOCKED)
			return _("Blocked Grate Door");
		return _("Grate Door");
	}

	return door.name();
}

void UpdateInteractableDoorAnnouncements()
{
	static std::optional<int> LastInteractableDoorId;
	static std::optional<int> LastInteractableDoorState;

	if (MyPlayer == nullptr) {
		LastInteractableDoorId = std::nullopt;
		LastInteractableDoorState = std::nullopt;
		return;
	}
	if (leveltype == DTYPE_TOWN) {
		LastInteractableDoorId = std::nullopt;
		LastInteractableDoorState = std::nullopt;
		return;
	}
	if (MyPlayerIsDead || MyPlayer->_pmode == PM_DEATH || MyPlayer->hasNoLife()) {
		LastInteractableDoorId = std::nullopt;
		LastInteractableDoorState = std::nullopt;
		return;
	}
	if (InGameMenu() || invflag) {
		LastInteractableDoorId = std::nullopt;
		LastInteractableDoorState = std::nullopt;
		return;
	}

	const Player &player = *MyPlayer;
	const Point playerPosition = player.position.tile;

	std::optional<int> bestId;
	int bestRotations = 5;
	int bestDistance = 0;

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			if (dx == 0 && dy == 0)
				continue;

			const Point pos = playerPosition + Displacement { dx, dy };
			if (!InDungeonBounds(pos))
				continue;

			const int objectId = std::abs(dObject[pos.x][pos.y]) - 1;
			if (objectId < 0 || objectId >= MAXOBJECTS)
				continue;

			const Object &door = Objects[objectId];
			if (!door.isDoor() || !door.canInteractWith())
				continue;

			const int distance = playerPosition.WalkingDistance(door.position);
			if (distance > 1)
				continue;

			const int d1 = static_cast<int>(player._pdir);
			const int d2 = static_cast<int>(GetDirection(playerPosition, door.position));

			int rotations = std::abs(d1 - d2);
			if (rotations > 4)
				rotations = 4 - (rotations % 4);

			if (!bestId || rotations < bestRotations || (rotations == bestRotations && distance < bestDistance)
			    || (rotations == bestRotations && distance == bestDistance && objectId < *bestId)) {
				bestRotations = rotations;
				bestDistance = distance;
				bestId = objectId;
			}
		}
	}

	if (!bestId) {
		LastInteractableDoorId = std::nullopt;
		LastInteractableDoorState = std::nullopt;
		return;
	}

	const Object &door = Objects[*bestId];
	const int state = door._oVar4;
	if (LastInteractableDoorId && LastInteractableDoorState && *LastInteractableDoorId == *bestId && *LastInteractableDoorState == state)
		return;

	LastInteractableDoorId = *bestId;
	LastInteractableDoorState = state;

	const StringOrView label = DoorLabelForSpeech(door);
	if (!label.empty())
		SpeakText(label.str(), /*force=*/true);
}

} // namespace devilution
