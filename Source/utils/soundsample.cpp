#include "utils/soundsample.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <utility>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#else
#include <Aulib/Decoder.h>
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

#ifndef USE_SDL3
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

class PlaybackRateDecoder final : public Aulib::Decoder {
public:
	PlaybackRateDecoder(std::unique_ptr<Aulib::Decoder> inner, float playbackRate)
	    : inner_(std::move(inner))
	    , playbackRate_(playbackRate)
	{
	}

	auto open(SDL_RWops *rwops) -> bool override
	{
		if (isOpen())
			return true;
		if (inner_ == nullptr)
			return false;

		if (!inner_->open(rwops))
			return false;

		setIsOpen(true);
		return true;
	}

	auto getChannels() const -> int override
	{
		return inner_ != nullptr ? inner_->getChannels() : 0;
	}

	auto getRate() const -> int override
	{
		if (inner_ == nullptr)
			return 0;

		const int baseRate = inner_->getRate();
		if (baseRate <= 0)
			return baseRate;

		const int adjustedRate = static_cast<int>(std::lround(static_cast<float>(baseRate) * playbackRate_));
		return std::max(adjustedRate, 1);
	}

	auto rewind() -> bool override
	{
		return inner_ != nullptr && inner_->rewind();
	}

	auto duration() const -> std::chrono::microseconds override
	{
		if (inner_ == nullptr)
			return {};

		const auto base = inner_->duration();
		if (playbackRate_ <= 0)
			return base;

		return std::chrono::duration_cast<std::chrono::microseconds>(base / playbackRate_);
	}

	auto seekToTime(std::chrono::microseconds pos) -> bool override
	{
		if (inner_ == nullptr)
			return false;
		if (playbackRate_ <= 0)
			return inner_->seekToTime(pos);

		return inner_->seekToTime(std::chrono::duration_cast<std::chrono::microseconds>(pos * playbackRate_));
	}

protected:
	auto doDecoding(float buf[], int len, bool &callAgain) -> int override
	{
		return inner_ != nullptr ? inner_->decode(buf, len, callAgain) : 0;
	}

private:
	std::unique_ptr<Aulib::Decoder> inner_;
	float playbackRate_;
};

std::unique_ptr<Aulib::Stream> CreateStream(SDL_IOStream *handle, bool isMp3, float playbackRate)
{
	std::unique_ptr<Aulib::Decoder> decoder;
	if (isMp3) {
		decoder = std::make_unique<Aulib::DecoderDrmp3>();
	} else {
		const auto rwPos = SDL_RWtell(handle);
		decoder = Aulib::Decoder::decoderFor(handle);
		SDL_RWseek(handle, rwPos, RW_SEEK_SET);
		if (decoder == nullptr)
			decoder = std::make_unique<Aulib::DecoderDrwav>();
	}

	if (playbackRate != 1.0F)
		decoder = std::make_unique<PlaybackRateDecoder>(std::move(decoder), playbackRate);

	if (!decoder->open(handle)) // open for `getRate`
		return nullptr;
	auto resampler = CreateAulibResampler(decoder->getRate());
	return std::make_unique<Aulib::Stream>(handle, std::move(decoder), std::move(resampler), /*closeRw=*/true);
}

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
#endif

} // namespace

///// SoundSample /////

SoundSample::SoundSample() = default;
SoundSample::~SoundSample() = default;
SoundSample::SoundSample(SoundSample &&) noexcept = default;
SoundSample &SoundSample::operator=(SoundSample &&) noexcept = default;

#ifndef USE_SDL3
void SoundSample::SetFinishCallback(Aulib::Stream::Callback &&callback)
{
	stream_->setFinishCallback(std::forward<Aulib::Stream::Callback>(callback));
}
#endif

void SoundSample::Stop()
{
#ifndef USE_SDL3
	stream_->stop();
#endif
}

void SoundSample::Mute()
{
#ifndef USE_SDL3
	stream_->mute();
#endif
}

void SoundSample::Unmute()
{
#ifndef USE_SDL3
	stream_->unmute();
#endif
}

void SoundSample::Release()
{
#ifndef USE_SDL3
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
	return false;
#else
	return stream_ && stream_->isPlaying();
#endif
}

bool SoundSample::Play(int numIterations)
{
#ifdef USE_SDL3
	return false;
#else
	if (!stream_->play(numIterations)) {
		LogError(LogCategory::Audio, "Aulib::Stream::play (from SoundSample::Play): {}", SDL_GetError());
		return false;
	}
	return true;
#endif
}

int SoundSample::SetChunkStream(std::string filePath, bool isMp3, bool logErrors, float playbackRate)
{
#ifdef USE_SDL3
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
	playbackRate_ = playbackRate;
	stream_ = CreateStream(handle, isMp3, playbackRate_);
	if (!stream_) {
		SDL_RWclose(handle);
		if (logErrors)
			LogError(LogCategory::Audio, "CreateStream failed (from SoundSample::SetChunkStream) for {}: {}", file_path_, SDL_GetError());
		return -1;
	}
	if (!stream_->open()) {
		stream_ = nullptr;
		if (logErrors)
			LogError(LogCategory::Audio, "Aulib::Stream::open (from SoundSample::SetChunkStream) for {}: {}", file_path_, SDL_GetError());
		return -1;
	}
	return 0;
#endif
}

int SoundSample::SetChunk(ArraySharedPtr<std::uint8_t> fileData, std::size_t dwBytes, bool isMp3, float playbackRate)
{
#ifdef USE_SDL3
	return 0;
#else
	isMp3_ = isMp3;
	playbackRate_ = playbackRate;
	file_data_ = std::move(fileData);
	file_data_size_ = dwBytes;
	SDL_IOStream *buf = SDL_IOFromConstMem(file_data_.get(), static_cast<int>(dwBytes));
	if (buf == nullptr) {
		return -1;
	}

	stream_ = CreateStream(buf, isMp3_, playbackRate_);
	if (!stream_) {
		SDL_RWclose(buf);
		file_data_ = nullptr;
		return -1;
	}
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
#ifndef USE_SDL3
	stream_->setVolume(VolumeLogToLinear(logVolume, logMin, logMax));
#endif
}

void SoundSample::SetStereoPosition(int logPan)
{
#ifndef USE_SDL3
	stream_->setStereoPosition(PanLogToLinear(logPan));
#endif
}

int SoundSample::GetLength() const
{
#ifdef USE_SDL3
	return 0;
#else
	if (!stream_)
		return 0;
	return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(stream_->duration()).count());
#endif
}

} // namespace devilution
