// Stubbed implementation of SoundPool for the NOSOUND mode.
#include "engine/sound_pool.hpp"

namespace devilution {

struct SoundPool::Impl {
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
}

bool SoundPool::EnsureLoaded(SoundId id, std::initializer_list<std::string_view> candidatePaths)
{
	(void)id;
	(void)candidatePaths;
	return false;
}

bool SoundPool::IsLoaded(SoundId id) const
{
	(void)id;
	return false;
}

void SoundPool::UpdateEmitters(std::span<const EmitterRequest> emitters, uint32_t nowMs)
{
	(void)emitters;
	(void)nowMs;
}

void SoundPool::PlayOneShot(SoundId id, Point position, bool stopEmitters, uint32_t nowMs)
{
	(void)id;
	(void)position;
	(void)stopEmitters;
	(void)nowMs;
}

} // namespace devilution

