#pragma once

#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <queue>
#include <variant>

#include <Aulib/Decoder.h>

#ifdef USE_SDL3
#include <SDL3/SDL_mutex.h>
#else
#include <SDL_mutex.h>
#endif

#include "utils/sdl_compat.h"
#include "utils/sdl_mutex.h"

namespace devilution {

/**
 * @brief A Decoder interface implementations that simply has the samples pushed into it by the user.
 */
class PushAulibDecoder final : public ::Aulib::Decoder {
public:
	PushAulibDecoder(int numChannels, int sampleRate)
	    : numChannels_(numChannels)
	    , sampleRate_(sampleRate)
	{
	}

	template <typename T>
	void PushSamples(const T *data, unsigned size) noexcept
	{
		AudioQueueItem item { data, size };
		const auto lock = std::lock_guard(queue_mutex_);
		queue_.push(std::move(item));
	}

	void DiscardPendingSamples() noexcept;

	bool open(SDL_IOStream *rwops) override;

	[[nodiscard]] int getChannels() const override
	{
		return numChannels_;
	}

	[[nodiscard]] int getRate() const override
	{
		return sampleRate_;
	}

	bool rewind() override;
	[[nodiscard]] std::chrono::microseconds duration() const override;
	bool seekToTime(std::chrono::microseconds pos) override;

protected:
	int doDecoding(float buf[], int len, bool &callAgain) override;

private:
	struct AudioQueueItem {
		std::variant<
		    std::unique_ptr<int16_t[]>,
		    std::unique_ptr<uint8_t[]>>
		    data;
		unsigned len;
		unsigned pos;

		template <typename T>
		AudioQueueItem(const T *data, unsigned size)
		    : data { std::unique_ptr<T[]> { new T[size] } }
		    , len { size }
		    , pos { 0 }
		{
			std::memcpy(std::get<std::unique_ptr<T[]>>(this->data).get(), data, size * sizeof(T));
		}
	};

	const int numChannels_;
	const int sampleRate_;

	// Requires holding the queue_mutex_.
	AudioQueueItem *Next();

	std::queue<AudioQueueItem> queue_;
	SdlMutex queue_mutex_;
};

} // namespace devilution
