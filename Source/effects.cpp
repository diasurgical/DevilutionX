/**
 * @file effects.cpp
 *
 * Implementation of functions for loading and playing sounds.
 */
#include "effects.h"

#include <cstdint>
#include <string_view>

#include <expected.hpp>
#include <magic_enum/magic_enum.hpp>

#include "data/file.hpp"
#include "data/iterators.hpp"
#include "data/record_reader.hpp"
#include "engine/random.hpp"
#include "engine/sound.h"
#include "engine/sound_defs.hpp"
#include "engine/sound_position.hpp"
#include "game_mode.hpp"
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

void StreamPlay(TSFX *pSFX, int lVolume, int lPan)
{
	assert(pSFX);
	assert(pSFX->bFlags & sfx_STREAM);
	stream_stop();

	if (lVolume >= VOLUME_MIN) {
		if (lVolume > VOLUME_MAX)
			lVolume = VOLUME_MAX;
		if (pSFX->pSnd == nullptr)
			pSFX->pSnd = sound_file_load(pSFX->pszName.c_str(), AllowStreaming);
		if (pSFX->pSnd->DSB.IsLoaded())
			pSFX->pSnd->DSB.PlayWithVolumeAndPan(lVolume, sound_get_or_set_sound_volume(1), lPan);
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

	if (pSFX->pSnd == nullptr)
		pSFX->pSnd = sound_file_load(pSFX->pszName.c_str());

	if (pSFX->pSnd != nullptr && pSFX->pSnd->DSB.IsLoaded())
		snd_play_snd(pSFX->pSnd.get(), lVolume, lPan);
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
}

void PrivSoundInit(uint8_t bLoadMask)
{
	if (!gbSndInited) {
		return;
	}

	if (sgSFX.empty()) LoadEffectsData();

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

void effects_cleanup_sfx()
{
	sound_stop();

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
		snd_play_snd(sfx.pSnd.get(), 0, 0);
	}
}

int GetSFXLength(SfxID nSFX)
{
	TSFX &sfx = sgSFX[static_cast<int16_t>(nSFX)];
	if (sfx.pSnd == nullptr)
		sfx.pSnd = sound_file_load(sfx.pszName.c_str(),
		    /*stream=*/AllowStreaming && (sfx.bFlags & sfx_STREAM) != 0);
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
