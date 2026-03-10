/**
 * @file effects.cpp
 *
 * Implementation of functions for loading and playing sounds.
 */
#include "effects.h"

#include <array>
#include <cstdint>
#include <string_view>

#include <expected.hpp>
#include <magic_enum/magic_enum.hpp>
#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "data/file.hpp"
#include "data/iterators.hpp"
#include "data/record_reader.hpp"
#include "engine/random.hpp"
#include "engine/sound.h"
#include "engine/sound_defs.hpp"
#include "engine/sound_position.hpp"
#include "game_mode.hpp"
#include "options.h"
#include "player.h"
#include "utils/is_of.hpp"

namespace devilution {

int sfxdelay;
SfxID sfxdnum = SfxID::None;

namespace {

#ifndef DISABLE_STREAMING_SOUNDS
constexpr bool AllowStreaming = true;
#else
constexpr bool AllowStreaming = false;
#endif

/** Specifies the sound file and the playback state of the current sound effect. */
TSFX *sgpStreamSFX = nullptr;

/** List of all sounds, except monsters and music */
std::vector<TSFX> sgSFX;

#ifdef __DREAMCAST__
constexpr uint32_t DreamcastMissingLoadRetryMs = 2000;
constexpr uint32_t DreamcastDeferredLoadRetryMs = 250;
constexpr uint32_t DreamcastLateLoadThresholdMs = 20;
constexpr uint32_t DreamcastRealtimeLoadIntervalMs = 500;

std::array<uint32_t, static_cast<size_t>(SfxID::LAST) + 1> SfxLoadRetryAfterMs {};
uint32_t NextDreamcastRealtimeLoadAtMs = 0;

size_t GetSfxIndex(const TSFX *sfx)
{
	return static_cast<size_t>(sfx - sgSFX.data());
}

bool ShouldAttemptSfxLoadNow(const TSFX *sfx)
{
	const size_t index = GetSfxIndex(sfx);
	if (index >= SfxLoadRetryAfterMs.size())
		return true;
	return SDL_GetTicks() >= SfxLoadRetryAfterMs[index];
}

void DeferSfxLoad(const TSFX *sfx, uint32_t delayMs)
{
	const size_t index = GetSfxIndex(sfx);
	if (index >= SfxLoadRetryAfterMs.size())
		return;
	SfxLoadRetryAfterMs[index] = SDL_GetTicks() + delayMs;
}

bool TryLoadSfxForPlayback(TSFX *sfx, bool stream, bool allowBlockingLoad, bool *loadedLate)
{
	if (loadedLate != nullptr)
		*loadedLate = false;
	if (sfx->pSnd != nullptr)
		return true;
	if (!ShouldAttemptSfxLoadNow(sfx))
		return false;
	if (!allowBlockingLoad) {
		DeferSfxLoad(sfx, DreamcastDeferredLoadRetryMs);
		return false;
	}

	const uint32_t startedAt = SDL_GetTicks();
	sfx->pSnd = sound_file_load(sfx->pszName.c_str(), stream);
	if (sfx->pSnd == nullptr) {
		DeferSfxLoad(sfx, DreamcastMissingLoadRetryMs);
		return false;
	}

	if (loadedLate != nullptr && SDL_GetTicks() - startedAt > DreamcastLateLoadThresholdMs)
		*loadedLate = true;
	return true;
}

void PreloadDreamcastSfx(SfxID id)
{
	const size_t index = static_cast<size_t>(id);
	if (index >= sgSFX.size())
		return;

	TSFX &sfx = sgSFX[index];
	if (sfx.pSnd != nullptr || (sfx.bFlags & sfx_STREAM) != 0)
		return;
	if (!ShouldAttemptSfxLoadNow(&sfx))
		return;

	sfx.pSnd = sound_file_load(sfx.pszName.c_str(), /*stream=*/false);
	if (sfx.pSnd == nullptr)
		DeferSfxLoad(&sfx, DreamcastMissingLoadRetryMs);
}

/**
 * Evict non-playing sounds to free memory for new sound loading.
 * @param exclude Sound to skip during eviction (the one being loaded)
 * @param streamOnly If true, only count/evict sounds with sfx_STREAM flag
 * @param maxLoaded If loaded count reaches this, start evicting
 * @param targetLoaded Evict until loaded count drops below this
 */
void EvictSoundsIfNeeded(TSFX *exclude, bool streamOnly, int maxLoaded, int targetLoaded)
{
	int loaded = 0;
	for (const auto &sfx : sgSFX) {
		if (sfx.pSnd != nullptr && (!streamOnly || (sfx.bFlags & sfx_STREAM) != 0))
			++loaded;
	}
	if (loaded < maxLoaded)
		return;
	for (auto &sfx : sgSFX) {
		if (&sfx == exclude)
			continue;
		if (sfx.pSnd == nullptr || sfx.pSnd->isPlaying())
			continue;
		if (streamOnly && (sfx.bFlags & sfx_STREAM) == 0)
			continue;
		sfx.pSnd = nullptr;
		--loaded;
		if (loaded < targetLoaded)
			break;
	}
}
#endif

void StreamPlay(TSFX *pSFX, int lVolume, int lPan)
{
	assert(pSFX);
	assert(pSFX->bFlags & sfx_STREAM);
	stream_stop();

	if (lVolume >= VOLUME_MIN) {
		if (lVolume > VOLUME_MAX)
			lVolume = VOLUME_MAX;
#ifdef __DREAMCAST__
		if (pSFX->pSnd == nullptr) {
			music_mute();
			EvictSoundsIfNeeded(pSFX, /*streamOnly=*/true, /*maxLoaded=*/8, /*targetLoaded=*/4);
			bool loadedLate = false;
			const bool loaded = TryLoadSfxForPlayback(pSFX, AllowStreaming, /*allowBlockingLoad=*/true, &loadedLate);
			music_unmute();
			if (!loaded || loadedLate)
				return;
		}
		if (pSFX->pSnd != nullptr && pSFX->pSnd->DSB.IsLoaded())
			pSFX->pSnd->DSB.PlayWithVolumeAndPan(lVolume, sound_get_or_set_sound_volume(1), lPan);
#else
		if (pSFX->pSnd == nullptr)
			pSFX->pSnd = sound_file_load(pSFX->pszName.c_str(), AllowStreaming);
		if (pSFX->pSnd->DSB.IsLoaded())
			pSFX->pSnd->DSB.PlayWithVolumeAndPan(lVolume, sound_get_or_set_sound_volume(1), lPan);
#endif
		sgpStreamSFX = pSFX;
	}
}

void StreamUpdate()
{
	if (sgpStreamSFX != nullptr && !sgpStreamSFX->pSnd->isPlaying()) {
		stream_stop();
	}
}

void PlaySfxPriv(TSFX *pSFX, bool loc, Point position)
{
	if (MyPlayer->pLvlLoad != 0 && gbIsMultiplayer) {
		return;
	}
	if (!gbSndInited || !gbSoundOn || gbBufferMsgs != 0) {
		return;
	}

	if ((pSFX->bFlags & (sfx_STREAM | sfx_MISC)) == 0 && pSFX->pSnd != nullptr && pSFX->pSnd->isPlaying()) {
		return;
	}

	int lVolume = 0;
	int lPan = 0;
	if (loc && !CalculateSoundPosition(position, &lVolume, &lPan)) {
		return;
	}

	if ((pSFX->bFlags & sfx_STREAM) != 0) {
		StreamPlay(pSFX, lVolume, lPan);
		return;
	}

#ifdef __DREAMCAST__
	if (pSFX->pSnd == nullptr) {
		music_mute();
		EvictSoundsIfNeeded(pSFX, /*streamOnly=*/false, /*maxLoaded=*/20, /*targetLoaded=*/15);
		bool loadedLate = false;
		const uint32_t now = SDL_GetTicks();
		const bool canDoRealtimeLoad = !loc || now >= NextDreamcastRealtimeLoadAtMs;
		bool loaded = TryLoadSfxForPlayback(pSFX, /*stream=*/false, /*allowBlockingLoad=*/canDoRealtimeLoad, &loadedLate);
		if (loc && canDoRealtimeLoad)
			NextDreamcastRealtimeLoadAtMs = SDL_GetTicks() + DreamcastRealtimeLoadIntervalMs;
		// For non-positional (menu/UI) sounds, one eviction+retry is acceptable.
		if (!loaded && !loc) {
			EvictSoundsIfNeeded(nullptr, /*streamOnly=*/false, /*maxLoaded=*/0, /*targetLoaded=*/0);
			ClearDuplicateSounds();
			DeferSfxLoad(pSFX, 0);
			loaded = TryLoadSfxForPlayback(pSFX, /*stream=*/false, /*allowBlockingLoad=*/true, &loadedLate);
		}
		music_unmute();
		if (!loaded || loadedLate)
			return;
	}
#else
	if (pSFX->pSnd == nullptr)
		pSFX->pSnd = sound_file_load(pSFX->pszName.c_str());
#endif

	if (pSFX->pSnd == nullptr || !pSFX->pSnd->DSB.IsLoaded())
		return;

	const auto id = static_cast<SfxID>(pSFX - sgSFX.data());
	const bool useCuesVolume = (id >= SfxID::AccessibilityWeapon && id <= SfxID::AccessibilityInteract);
	const int userVolume = useCuesVolume ? *GetOptions().Audio.audioCuesVolume : *GetOptions().Audio.soundVolume;
	snd_play_snd(pSFX->pSnd.get(), lVolume, lPan, userVolume);
}

SfxID RndSFX(SfxID psfx)
{
	switch (psfx) {
	case SfxID::Warrior69:
	case SfxID::Sorceror69:
	case SfxID::Rogue69:
	case SfxID::Monk69:
	case SfxID::Swing:
	case SfxID::SpellAcid:
	case SfxID::OperateShrine:
		return PickRandomlyAmong({ psfx, static_cast<SfxID>(static_cast<int16_t>(psfx) + 1) });
	case SfxID::Warrior14:
	case SfxID::Warrior15:
	case SfxID::Warrior16:
	case SfxID::Warrior2:
	case SfxID::Rogue14:
	case SfxID::Sorceror14:
	case SfxID::Monk14:
		return PickRandomlyAmong({ psfx, static_cast<SfxID>(static_cast<int16_t>(psfx) + 1), static_cast<SfxID>(static_cast<int16_t>(psfx) + 2) });
	default:
		return psfx;
	}
}

tl::expected<sfx_flag, std::string> ParseSfxFlag(std::string_view value)
{
	if (value == "Stream") return sfx_STREAM;
	if (value == "Misc") return sfx_MISC;
	if (value == "Ui") return sfx_UI;
	if (value == "Monk") return sfx_MONK;
	if (value == "Rogue") return sfx_ROGUE;
	if (value == "Warrior") return sfx_WARRIOR;
	if (value == "Sorcerer") return sfx_SORCERER;
	return tl::make_unexpected("Unknown enum value");
}

void LoadEffectsData()
{
	const std::string_view filename = "txtdata\\sound\\effects.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	dataFile.skipHeaderOrDie(filename);

	sgSFX.clear();
	sgSFX.reserve(dataFile.numRecords());
	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		TSFX &item = sgSFX.emplace_back();
		reader.advance(); // Skip the first column (effect ID).
		reader.readEnumList("flags", item.bFlags, ParseSfxFlag);
		reader.readString("path", item.pszName);
	}
	sgSFX.shrink_to_fit();
#ifdef __DREAMCAST__
	SfxLoadRetryAfterMs.fill(0);
#endif
}

void PrivSoundInit(uint8_t bLoadMask)
{
	if (!gbSndInited) {
		return;
	}

	if (sgSFX.empty()) LoadEffectsData();

#ifdef __DREAMCAST__
	// Free non-playing sounds during level transitions to reclaim RAM.
	for (auto &sfx : sgSFX) {
		if (sfx.pSnd != nullptr && !sfx.pSnd->isPlaying()) {
			sfx.pSnd = nullptr;
		}
	}
	// Keep high-frequency sounds resident to avoid CD reads in combat.
	for (const SfxID id : { SfxID::Walk, SfxID::Swing, SfxID::Swing2, SfxID::ShootBow, SfxID::CastSpell,
	         SfxID::CastFire, SfxID::SpellFireHit, SfxID::ItemPotion, SfxID::ItemGold, SfxID::GrabItem,
	         SfxID::DoorOpen, SfxID::DoorClose, SfxID::ChestOpen, SfxID::MenuMove, SfxID::MenuSelect }) {
		PreloadDreamcastSfx(id);
	}
	(void)bLoadMask;
	return;
#endif

	for (auto &sfx : sgSFX) {
		if (sfx.bFlags == 0 || sfx.pSnd != nullptr) {
			continue;
		}

		if ((sfx.bFlags & sfx_STREAM) != 0) {
			continue;
		}

		if ((sfx.bFlags & bLoadMask) == 0) {
			continue;
		}

		sfx.pSnd = sound_file_load(sfx.pszName.c_str());
	}
}

} // namespace

bool effect_is_playing(SfxID nSFX)
{
	if (!gbSndInited) return false;

	TSFX *sfx = &sgSFX[static_cast<int16_t>(nSFX)];
	if (sfx->pSnd != nullptr)
		return sfx->pSnd->isPlaying();

	if ((sfx->bFlags & sfx_STREAM) != 0)
		return sfx == sgpStreamSFX;

	return false;
}

void stream_stop()
{
	if (sgpStreamSFX != nullptr) {
		sgpStreamSFX->pSnd = nullptr;
		sgpStreamSFX = nullptr;
	}
}

void PlaySFX(SfxID psfx)
{
	psfx = RndSFX(psfx);

	if (!gbSndInited) return;

	PlaySfxPriv(&sgSFX[static_cast<int16_t>(psfx)], false, { 0, 0 });
}

void PlaySfxLoc(SfxID psfx, Point position, bool randomizeByCategory)
{
	if (randomizeByCategory) {
		psfx = RndSFX(psfx);
	}

	if (!gbSndInited) return;

	if (IsAnyOf(psfx, SfxID::Walk, SfxID::ShootBow, SfxID::CastSpell, SfxID::Swing)) {
		TSnd *pSnd = sgSFX[static_cast<int16_t>(psfx)].pSnd.get();
		if (pSnd != nullptr)
			pSnd->start_tc = 0;
	}

	PlaySfxPriv(&sgSFX[static_cast<int16_t>(psfx)], true, position);
}

void sound_stop()
{
	if (!gbSndInited)
		return;
	ClearDuplicateSounds();
	for (auto &sfx : sgSFX) {
		if (sfx.pSnd != nullptr && sfx.pSnd->DSB.IsLoaded()) {
			sfx.pSnd->DSB.Stop();
		}
	}
}

void sound_update()
{
	if (!gbSndInited) {
		return;
	}

	StreamUpdate();
}

void effects_cleanup_sfx(bool fullUnload)
{
	sound_stop();

	if (fullUnload) {
		sgSFX.clear();
#ifdef __DREAMCAST__
		SfxLoadRetryAfterMs.fill(0);
#endif
		return;
	}

	for (auto &sfx : sgSFX)
		sfx.pSnd = nullptr;
}

void sound_init()
{
	uint8_t mask = sfx_MISC;
	if (gbIsMultiplayer) {
		mask |= (sfx_WARRIOR | sfx_MONK);
		if (!gbIsSpawn)
			mask |= (sfx_ROGUE | sfx_SORCERER);
	} else {
		switch (MyPlayer->_pClass) {
		case HeroClass::Warrior:
		case HeroClass::Barbarian:
			mask |= sfx_WARRIOR;
			break;
		case HeroClass::Rogue:
		case HeroClass::Bard:
			mask |= sfx_ROGUE;
			break;
		case HeroClass::Sorcerer:
			mask |= sfx_SORCERER;
			break;
		case HeroClass::Monk:
			mask |= sfx_MONK;
			break;
		default:
			if (static_cast<size_t>(MyPlayer->_pClass) < GetNumPlayerClasses()) {
				// this is a custom class, so we need to add init sounds, since we can't determine which ones will be used by it
				mask |= (sfx_WARRIOR | sfx_MONK);
				if (!gbIsSpawn)
					mask |= (sfx_ROGUE | sfx_SORCERER);
			} else {
				app_fatal("effects:1");
			}
		}
	}

	PrivSoundInit(mask);
}

void ui_sound_init()
{
	PrivSoundInit(sfx_UI);
}

void effects_play_sound(SfxID id)
{
	if (!gbSndInited || !gbSoundOn) {
		return;
	}

	TSFX &sfx = sgSFX[static_cast<int16_t>(id)];
	if (sfx.pSnd != nullptr && !sfx.pSnd->isPlaying()) {
		snd_play_snd(sfx.pSnd.get(), 0, 0, *GetOptions().Audio.soundVolume);
	}
}

int GetSFXLength(SfxID nSFX)
{
	TSFX &sfx = sgSFX[static_cast<int16_t>(nSFX)];
	if (sfx.pSnd == nullptr) {
#ifdef __DREAMCAST__
		music_mute();
#endif
		sfx.pSnd = sound_file_load(sfx.pszName.c_str(),
		    /*stream=*/AllowStreaming && (sfx.bFlags & sfx_STREAM) != 0);
#ifdef __DREAMCAST__
		music_unmute();
#endif
	}
	if (sfx.pSnd == nullptr)
		return 0;
	return sfx.pSnd->DSB.GetLength();
}

tl::expected<HeroSpeech, std::string> ParseHeroSpeech(std::string_view value)
{
	const std::optional<HeroSpeech> enumValueOpt = magic_enum::enum_cast<HeroSpeech>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	return tl::make_unexpected("Unknown enum value.");
}

tl::expected<SfxID, std::string> ParseSfxId(std::string_view value)
{
	const std::optional<SfxID> enumValueOpt = magic_enum::enum_cast<SfxID>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	return tl::make_unexpected("Unknown enum value.");
}

} // namespace devilution
