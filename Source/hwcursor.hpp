/**
 * @file hwcursor.hpp
 *
 * Hardware cursor (SDL2 only).
 */
#pragma once

#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_version.h>
#else
#include <SDL_version.h>
#endif

#include "options.h"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"

// Set this to 1 to log the hardware cursor state changes.
#define LOG_HWCURSOR 0

namespace devilution {

// Whether the hardware cursor is enabled in settings.
inline bool IsHardwareCursorEnabled()
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return *GetOptions().Graphics.hardwareCursor && HardwareCursorSupported();
#else
	return false;
#endif
}

enum class CursorType : uint8_t {
	Unknown,
	UserInterface,
	Game,
};

class CursorInfo {
public:
	CursorInfo() = default;

	static CursorInfo UserInterfaceCursor()
	{
		return CursorInfo { CursorType::UserInterface };
	}

	static CursorInfo GameCursor(int gameSpriteId)
	{
		return CursorInfo { CursorType::Game, gameSpriteId };
	}

	static CursorInfo UnknownCursor()
	{
		return CursorInfo { CursorType::Unknown };
	}

	[[nodiscard]] CursorType type() const
	{
		return type_;
	}

	[[nodiscard]] int id() const
	{
		return id_;
	}

	[[nodiscard]] bool Enabled() const
	{
		return enabled_;
	}

	void SetEnabled(bool value)
	{
#if LOG_HWCURSOR
		if (enabled_ != value) {
			Log("hwcursor: SetEnabled {}", value);
		}
#endif
		enabled_ = value;
	}

	[[nodiscard]] bool needsReinitialization() const
	{
		return needs_reinitialization_;
	}

	void setNeedsReinitialization(bool value)
	{
#if LOG_HWCURSOR
		if (needs_reinitialization_ != value) {
			Log("hwcursor: setNeedsReinitialization {}", value);
		}
#endif
		needs_reinitialization_ = value;
	}

	bool operator==(const CursorInfo &other) const
	{
		return type_ == other.type_ && (type_ != CursorType::Game || id_ == other.id_);
	}
	bool operator!=(const CursorInfo &other) const
	{
		return !(*this == other);
	}

private:
	explicit CursorInfo(CursorType type, int id = 0)
	    : type_(type)
	    , id_(id)
	    , enabled_(false)
	{
	}

	CursorType type_ = CursorType::Unknown;

	// ID for CursorType::Game
	int id_;

	bool enabled_ = false;

	bool needs_reinitialization_ = false;
};

CursorInfo &GetCurrentCursorInfo();

// Whether the current cursor is a hardware cursor.
inline bool IsHardwareCursor()
{
	return GetCurrentCursorInfo().Enabled();
}

void SetHardwareCursor(CursorInfo cursorInfo);

inline void DoReinitializeHardwareCursor()
{
#if LOG_HWCURSOR
	Log("hwcursor: DoReinitializeHardwareCursor");
#endif
	SetHardwareCursor(GetCurrentCursorInfo());
}

inline bool IsHardwareCursorVisible()
{
#ifndef USE_SDL1
	return SDL_CursorVisible();
#else
	return false;
#endif
}

inline void SetHardwareCursorVisible(bool visible)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (IsHardwareCursorVisible() == visible)
		return;
	if (visible && GetCurrentCursorInfo().needsReinitialization()) {
		DoReinitializeHardwareCursor();
	}
#if LOG_HWCURSOR
	Log("hwcursor: SetHardwareCursorVisible {}", visible);
#endif
	if (!(visible ? SDLC_ShowCursor() : SDLC_HideCursor())) {
		LogError("{}", SDL_GetError());
		SDL_ClearError();
	}
#endif
}

inline void ReinitializeHardwareCursor()
{
	if (IsHardwareCursorVisible()) {
		DoReinitializeHardwareCursor();
	} else {
		GetCurrentCursorInfo().setNeedsReinitialization(true);
	}
}

} // namespace devilution
