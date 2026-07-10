#include "utils/soundsample.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <utility>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3_mixer/SDL_mixer.h>
#else
#include <Aulib/DecoderDrmp3.h>
#include <Aulib/DecoderDrwav.h>
#include <Aulib/Stream.h>

#include <SDL.h>
#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#else
#include "utils/sdl2_backports.h"
#endif
#include "utils/aulib.hpp"
#endif

#include "engine/assets.hpp"
#ifdef USE_SDL3
#include "engine/sound.h" // for CurrentMixer
#endif
#include "options.h"
#include "utils/log.hpp"
#include "utils/math.h"
#include "utils/stubs.h"

namespace devilution {

namespace {

constexpr float LogBase = 10.0;

/**
 * Scaling factor for attenuating volume.
 * Picked so that a volume change of -10 dB results in half perceived loudness.
 * VolumeScale = -1000 / log(0.5)
 */
constexpr float VolumeScale = 3321.9281F;

/**
 * Min and max volume range, in millibel.
 * -100 dB (muted) to 0 dB (max. loudness).
 */
constexpr float MillibelMin = -10000.F;
constexpr float MillibelMax = 0.F;

/**
 * Stereo separation factor for left/right speaker panning. Lower values increase separation, moving
 * sounds further left/right, while higher values will pull sounds more towards the middle, reducing separation.
 * Current value is tuned to have ~2:1 mix for sounds that happen on the edge of a 640x480 screen.
 */
constexpr float StereoSeparation = 6000.F;

/**
 * @brief Converts log volume passed in into linear volume.
 * @param logVolume Logarithmic volume in the range [logMin..logMax]
 * @param logMin Volume range minimum (usually ATTENUATION_MIN for game sounds and VOLUME_MIN for volume sliders)
 * @param logMax Volume range maximum (usually 0)
 * @return Linear volume in the range [0..1]
 */
float VolumeLogToLinear(int logVolume, int logMin, int logMax)
{
	const auto logScaled = math::Remap(static_cast<float>(logMin), static_cast<float>(logMax), MillibelMin, MillibelMax, static_cast<float>(logVolume));
	return std::pow(LogBase, logScaled / VolumeScale); // linVolume
}

#ifdef USE_SDL3

/**
 * @brief Converts a log pan value to left/right linear gain factors.
 * @param logPan Pan value in the range [PAN_MIN..PAN_MAX]. Negative = left, positive = right.
 * @param[out] leftGain Linear gain for the left channel [0..1]
 * @param[out] rightGain Linear gain for the right channel [0..1]
 */
void PanLogToLeftRight(int logPan, float &leftGain, float &rightGain)
{
	if (logPan == 0) {
		leftGain = 1.0f;
		rightGain = 1.0f;
		return;
	}

	auto factor = std::pow(LogBase, static_cast<float>(-std::abs(logPan)) / StereoSeparation);

	if (logPan < 0) {
		// Sound is to the left: attenuate right channel
		leftGain = 1.0f;
		rightGain = factor;
	} else {
		// Sound is to the right: attenuate left channel
		leftGain = factor;
		rightGain = 1.0f;
	}
}

#else  // !USE_SDL3

float PanLogToLinear(int logPan)
{
	if (logPan == 0)
		return 0;

	auto factor = std::pow(LogBase, static_cast<float>(-std::abs(logPan)) / StereoSeparation);

	return copysign(1.F - factor, static_cast<float>(logPan));
}

std::unique_ptr<Aulib::Decoder> CreateDecoder(bool isMp3)
{
	if (isMp3)
		return std::make_unique<Aulib::DecoderDrmp3>();
	return std::make_unique<Aulib::DecoderDrwav>();
}

std::unique_ptr<Aulib::Stream> CreateStream(SDL_IOStream *handle, bool isMp3)
{
	auto decoder = CreateDecoder(isMp3);
	if (!decoder->open(handle)) // open for `getRate`
		return nullptr;
	auto resampler = CreateAulibResampler(decoder->getRate());
	return std::make_unique<Aulib::Stream>(handle, std::move(decoder), std::move(resampler), /*closeRw=*/true);
}
#endif // USE_SDL3

} // namespace

///// SoundSample /////

SoundSample::SoundSample(SoundSample &&other) noexcept
    : file_data_(std::move(other.file_data_))
    , file_data_size_(other.file_data_size_)
    , file_path_(std::move(other.file_path_))
    , isMp3_(other.isMp3_)
#ifdef USE_SDL3
    , audio_(other.audio_)
    , track_(other.track_)
    , gain_(other.gain_)
    , muteGain_(other.muteGain_)
    , hasStereoGains_(other.hasStereoGains_)
    , leftGain_(other.leftGain_)
    , rightGain_(other.rightGain_)
#else
    , stream_(std::move(other.stream_))
#endif
{
#ifdef USE_SDL3
	other.audio_ = nullptr;
	other.track_ = nullptr;
#endif
	other.file_data_size_ = 0;
}

SoundSample &SoundSample::operator=(SoundSample &&other) noexcept
{
	if (this != &other) {
		Release();
		file_data_ = std::move(other.file_data_);
		file_data_size_ = other.file_data_size_;
		file_path_ = std::move(other.file_path_);
		isMp3_ = other.isMp3_;
#ifdef USE_SDL3
		audio_ = other.audio_;
		track_ = other.track_;
		gain_ = other.gain_;
		muteGain_ = other.muteGain_;
		hasStereoGains_ = other.hasStereoGains_;
		leftGain_ = other.leftGain_;
		rightGain_ = other.rightGain_;
		other.audio_ = nullptr;
		other.track_ = nullptr;
#else
		stream_ = std::move(other.stream_);
#endif
		other.file_data_size_ = 0;
	}
	return *this;
}

SoundSample::~SoundSample()
{
	Release();
}

#ifndef USE_SDL3
void SoundSample::SetFinishCallback(Aulib::Stream::Callback &&callback)
{
	stream_->setFinishCallback(std::forward<Aulib::Stream::Callback>(callback));
}
#endif

void SoundSample::Stop()
{
#ifdef USE_SDL3
	if (track_ != nullptr) {
		MIX_StopTrack(track_, 0);
	}
#else
	stream_->stop();
#endif
}

void SoundSample::Mute()
{
#ifdef USE_SDL3
	muteGain_ = 0.0f;
	if (track_ != nullptr) {
		MIX_SetTrackGain(track_, 0.0f);
	}
#else
	stream_->mute();
#endif
}

void SoundSample::Unmute()
{
#ifdef USE_SDL3
	muteGain_ = 1.0f;
	if (track_ != nullptr) {
		MIX_SetTrackGain(track_, gain_);
	}
#else
	stream_->unmute();
#endif
}

void SoundSample::Release()
{
#ifdef USE_SDL3
	if (track_ != nullptr) {
		MIX_DestroyTrack(track_);
		track_ = nullptr;
	}
	if (audio_ != nullptr) {
		MIX_DestroyAudio(audio_);
		audio_ = nullptr;
	}
#else
	stream_ = nullptr;
#endif
	file_data_ = nullptr;
	file_data_size_ = 0;
}

/**
 * @brief Check if a the sound is being played atm
 */
bool SoundSample::IsPlaying()
{
#ifdef USE_SDL3
	if (track_ == nullptr) return false;
	return MIX_TrackPlaying(track_);
#else
	return stream_ && stream_->isPlaying();
#endif
}

bool SoundSample::Play(int numIterations)
{
#ifdef USE_SDL3
	if (audio_ == nullptr || CurrentMixer == nullptr) return false;

	// Create a track on demand if we don't have one yet.
	if (track_ == nullptr) {
		track_ = MIX_CreateTrack(CurrentMixer);
		if (track_ == nullptr) {
			LogError(LogCategory::Audio, "MIX_CreateTrack failed: {}", SDL_GetError());
			return false;
		}
	}

	if (!MIX_SetTrackAudio(track_, audio_)) {
		LogError(LogCategory::Audio, "MIX_SetTrackAudio failed: {}", SDL_GetError());
		return false;
	}

	MIX_SetTrackGain(track_, gain_ * muteGain_);

	// Apply deferred stereo position if it was set before the track existed.
	if (hasStereoGains_) {
		MIX_StereoGains gains;
		gains.left = leftGain_;
		gains.right = rightGain_;
		MIX_SetTrackStereo(track_, &gains);
	}

	// numIterations: 1 = play once, 0 = loop forever.
	// MIX_PROP_PLAY_LOOPS_NUMBER: 0 = don't loop, -1 = loop forever, N = loop N times.
	SDL_PropertiesID props = SDL_CreateProperties();
	if (numIterations == 0) {
		SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
	} else if (numIterations > 1) {
		SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, numIterations - 1);
	}
	// numIterations == 1 means play once with no looping, which is the default (0 loops).

	bool result = MIX_PlayTrack(track_, props);
	SDL_DestroyProperties(props);

	if (!result) {
		LogError(LogCategory::Audio, "MIX_PlayTrack failed: {}", SDL_GetError());
		return false;
	}
	return true;
#else
	if (!stream_->play(numIterations)) {
		LogError(LogCategory::Audio, "Aulib::Stream::play (from SoundSample::Play): {}", SDL_GetError());
		return false;
	}
	return true;
#endif
}

int SoundSample::SetChunkStream(std::string filePath, bool isMp3, bool logErrors)
{
#ifdef USE_SDL3
	SDL_IOStream *handle = OpenAssetAsSdlRwOps(filePath.c_str(), /*threadsafe=*/true);
	if (handle == nullptr) {
		if (logErrors)
			LogError(LogCategory::Audio, "OpenAsset failed (from SoundSample::SetChunkStream) for {}: {}", filePath, SDL_GetError());
		return -1;
	}

	file_path_ = std::move(filePath);
	isMp3_ = isMp3;

	// Let SDL3_mixer stream-decode (predecode=false). It keeps the compressed data in RAM
	// and decodes on the fly during playback, saving memory for large music files.
	audio_ = MIX_LoadAudio_IO(CurrentMixer, handle, /*predecode=*/false, /*closeio=*/true);
	if (audio_ == nullptr) {
		if (logErrors)
			LogError(LogCategory::Audio, "MIX_LoadAudio_IO failed (from SoundSample::SetChunkStream) for {}: {}", file_path_, SDL_GetError());
		return -1;
	}

	return 0;
#else
	SDL_IOStream *handle = OpenAssetAsSdlRwOps(filePath.c_str(), /*threadsafe=*/true);
	if (handle == nullptr) {
		if (logErrors)
			LogError(LogCategory::Audio, "OpenAsset failed (from SoundSample::SetChunkStream) for {}: {}", filePath, SDL_GetError());
		return -1;
	}
	file_path_ = std::move(filePath);
	isMp3_ = isMp3;
	stream_ = CreateStream(handle, isMp3);
	if (!stream_->open()) {
		stream_ = nullptr;
		if (logErrors)
			LogError(LogCategory::Audio, "Aulib::Stream::open (from SoundSample::SetChunkStream) for {}: {}", file_path_, SDL_GetError());
		return -1;
	}
	return 0;
#endif
}

int SoundSample::SetChunk(ArraySharedPtr<std::uint8_t> fileData, std::size_t dwBytes, bool isMp3)
{
#ifdef USE_SDL3
	isMp3_ = isMp3;
	file_data_ = std::move(fileData);
	file_data_size_ = dwBytes;

	// Create an IOStream from the in-memory data. SDL3_mixer will keep a reference
	// to the data, so file_data_ must remain alive for the lifetime of the audio.
	SDL_IOStream *io = SDL_IOFromConstMem(file_data_.get(), static_cast<size_t>(dwBytes));
	if (io == nullptr) {
		LogError(LogCategory::Audio, "SDL_IOFromConstMem failed: {}", SDL_GetError());
		return -1;
	}

	// For small sound effects, predecode to avoid repeated decoding overhead.
	audio_ = MIX_LoadAudio_IO(CurrentMixer, io, /*predecode=*/true, /*closeio=*/true);
	if (audio_ == nullptr) {
		LogError(LogCategory::Audio, "MIX_LoadAudio_IO failed (from SoundSample::SetChunk): {}", SDL_GetError());
		return -1;
	}

	return 0;
#else
	isMp3_ = isMp3;
	file_data_ = std::move(fileData);
	file_data_size_ = dwBytes;
	SDL_IOStream *buf = SDL_IOFromConstMem(file_data_.get(), static_cast<int>(dwBytes));
	if (buf == nullptr) {
		return -1;
	}

	stream_ = CreateStream(buf, isMp3_);
	if (!stream_->open()) {
		stream_ = nullptr;
		file_data_ = nullptr;
		LogError(LogCategory::Audio, "Aulib::Stream::open (from SoundSample::SetChunk): {}", SDL_GetError());
		return -1;
	}

	return 0;
#endif
}

void SoundSample::SetVolume(int logVolume, int logMin, int logMax)
{
#ifdef USE_SDL3
	gain_ = VolumeLogToLinear(logVolume, logMin, logMax);
	if (track_ != nullptr) {
		MIX_SetTrackGain(track_, gain_ * muteGain_);
	}
#else
	stream_->setVolume(VolumeLogToLinear(logVolume, logMin, logMax));
#endif
}

void SoundSample::SetStereoPosition(int logPan)
{
#ifdef USE_SDL3
	PanLogToLeftRight(logPan, leftGain_, rightGain_);
	hasStereoGains_ = true;
	if (track_ != nullptr) {
		MIX_StereoGains gains;
		gains.left = leftGain_;
		gains.right = rightGain_;
		MIX_SetTrackStereo(track_, &gains);
	}
#else
	stream_->setStereoPosition(PanLogToLinear(logPan));
#endif
}

int SoundSample::GetLength() const
{
#ifdef USE_SDL3
	if (audio_ == nullptr) return 0;
	Sint64 frames = MIX_GetAudioDuration(audio_);
	if (frames <= 0) return 0;
	Sint64 ms = MIX_AudioFramesToMS(audio_, frames);
	return static_cast<int>(ms);
#else
	if (!stream_)
		return 0;
	return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(stream_->duration()).count());
#endif
}

} // namespace devilution