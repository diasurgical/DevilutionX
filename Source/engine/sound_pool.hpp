#pragma once

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <span>
#include <string_view>

#include "engine/point.hpp"

namespace devilution {

class SoundPool {
public:
	enum class SoundId : uint8_t {
		WeaponItem,
		ArmorItem,
		GoldItem,
		PotionItem,
		ScrollItem,
		Chest,
		Door,
		Stairs,
		Monster,
		Interact,
		COUNT,
	};

	struct EmitterRequest {
		uint32_t emitterId;
		SoundId sound;
		Point position;
		uint32_t intervalMs;
	};

	static SoundPool &Get();

	SoundPool(const SoundPool &) = delete;
	SoundPool &operator=(const SoundPool &) = delete;

	void Clear();

	[[nodiscard]] bool EnsureLoaded(SoundId id, std::initializer_list<std::string_view> candidatePaths);
	[[nodiscard]] bool IsLoaded(SoundId id) const;

	void UpdateEmitters(std::span<const EmitterRequest> emitters, uint32_t nowMs);

	// For one-shot navigation cues (not counted in the 3 emitter limit).
	void PlayOneShot(SoundId id, Point position, bool stopEmitters, uint32_t nowMs);

private:
	struct Impl;

	SoundPool();
	~SoundPool();

	std::unique_ptr<Impl> impl_;
};

} // namespace devilution
