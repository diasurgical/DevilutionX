#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "engine/sound_defs.hpp"
#include "utils/stdcompat/shared_ptr_array.hpp"

#ifndef USE_SDL3
// Forward-declares Aulib::Stream to avoid adding dependencies
// on SDL_audiolib to every user of this header.
namespace Aulib {
class Stream;
} // namespace Aulib
#endif

namespace devilution {

class SoundSample final {
public:
	SoundSample() = default;
	SoundSample(SoundSample &&) noexcept = default;
	SoundSample &operator=(SoundSample &&) noexcept = default;

	[[nodiscard]] bool IsLoaded() const
	{
#ifdef USE_SDL3
		return false;
#else
		return stream_ != nullptr;
#endif
	}

	void Release();
	bool IsPlaying();

	// Returns 0 on success.
	int SetChunkStream(std::string filePath, bool isMp3, bool logErrors = true);

#if !defined(USE_SDL3) && !defined(PS2)
	void SetFinishCallback(std::function<void(Aulib::Stream &)> &&callback);
#endif

	/**
	 * @brief Sets the sample's WAV, FLAC, or Ogg/Vorbis data.
	 * @param fileData Buffer containing the data
	 * @param dwBytes Length of buffer
	 * @param isMp3 Whether the data is an MP3
	 * @return 0 on success, -1 otherwise
	 */
	int SetChunk(ArraySharedPtr<std::uint8_t> fileData, std::size_t dwBytes, bool isMp3);

	[[nodiscard]] bool IsStreaming() const
	{
#ifdef PS2
		return sampleId_ == nullptr;
#else
		return file_data_ == nullptr;
#endif
	}

	int DuplicateFrom(const SoundSample &other)
	{
#ifdef PS2
		if (other.IsStreaming())
			return SetChunkStream(other.file_path_, false);
		sampleId_ = other.sampleId_;
		return 0;
#else
		if (other.IsStreaming())
			return SetChunkStream(other.file_path_, other.isMp3_);
		return SetChunk(other.file_data_, other.file_data_size_, other.isMp3_);
#endif
	}

	/**
	 * @brief Start playing the sound for a given number of iterations (0 means loop).
	 */
	bool Play(int numIterations = 1);

	/**
	 * @brief Start playing the sound with the given sound and user volume, and a stereo position.
	 */
	bool PlayWithVolumeAndPan(int logSoundVolume, int logUserVolume, int logPan)
	{
		SetVolume(logSoundVolume + logUserVolume * (ATTENUATION_MIN / VOLUME_MIN), ATTENUATION_MIN, 0);
		SetStereoPosition(logPan);
		return Play();
	}

	/**
	 * @brief Stop playing the sound
	 */
	void Stop();

	void SetVolume(int logVolume, int logMin, int logMax);
	void SetStereoPosition(int logPan);

	void Mute();
	void Unmute();

	/**
	 * @return Audio duration in ms
	 */
	int GetLength() const;

private:
	// Set for streaming audio to allow for duplicating it:
	std::string file_path_;

#ifdef PS2
	int channel_ = -1;
	int pan_ = 0;
	int volume_ = 100;
	audsrv_adpcm_t *sampleId_ = nullptr;
	std::unique_ptr<audsrv_adpcm_t> stream_;
#else
	// Non-streaming audio fields:
	ArraySharedPtr<std::uint8_t> file_data_;
	std::size_t file_data_size_;

	bool isMp3_;

#ifndef USE_SDL3
	std::unique_ptr<Aulib::Stream> stream_;
#endif
};

} // namespace devilution
