/**
 * @file sound.h
 *
 * Interface of functions setting up the audio pipeline.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <expected.hpp>

#include "levels/gendung.h"
#include "utils/attributes.h"

#ifndef NOSOUND
#ifdef USE_SDL3
#include <SDL3/SDL_audio.h>
#endif

#include "utils/soundsample.h"
#endif

namespace devilution {

enum _music_id : uint8_t {
	TMUSIC_TOWN,
	TMUSIC_CATHEDRAL,
	TMUSIC_CATACOMBS,
	TMUSIC_CAVES,
	TMUSIC_HELL,
	TMUSIC_NEST,
	TMUSIC_CRYPT,
	TMUSIC_INTRO,
	NUM_MUSIC,
};

struct TSnd {
	uint32_t start_tc;

#ifndef NOSOUND
	SoundSample DSB;

	bool isPlaying()
	{
		return DSB.IsPlaying();
	}
#else
	bool isPlaying()
	{
		return false;
	}
#endif

	~TSnd();
};

extern bool gbSndInited;
#ifdef USE_SDL3
extern SDL_AudioDeviceID CurrentAudioDeviceId;
#endif

extern _music_id sgnMusicTrack;

void ClearDuplicateSounds();
void snd_play_snd(TSnd *pSnd, int lVolume, int lPan);
std::unique_ptr<TSnd> sound_file_load(const char *path, bool stream = false);
tl::expected<std::unique_ptr<TSnd>, std::string> SoundFileLoadWithStatus(const char *path, bool stream = false);
void snd_init();
void snd_deinit();
_music_id GetLevelMusic(dungeon_type dungeonType);
void music_stop();
void music_start(_music_id nTrack);
void sound_disable_music(bool disable);
int sound_get_or_set_music_volume(int volume);
int sound_get_or_set_sound_volume(int volume);
void music_mute();
void music_unmute();

/* data */

extern DVL_API_FOR_TEST bool gbMusicOn;
extern DVL_API_FOR_TEST bool gbSoundOn;

} // namespace devilution
