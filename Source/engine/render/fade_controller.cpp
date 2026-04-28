/**
 * @file fade_controller.cpp
 *
 * Fade state machine implementation.
 */
#include "engine/render/fade_controller.h"

#include <algorithm>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

namespace devilution {

void FadeController::BlackOutScreen()
{
	fadeDirection_ = FadeDirection::None;
	fadeLevel_ = 256;
	overlayActive_ = true;
}

void FadeController::StartFadeIn(int fr)
{
	if (!overlayActive_)
		return;

	if (fr <= 0) {
		fadeDirection_ = FadeDirection::None;
		fadeLevel_ = 0;
		overlayActive_ = false;
		return;
	}

	fadeDirection_ = FadeDirection::In;
	fadeStartMs_ = SDL_GetTicks();
	fadeSpeed_ = fr * 3;
	fadeStartLevel_ = fadeLevel_;
}

void FadeController::StartFadeOut(int fr)
{
	if (fr <= 0) {
		fadeDirection_ = FadeDirection::None;
		fadeLevel_ = 256;
		overlayActive_ = true;
		return;
	}

	fadeDirection_ = FadeDirection::Out;
	fadeStartMs_ = SDL_GetTicks();
	fadeSpeed_ = fr * 3;
	fadeStartLevel_ = fadeLevel_;
	overlayActive_ = true;
}

bool FadeController::IsFading() const
{
	return fadeDirection_ != FadeDirection::None;
}

int FadeController::GetFadeLevel() const
{
	return fadeLevel_;
}

bool FadeController::IsBlackedOut() const
{
	return overlayActive_;
}

int FadeController::Update()
{
	if (!overlayActive_)
		return -1;

	if (fadeDirection_ != FadeDirection::None) {
		const uint32_t elapsed = SDL_GetTicks() - fadeStartMs_;
		const int progress = std::min<int>(static_cast<int>(static_cast<uint64_t>(fadeSpeed_) * elapsed / 50), 256);

		if (fadeDirection_ == FadeDirection::In) {
			fadeLevel_ = std::max(0, fadeStartLevel_ - (fadeStartLevel_ * progress / 256));
		} else {
			fadeLevel_ = std::min(256, fadeStartLevel_ + ((256 - fadeStartLevel_) * progress / 256));
		}

		if (progress >= 256) {
			fadeDirection_ = FadeDirection::None;
			if (fadeLevel_ <= 0) {
				fadeLevel_ = 0;
				overlayActive_ = false;
				return 0;
			} else {
				fadeLevel_ = 256;
			}
		}
	}

	return fadeLevel_;
}

} // namespace devilution
