/**
 * @file movie.cpp
 *
 * Implementation of video playback.
 */

#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keycode.h>
#else
#include <SDL.h>
#endif

#include "controls/control_mode.hpp"
#include "controls/plrctrls.h"
#include "diablo.h"
#include "effects.h"
#include "engine/backbuffer_state.hpp"
#include "engine/demomode.h"
#include "engine/events.hpp"
#include "engine/sound.h"
#include "hwcursor.hpp"
#include "storm/storm_svid.h"
#include "utils/display.h"
#include "utils/sdl_compat.h"

namespace devilution {

/** Should the movie continue playing. */
bool movie_playing;
/** Should the movie play in a loop. */
bool loop_movie;

void play_movie(const char *pszMovie, bool userCanClose)
{
	if (demo::IsRunning())
		return;

	movie_playing = true;

	sound_disable_music(true);
	stream_stop();

	if (IsHardwareCursorEnabled() && ControlDevice == ControlTypes::KeyboardAndMouse) {
		SetHardwareCursorVisible(false);
	}

	if (SVidPlayBegin(pszMovie, loop_movie ? 0x100C0808 : 0x10280808)) {
		SDL_Event event;
		uint16_t modState;
		while (movie_playing) {
			while (movie_playing && FetchMessage(&event, &modState)) {
				if (userCanClose) {
					for (const ControllerButtonEvent ctrlEvent : ToControllerButtonEvents(event)) {
						if (!SkipsMovie(ctrlEvent))
							continue;
						movie_playing = false;
						break;
					}
				}
				switch (event.type) {
				case SDL_EVENT_KEY_DOWN:
				case SDL_EVENT_MOUSE_BUTTON_UP:
					if (userCanClose || (event.type == SDL_EVENT_KEY_DOWN && SDLC_EventKey(event) == SDLK_ESCAPE))
						movie_playing = false;
					break;
#ifdef USE_SDL3
				case SDL_EVENT_WINDOW_FOCUS_LOST:
					if (*GetOptions().Gameplay.pauseOnFocusLoss) diablo_focus_pause();
					break;
				case SDL_EVENT_WINDOW_FOCUS_GAINED:
					if (*GetOptions().Gameplay.pauseOnFocusLoss) diablo_focus_unpause();
					break;
#elif !defined(USE_SDL1)
				case SDL_WINDOWEVENT:
					if (*GetOptions().Gameplay.pauseOnFocusLoss) {
						if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
							diablo_focus_pause();
						else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
							diablo_focus_unpause();
					}
					break;
#else
				case SDL_ACTIVEEVENT:
					if ((event.active.state & SDL_APPINPUTFOCUS) != 0) {
						if (event.active.gain == 0)
							diablo_focus_pause();
						else
							diablo_focus_unpause();
					}
					break;
#endif
				case SDL_EVENT_QUIT:
					SVidPlayEnd();
					diablo_quit(0);
				default:
					break;
				}
			}
			if (!SVidPlayContinue())
				break;
		}
		SVidPlayEnd();
	}

	sound_disable_music(false);

	movie_playing = false;

#ifdef USE_SDL3
	float x, y;
	SDL_GetMouseState(&x, &y);
	MousePosition.x = static_cast<int>(x);
	MousePosition.y = static_cast<int>(y);
#else
	SDL_GetMouseState(&MousePosition.x, &MousePosition.y);
#endif
	OutputToLogical(&MousePosition.x, &MousePosition.y);
	InitBackbufferState();
}

void PlayInGameMovie(const char *pszMovie)
{
	PaletteFadeOut(8);
	play_movie(pszMovie, false);
	ClearScreenBuffer();
	RedrawEverything();
	scrollrt_draw_game_screen();
	PaletteFadeIn(8);
	RedrawEverything();
}

} // namespace devilution
