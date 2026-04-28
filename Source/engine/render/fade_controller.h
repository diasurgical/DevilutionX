/**
 * @file fade_controller.h
 *
 * FadeController: manages the black overlay fade state machine.
 * Each renderer backend owns an instance privately.
 */
#pragma once

#include <cstdint>

namespace devilution {

class FadeController {
public:
	/**
	 * @brief Immediately set the overlay to fully opaque (screen is black).
	 */
	void BlackOutScreen();

	/**
	 * @brief Begin fading in (overlay goes from current opacity → transparent).
	 * @param fr Speed factor. 0 means instant (overlay removed).
	 */
	void StartFadeIn(int fr);

	/**
	 * @brief Begin fading out (overlay goes from current opacity → fully opaque).
	 * @param fr Speed factor. 0 = instant.
	 */
	void StartFadeOut(int fr);

	/**
	 * @brief Returns true while a fade animation is in progress.
	 */
	bool IsFading() const;

	/**
	 * @brief Returns the current overlay opacity.
	 * @return 0 = fully transparent (no overlay), 256 = fully opaque (black).
	 */
	int GetFadeLevel() const;

	/**
	 * @brief Returns true if the black overlay is active (any opacity > 0).
	 */
	bool IsBlackedOut() const;

	/**
	 * @brief Advance the fade timer. Returns the current fade level if the overlay
	 * is active (caller should apply it), or -1 if no overlay is active.
	 */
	int Update();

private:
	enum class FadeDirection {
		None,
		In,
		Out,
	};

	FadeDirection fadeDirection_ = FadeDirection::None;
	uint32_t fadeStartMs_ = 0;
	int fadeSpeed_ = 0;
	int fadeStartLevel_ = 0;
	int fadeLevel_ = 0;
	bool overlayActive_ = false;
};

} // namespace devilution
