#pragma once
#ifndef NOSOUND

#include <memory>

#include <Aulib/Stream.h>

#include "engine/sound_defs.hpp" // for DVL_AULIB_SUPPORTS_SDL_RESAMPLER

#ifdef DEVILUTIONX_RESAMPLER_SPEEX
#include <Aulib/ResamplerSpeex.h>
#endif

#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
#include <Aulib/ResamplerSdl.h>
#endif

#include "options.h"

namespace devilution {

inline std::unique_ptr<Aulib::Resampler> CreateAulibResampler(int sourceRate)
{
	if (Aulib::sampleRate() == sourceRate)
		return nullptr;
	switch (*GetOptions().Audio.resampler) {
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
	case Resampler::Speex:
		return std::make_unique<Aulib::ResamplerSpeex>(*GetOptions().Audio.resamplingQuality);
#endif
#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
	case Resampler::SDL:
		return std::make_unique<Aulib::ResamplerSdl>();
#endif
	}
	return nullptr;
}

} // namespace devilution

#endif // !NOSOUND
