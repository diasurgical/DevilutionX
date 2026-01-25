#include "engine/sound_pool.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <optional>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "engine/assets.hpp"
#include "engine/sound.h"
#include "engine/sound_position.hpp"
#include "utils/stdcompat/shared_ptr_array.hpp"

namespace devilution {

namespace {

constexpr size_t MaxEmitters = 3;
constexpr size_t SoundIdCount = static_cast<size_t>(SoundPool::SoundId::COUNT);

struct CachedSoundData {
	ArraySharedPtr<std::uint8_t> data;
	size_t size;
	bool isMp3;
};

[[nodiscard]] size_t ToIndex(SoundPool::SoundId id)
{
	return static_cast<size_t>(id);
}

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

} // namespace

struct SoundPool::Impl {
	struct ActiveEmitter {
		uint32_t emitterId;
		SoundId sound;
		SoundSample sample;
		uint32_t lastPlayMs;
	};

	std::array<std::optional<CachedSoundData>, SoundIdCount> cachedSounds;
	std::array<std::optional<ActiveEmitter>, MaxEmitters> activeEmitters;

	std::optional<SoundId> oneShotSoundId;
	SoundSample oneShotSample;

	void StopEmitter(ActiveEmitter &emitter)
	{
		if (emitter.sample.IsLoaded())
			emitter.sample.Stop();
		emitter.sample.Release();
	}

	void StopOneShot()
	{
		if (oneShotSample.IsLoaded())
			oneShotSample.Stop();
		oneShotSample.Release();
		oneShotSoundId = std::nullopt;
	}

	void StopAllEmitters()
	{
		for (auto &slot : activeEmitters) {
			if (!slot)
				continue;
			StopEmitter(*slot);
			slot = std::nullopt;
		}
	}

	[[nodiscard]] bool EnsureSampleLoaded(SoundSample &sample, SoundId id)
	{
		const std::optional<CachedSoundData> &cached = cachedSounds[ToIndex(id)];
		if (!cached)
			return false;

		const int error = sample.SetChunk(cached->data, cached->size, cached->isMp3);
		return error == 0;
	}

	[[nodiscard]] bool PlaySampleAt(SoundSample &sample, Point position)
	{
		if (!sample.IsLoaded())
			return false;

		int logVolume = 0;
		int logPan = 0;
		if (!CalculateSoundPosition(position, &logVolume, &logPan))
			return false;

		// Restart to keep tempo readable.
		if (sample.IsPlaying())
			sample.Stop();

		return sample.PlayWithVolumeAndPan(logVolume, sound_get_or_set_sound_volume(/*volume=*/1), logPan);
	}
};

SoundPool &SoundPool::Get()
{
	static SoundPool instance;
	return instance;
}

SoundPool::SoundPool()
    : impl_(std::make_unique<Impl>())
{
}

SoundPool::~SoundPool() = default;

void SoundPool::Clear()
{
	if (impl_ == nullptr)
		return;

	impl_->StopAllEmitters();
	impl_->StopOneShot();
	impl_->cachedSounds = {};
}

bool SoundPool::EnsureLoaded(SoundId id, std::initializer_list<std::string_view> candidatePaths)
{
	if (impl_ == nullptr)
		return false;
	if (id == SoundId::COUNT)
		return false;

	std::optional<CachedSoundData> &cached = impl_->cachedSounds[ToIndex(id)];
	if (cached)
		return true;

	// Match the old proximity-audio behavior: try multiple file types/paths and keep the first one
	// that successfully decodes in the current audio pipeline. This avoids caching an asset we can
	// locate but cannot decode (e.g. OGG on setups without an OGG decoder).
	for (const std::string_view path : candidatePaths) {
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

		const bool isMp3 = IsMp3Path(path);
		SoundSample testSample;
		if (testSample.SetChunk(fileData, size, isMp3) != 0)
			continue;

		cached = CachedSoundData { .data = std::move(fileData), .size = size, .isMp3 = isMp3 };
		return true;
	}

	return false;
}

bool SoundPool::IsLoaded(SoundId id) const
{
	if (impl_ == nullptr)
		return false;
	if (id == SoundId::COUNT)
		return false;

	return impl_->cachedSounds[ToIndex(id)].has_value();
}

void SoundPool::UpdateEmitters(std::span<const EmitterRequest> emitters, uint32_t nowMs)
{
	if (impl_ == nullptr)
		return;

	if (!gbSndInited || !gbSoundOn) {
		impl_->StopAllEmitters();
		return;
	}

	assert(emitters.size() <= MaxEmitters);

	const auto isRequested = [&emitters](uint32_t emitterId) {
		for (const auto &req : emitters) {
			if (req.emitterId == emitterId)
				return true;
		}
		return false;
	};

	for (auto &slot : impl_->activeEmitters) {
		if (!slot)
			continue;
		if (isRequested(slot->emitterId))
			continue;
		impl_->StopEmitter(*slot);
		slot = std::nullopt;
	}

	for (const auto &req : emitters) {
		std::optional<size_t> slotIndex;
		for (size_t i = 0; i < impl_->activeEmitters.size(); ++i) {
			if (impl_->activeEmitters[i] && impl_->activeEmitters[i]->emitterId == req.emitterId) {
				slotIndex = i;
				break;
			}
		}

		bool isNew = false;
		if (!slotIndex) {
			for (size_t i = 0; i < impl_->activeEmitters.size(); ++i) {
				if (!impl_->activeEmitters[i]) {
					impl_->activeEmitters[i].emplace();
					impl_->activeEmitters[i]->emitterId = req.emitterId;
					impl_->activeEmitters[i]->sound = SoundId::COUNT;
					impl_->activeEmitters[i]->lastPlayMs = nowMs;
					impl_->activeEmitters[i]->sample.Release();
					slotIndex = i;
					isNew = true;
					break;
				}
			}
		}

		if (!slotIndex)
			continue;

		Impl::ActiveEmitter &active = *impl_->activeEmitters[*slotIndex];

		if (active.sound != req.sound || !active.sample.IsLoaded()) {
			active.sample.Release();
			active.sound = req.sound;
			if (!impl_->EnsureSampleLoaded(active.sample, req.sound)) {
				impl_->StopEmitter(active);
				impl_->activeEmitters[*slotIndex] = std::nullopt;
				continue;
			}
		}

		const bool shouldPlay = isNew || (req.intervalMs != 0 && nowMs - active.lastPlayMs >= req.intervalMs);
		if (!shouldPlay)
			continue;

		if (impl_->PlaySampleAt(active.sample, req.position))
			active.lastPlayMs = nowMs;
	}
}

void SoundPool::PlayOneShot(SoundId id, Point position, bool stopEmitters, uint32_t nowMs)
{
	if (impl_ == nullptr)
		return;

	if (!gbSndInited || !gbSoundOn)
		return;

	if (stopEmitters)
		impl_->StopAllEmitters();

	if (impl_->oneShotSoundId != id || !impl_->oneShotSample.IsLoaded()) {
		impl_->oneShotSample.Release();
		impl_->oneShotSoundId = id;
		if (!impl_->EnsureSampleLoaded(impl_->oneShotSample, id)) {
			impl_->StopOneShot();
			return;
		}
	}

	(void)nowMs;
	impl_->PlaySampleAt(impl_->oneShotSample, position);
}

} // namespace devilution
