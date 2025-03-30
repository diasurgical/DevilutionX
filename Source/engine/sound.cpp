/**
 * @file sound.cpp
 *
 * Implementation of functions setting up the audio pipeline.
 */
#include "engine/sound.h"

#include <algorithm>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include <Aulib/Stream.h>
#include <SDL.h>
#include <expected.hpp>

#include "engine/assets.hpp"
#include "options.h"
#include "utils/log.hpp"
#include "utils/math.h"
#include "utils/sdl_mutex.h"
#include "utils/status_macros.hpp"
#include "utils/stdcompat/shared_ptr_array.hpp"
#include "utils/str_cat.hpp"
#include "utils/stubs.h"

namespace devilution {

bool gbSndInited;
/** The active background music track id. */
_music_id sgnMusicTrack = NUM_MUSIC;

bool gbMusicOn = true;
/** Specifies whether sound effects are enabled. */
bool gbSoundOn = true;

namespace {

SoundSample music;

std::string GetMp3Path(const char *path)
{
	std::string mp3Path = path;
	const std::string::size_type dot = mp3Path.find_last_of('.');
	mp3Path.replace(dot + 1, mp3Path.size() - (dot + 1), "mp3");
	return mp3Path;
}

tl::expected<void, std::string> LoadAudioFile(const char *path, bool stream, SoundSample &result)
{
	bool isMp3 = true;
	std::string foundPath = GetMp3Path(path);
	AssetRef ref = FindAsset(foundPath.c_str());
	if (!ref.ok()) {
		ref = FindAsset(path);
		foundPath = path;
		isMp3 = false;
	}
	if (!ref.ok()) {
		return tl::make_unexpected(StrCat("Audio file not found\n", path, "\n", SDL_GetError(), "\n" __FILE__ ":", __LINE__));
	}

#ifdef STREAM_ALL_AUDIO_MIN_FILE_SIZE
#if STREAM_ALL_AUDIO_MIN_FILE_SIZE == 0
	stream = true;
#else
	size_t size;
	if (!stream) {
		size = ref.size();
		stream = size >= STREAM_ALL_AUDIO_MIN_FILE_SIZE;
	}
#endif
#endif

	if (stream) {
		if (result.SetChunkStream(foundPath, isMp3, /*logErrors=*/true) != 0) {
			return tl::make_unexpected(StrCat("Failed to load audio file\n", foundPath, "\n", SDL_GetError(), "\n" __FILE__ ":", __LINE__));
		}
	} else {
#if !defined(STREAM_ALL_AUDIO_MIN_FILE_SIZE) || STREAM_ALL_AUDIO_MIN_FILE_SIZE == 0
		const size_t size = ref.size();
#endif
		AssetHandle handle = OpenAsset(std::move(ref));
		if (!handle.ok()) {
			return tl::make_unexpected(StrCat("Failed to load audio file\n", foundPath, "\n", SDL_GetError(), "\n" __FILE__ ":", __LINE__));
		}
		auto waveFile = MakeArraySharedPtr<std::uint8_t>(size);
		if (!handle.read(waveFile.get(), size)) {
			return tl::make_unexpected(StrCat("Failed to read file\n", foundPath, ": ", SDL_GetError(), __FILE__ ":", __LINE__));
		}
		const int error = result.SetChunk(waveFile, size, isMp3);
		if (error != 0) {
			return tl::make_unexpected(SDL_GetError());
		}
	}
	return {};
}

std::list<std::unique_ptr<SoundSample>> duplicateSounds;
std::optional<SdlMutex> duplicateSoundsMutex;

SoundSample *DuplicateSound(const SoundSample &sound)
{
	auto duplicate = std::make_unique<SoundSample>();
	if (duplicate->DuplicateFrom(sound) != 0)
		return nullptr;
	auto *result = duplicate.get();
	decltype(duplicateSounds.begin()) it;
	{
		const std::lock_guard<SdlMutex> lock(*duplicateSoundsMutex);
		duplicateSounds.push_back(std::move(duplicate));
		it = duplicateSounds.end();
		--it;
	}
	result->SetFinishCallback([it]([[maybe_unused]] Aulib::Stream &stream) {
		const std::lock_guard<SdlMutex> lock(*duplicateSoundsMutex);
		duplicateSounds.erase(it);
	});
	return result;
}

/** Maps from track ID to track name in spawn. */
const char *const SpawnMusicTracks[NUM_MUSIC] = {
	"music\\stowne.wav",
	"music\\slvla.wav",
	"music\\slvla.wav",
	"music\\slvla.wav",
	"music\\slvla.wav",
	"music\\dlvlf.wav",
	"music\\dlvle.wav",
	"music\\sintro.wav",
};
/** Maps from track ID to track name. */
const char *const MusicTracks[NUM_MUSIC] = {
	"music\\dtowne.wav",
	"music\\dlvla.wav",
	"music\\dlvlb.wav",
	"music\\dlvlc.wav",
	"music\\dlvld.wav",
	"music\\dlvlf.wav",
	"music\\dlvle.wav",
	"music\\dintro.wav",
};

int CapVolume(int volume)
{
	return std::clamp(volume, VOLUME_MIN, VOLUME_MAX);
}

void OptionAudioChanged()
{
	effects_cleanup_sfx();
	music_stop();
	snd_deinit();
	snd_init();
	music_start(TMUSIC_INTRO);
	if (gbRunGame)
		sound_init();
	else
		ui_sound_init();
}

const auto OptionChangeSampleRate = (GetOptions().Audio.sampleRate.SetValueChangedCallback(OptionAudioChanged), true);
const auto OptionChangeChannels = (GetOptions().Audio.channels.SetValueChangedCallback(OptionAudioChanged), true);
const auto OptionChangeBufferSize = (GetOptions().Audio.bufferSize.SetValueChangedCallback(OptionAudioChanged), true);
const auto OptionChangeResamplingQuality = (GetOptions().Audio.resamplingQuality.SetValueChangedCallback(OptionAudioChanged), true);
const auto OptionChangeResampler = (GetOptions().Audio.resampler.SetValueChangedCallback(OptionAudioChanged), true);
const auto OptionChangeDevice = (GetOptions().Audio.device.SetValueChangedCallback(OptionAudioChanged), true);

} // namespace

void ClearDuplicateSounds()
{
	const std::lock_guard<SdlMutex> lock(*duplicateSoundsMutex);
	duplicateSounds.clear();
}

void snd_play_snd(TSnd *pSnd, int lVolume, int lPan)
{
	if (pSnd == nullptr || !gbSoundOn) {
		return;
	}

	uint32_t tc = SDL_GetTicks();
	if (tc - pSnd->start_tc < 80) {
		return;
	}

	SoundSample *sound = &pSnd->DSB;
	if (sound->IsPlaying()) {
		sound = DuplicateSound(*sound);
		if (sound == nullptr)
			return;
	}

	sound->PlayWithVolumeAndPan(lVolume, *GetOptions().Audio.soundVolume, lPan);
	pSnd->start_tc = tc;
}

tl::expected<std::unique_ptr<TSnd>, std::string> SoundFileLoadWithStatus(const char *path, bool stream)
{
	auto snd = std::make_unique<TSnd>();
	snd->start_tc = SDL_GetTicks() - 80 - 1;
#ifndef NOSOUND
	RETURN_IF_ERROR(LoadAudioFile(path, stream, snd->DSB));
#endif
	return snd;
}

std::unique_ptr<TSnd> sound_file_load(const char *path, bool stream)
{
	tl::expected<std::unique_ptr<TSnd>, std::string> result = SoundFileLoadWithStatus(path, stream);
	if (!result.has_value()) app_fatal(result.error());
	return std::move(result).value();
}

TSnd::~TSnd()
{
	if (DSB.IsLoaded())
		DSB.Stop();
	DSB.Release();
}

void snd_init()
{
	GetOptions().Audio.soundVolume.SetValue(CapVolume(*GetOptions().Audio.soundVolume));
	gbSoundOn = *GetOptions().Audio.soundVolume > VOLUME_MIN;
	sgbSaveSoundOn = gbSoundOn;

	GetOptions().Audio.musicVolume.SetValue(CapVolume(*GetOptions().Audio.musicVolume));
	gbMusicOn = *GetOptions().Audio.musicVolume > VOLUME_MIN;

	// Initialize the SDL_audiolib library. Set the output sample rate to
	// 22kHz, the audio format to 16-bit signed, use 2 output channels
	// (stereo), and a 2KiB output buffer.
	if (!Aulib::init(*GetOptions().Audio.sampleRate, AUDIO_S16, *GetOptions().Audio.channels, *GetOptions().Audio.bufferSize, *GetOptions().Audio.device)) {
		LogError(LogCategory::Audio, "Failed to initialize audio (Aulib::init): {}", SDL_GetError());
		return;
	}
	LogVerbose(LogCategory::Audio, "Aulib sampleRate={} channels={} frameSize={} format={:#x}",
	    Aulib::sampleRate(), Aulib::channelCount(), Aulib::frameSize(), Aulib::sampleFormat());

	duplicateSoundsMutex.emplace();
	gbSndInited = true;
}

void snd_deinit()
{
	if (gbSndInited) {
		Aulib::quit();
		duplicateSoundsMutex = std::nullopt;
	}

	gbSndInited = false;
}

_music_id GetLevelMusic(dungeon_type dungeonType)
{
	switch (dungeonType) {
	case DTYPE_TOWN:
		return TMUSIC_TOWN;
	case DTYPE_CATHEDRAL:
		return TMUSIC_CATHEDRAL;
	case DTYPE_CATACOMBS:
		return TMUSIC_CATACOMBS;
	case DTYPE_CAVES:
		return TMUSIC_CAVES;
	case DTYPE_HELL:
		return TMUSIC_HELL;
	case DTYPE_NEST:
		return TMUSIC_NEST;
	case DTYPE_CRYPT:
		return TMUSIC_CRYPT;
	default:
		return TMUSIC_INTRO;
	}
}

void music_stop()
{
	music.Release();
	sgnMusicTrack = NUM_MUSIC;
}

void music_start(_music_id nTrack)
{
	const char *trackPath;

	assert(nTrack < NUM_MUSIC);
	music_stop();
	if (!gbMusicOn)
		return;
	if (HaveFullMusic())
		trackPath = MusicTracks[nTrack];
	else
		trackPath = SpawnMusicTracks[nTrack];

#ifdef DISABLE_STREAMING_MUSIC
	const bool stream = false;
#else
	const bool stream = true;
#endif
	if (!LoadAudioFile(trackPath, stream, music).has_value()) {
		music_stop();
		return;
	}

	music.SetVolume(*GetOptions().Audio.musicVolume, VOLUME_MIN, VOLUME_MAX);
	if (!diablo_is_focused())
		music_mute();
	if (!music.Play(/*numIterations=*/0)) {
		LogError(LogCategory::Audio, "Aulib::Stream::play (from music_start): {}", SDL_GetError());
		music_stop();
		return;
	}

	sgnMusicTrack = nTrack;
}

void sound_disable_music(bool disable)
{
	if (disable) {
		music_stop();
	} else if (sgnMusicTrack != NUM_MUSIC) {
		music_start(sgnMusicTrack);
	}
}

int sound_get_or_set_music_volume(int volume)
{
	if (volume == 1)
		return *GetOptions().Audio.musicVolume;

	GetOptions().Audio.musicVolume.SetValue(volume);

	if (music.IsLoaded())
		music.SetVolume(*GetOptions().Audio.musicVolume, VOLUME_MIN, VOLUME_MAX);

	return *GetOptions().Audio.musicVolume;
}

int sound_get_or_set_sound_volume(int volume)
{
	if (volume == 1)
		return *GetOptions().Audio.soundVolume;

	GetOptions().Audio.soundVolume.SetValue(volume);

	return *GetOptions().Audio.soundVolume;
}

void music_mute()
{
	if (music.IsLoaded())
		music.Mute();
}

void music_unmute()
{
	if (music.IsLoaded())
		music.Unmute();
}

} // namespace devilution
