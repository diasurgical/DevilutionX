// Compatibility wrappers for SDL 1 & 2.
#pragma once

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_version.h>
#else
#include <SDL.h>
#ifdef USE_SDL1
#include "utils/display.h"
#else
#include "utils/sdl2_backports.h"
#endif
#endif

#ifdef USE_SDL1
#define SDL_Scancode Uint8
#endif

#ifdef USE_SDL3
#define SDLC_PushEvent SDL_PushEvent
#define SDLC_SetClipboardText SDL_SetClipboardText
#define SDLC_HasClipboardText SDL_HasClipboardText

inline SDL_Keycode SDLC_EventKey(const SDL_Event &event) { return event.key.key; }
inline SDL_Scancode SDLC_EventScancode(const SDL_Event &event) { return event.key.scancode; }

inline int SDLC_EventMotionIntX(const SDL_Event &event) { return static_cast<int>(event.motion.x); }
inline int SDLC_EventMotionIntY(const SDL_Event &event) { return static_cast<int>(event.motion.y); }
inline int SDLC_EventButtonIntX(const SDL_Event &event) { return static_cast<int>(event.button.x); }
inline int SDLC_EventButtonIntY(const SDL_Event &event) { return static_cast<int>(event.button.y); }
inline int SDLC_EventWheelIntX(const SDL_Event &event)
{
#if SDL_VERSION_ATLEAST(3, 2, 12)
	return event.wheel.integer_x;
#else
	return static_cast<int>(event.wheel.x);
#endif
}

inline int SDLC_EventWheelIntY(const SDL_Event &event)
{
#if SDL_VERSION_ATLEAST(3, 2, 12)
	return event.wheel.integer_y;
#else
	return static_cast<int>(event.wheel.y);
#endif
}

inline const SDL_GamepadAxisEvent &SDLC_EventGamepadAxis(const SDL_Event &event) { return event.gaxis; }
inline const SDL_GamepadButtonEvent &SDLC_EventGamepadButton(const SDL_Event &event) { return event.gbutton; }
inline const SDL_GamepadDeviceEvent &SDLC_EventGamepadDevice(const SDL_Event &event) { return event.gdevice; }

#define SDLC_SURFACE_BITSPERPIXEL(surface) SDL_BITSPERPIXEL(surface->format)

#define SDLC_StartTextInput SDL_StartTextInput
#define SDLC_StopTextInput SDL_StopTextInput

#define SDLC_SetSurfacePalette SDL_SetSurfacePalette

#define SDLC_PointInRect SDL_PointInRect

#define SDLC_ShowCursor SDL_ShowCursor
#define SDLC_HideCursor SDL_HideCursor

#define SDLC_ShowSimpleMessageBox SDL_ShowSimpleMessageBox
#else
#define SDL_EVENT_AUDIO_DEVICE_ADDED SDL_AUDIODEVICEADDED
#define SDL_EVENT_AUDIO_DEVICE_REMOVED SDL_AUDIODEVICEREMOVED
#define SDL_EVENT_CLIPBOARD_UPDATE SDL_CLIPBOARDUPDATE
#define SDL_EVENT_GAMEPAD_AXIS_MOTION SDL_CONTROLLERAXISMOTION
#define SDL_EVENT_GAMEPAD_BUTTON_DOWN SDL_CONTROLLERBUTTONDOWN
#define SDL_EVENT_GAMEPAD_BUTTON_UP SDL_CONTROLLERBUTTONUP
#define SDL_EVENT_GAMEPAD_ADDED SDL_CONTROLLERDEVICEADDED
#define SDL_EVENT_GAMEPAD_REMAPPED SDL_CONTROLLERDEVICEREMAPPED
#define SDL_EVENT_GAMEPAD_REMOVED SDL_CONTROLLERDEVICEREMOVED
#define SDL_EVENT_GAMEPAD_SENSOR_UPDATE SDL_CONTROLLERSENSORUPDATE
#define SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED SDL_CONTROLLERSTEAMHANDLEUPDATED
#define SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN SDL_CONTROLLERTOUCHPADDOWN
#define SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION SDL_CONTROLLERTOUCHPADMOTION
#define SDL_EVENT_GAMEPAD_TOUCHPAD_UP SDL_CONTROLLERTOUCHPADUP
#define SDL_GAMEPAD_AXIS_INVALID SDL_CONTROLLER_AXIS_INVALID
#define SDL_GAMEPAD_AXIS_LEFTX SDL_CONTROLLER_AXIS_LEFTX
#define SDL_GAMEPAD_AXIS_LEFTY SDL_CONTROLLER_AXIS_LEFTY
#define SDL_GAMEPAD_AXIS_COUNT SDL_CONTROLLER_AXIS_MAX
#define SDL_GAMEPAD_AXIS_RIGHTX SDL_CONTROLLER_AXIS_RIGHTX
#define SDL_GAMEPAD_AXIS_RIGHTY SDL_CONTROLLER_AXIS_RIGHTY
#define SDL_GAMEPAD_AXIS_LEFT_TRIGGER SDL_CONTROLLER_AXIS_TRIGGERLEFT
#define SDL_GAMEPAD_AXIS_RIGHT_TRIGGER SDL_CONTROLLER_AXIS_TRIGGERRIGHT
#define SDL_EVENT_FINGER_DOWN SDL_FINGERDOWN
#define SDL_EVENT_FINGER_MOTION SDL_FINGERMOTION
#define SDL_EVENT_FINGER_UP SDL_FINGERUP
#define SDL_EVENT_JOYSTICK_AXIS_MOTION SDL_JOYAXISMOTION
#define SDL_EVENT_JOYSTICK_BATTERY_UPDATED SDL_JOYBATTERYUPDATED
#define SDL_EVENT_JOYSTICK_BUTTON_DOWN SDL_JOYBUTTONDOWN
#define SDL_EVENT_JOYSTICK_BUTTON_UP SDL_JOYBUTTONUP
#define SDL_EVENT_JOYSTICK_ADDED SDL_JOYDEVICEADDED
#define SDL_EVENT_JOYSTICK_REMOVED SDL_JOYDEVICEREMOVED
#define SDL_EVENT_JOYSTICK_BALL_MOTION SDL_JOYBALLMOTION
#define SDL_EVENT_JOYSTICK_HAT_MOTION SDL_JOYHATMOTION
#define SDL_EVENT_KEY_DOWN SDL_KEYDOWN
#define SDL_EVENT_KEYMAP_CHANGED SDL_KEYMAPCHANGED
#define SDL_EVENT_KEY_UP SDL_KEYUP
#define SDL_EVENT_LAST SDL_LASTEVENT
#define SDL_EVENT_LOCALE_CHANGED SDL_LOCALECHANGED
#define SDL_EVENT_MOUSE_BUTTON_DOWN SDL_MOUSEBUTTONDOWN
#define SDL_EVENT_MOUSE_BUTTON_UP SDL_MOUSEBUTTONUP
#define SDL_EVENT_MOUSE_MOTION SDL_MOUSEMOTION
#define SDL_EVENT_MOUSE_WHEEL SDL_MOUSEWHEEL
#define SDL_EVENT_POLL_SENTINEL SDL_POLLSENTINEL
#define SDL_EVENT_QUIT SDL_QUIT
#define SDL_EVENT_TEXT_INPUT SDL_TEXTINPUT
#define SDL_EVENT_USER SDL_USEREVENT

#define SDL_Gamepad SDL_GameController
#define SDL_GamepadButton SDL_GameControllerButton
#define SDL_GamepadAxisEvent SDL_ControllerAxisEvent
#define SDL_GamepadButtonEvent SDL_ControllerButtonEvent
#define SDL_GamepadDeviceEvent SDL_ControllerDeviceEvent
#define SDL_GAMEPAD_BUTTON_SOUTH SDL_CONTROLLER_BUTTON_A
#define SDL_GAMEPAD_BUTTON_EAST SDL_CONTROLLER_BUTTON_B
#define SDL_GAMEPAD_BUTTON_BACK SDL_CONTROLLER_BUTTON_BACK
#define SDL_GAMEPAD_BUTTON_DPAD_DOWN SDL_CONTROLLER_BUTTON_DPAD_DOWN
#define SDL_GAMEPAD_BUTTON_DPAD_LEFT SDL_CONTROLLER_BUTTON_DPAD_LEFT
#define SDL_GAMEPAD_BUTTON_DPAD_RIGHT SDL_CONTROLLER_BUTTON_DPAD_RIGHT
#define SDL_GAMEPAD_BUTTON_DPAD_UP SDL_CONTROLLER_BUTTON_DPAD_UP
#define SDL_GAMEPAD_BUTTON_GUIDE SDL_CONTROLLER_BUTTON_GUIDE
#define SDL_GAMEPAD_BUTTON_INVALID SDL_CONTROLLER_BUTTON_INVALID
#define SDL_GAMEPAD_BUTTON_LEFT_SHOULDER SDL_CONTROLLER_BUTTON_LEFTSHOULDER
#define SDL_GAMEPAD_BUTTON_LEFT_STICK SDL_CONTROLLER_BUTTON_LEFTSTICK
#define SDL_GAMEPAD_BUTTON_COUNT SDL_CONTROLLER_BUTTON_MAX
#define SDL_GAMEPAD_BUTTON_MISC1 SDL_CONTROLLER_BUTTON_MISC1
#define SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1 SDL_CONTROLLER_BUTTON_PADDLE1
#define SDL_GAMEPAD_BUTTON_LEFT_PADDLE1 SDL_CONTROLLER_BUTTON_PADDLE2
#define SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2 SDL_CONTROLLER_BUTTON_PADDLE3
#define SDL_GAMEPAD_BUTTON_LEFT_PADDLE2 SDL_CONTROLLER_BUTTON_PADDLE4
#define SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER SDL_CONTROLLER_BUTTON_RIGHTSHOULDER
#define SDL_GAMEPAD_BUTTON_RIGHT_STICK SDL_CONTROLLER_BUTTON_RIGHTSTICK
#define SDL_GAMEPAD_BUTTON_START SDL_CONTROLLER_BUTTON_START
#define SDL_GAMEPAD_BUTTON_TOUCHPAD SDL_CONTROLLER_BUTTON_TOUCHPAD
#define SDL_GAMEPAD_BUTTON_WEST SDL_CONTROLLER_BUTTON_X
#define SDL_GAMEPAD_BUTTON_NORTH SDL_CONTROLLER_BUTTON_Y

#define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT
#define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR
#define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT
#define SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO
#define SDL_GAMEPAD_TYPE_PS3 SDL_CONTROLLER_TYPE_PS3
#define SDL_GAMEPAD_TYPE_PS4 SDL_CONTROLLER_TYPE_PS4
#define SDL_GAMEPAD_TYPE_PS5 SDL_CONTROLLER_TYPE_PS5
#define SDL_GAMEPAD_TYPE_STANDARD SDL_CONTROLLER_TYPE_UNKNOWN
#define SDL_GAMEPAD_TYPE_VIRTUAL SDL_CONTROLLER_TYPE_VIRTUAL
#define SDL_GAMEPAD_TYPE_XBOX360 SDL_CONTROLLER_TYPE_XBOX360
#define SDL_GAMEPAD_TYPE_XBOXONE SDL_CONTROLLER_TYPE_XBOXONE

#define SDL_KMOD_ALT KMOD_ALT
#define SDL_KMOD_CAPS KMOD_CAPS
#define SDL_KMOD_CTRL KMOD_CTRL
#define SDL_KMOD_GUI KMOD_GUI
#define SDL_KMOD_LALT KMOD_LALT
#define SDL_KMOD_LCTRL KMOD_LCTRL
#define SDL_KMOD_LGUI KMOD_LGUI
#define SDL_KMOD_LSHIFT KMOD_LSHIFT
#define SDL_KMOD_MODE KMOD_MODE
#define SDL_KMOD_NONE KMOD_NONE
#define SDL_KMOD_NUM KMOD_NUM
#define SDL_KMOD_RALT KMOD_RALT
#define SDL_KMOD_RCTRL KMOD_RCTRL
#define SDL_KMOD_RGUI KMOD_RGUI
#define SDL_KMOD_RSHIFT KMOD_RSHIFT
#define SDL_KMOD_SCROLL KMOD_SCROLL
#define SDL_KMOD_SHIFT KMOD_SHIFT

#define SDLK_GRAVE SDLK_BACKQUOTE
#define SDLK_APOSTROPHE SDLK_QUOTE
#define SDLK_A SDLK_a
#define SDLK_B SDLK_b
#define SDLK_C SDLK_c
#define SDLK_D SDLK_d
#define SDLK_E SDLK_e
#define SDLK_F SDLK_f
#define SDLK_G SDLK_g
#define SDLK_H SDLK_h
#define SDLK_I SDLK_i
#define SDLK_J SDLK_j
#define SDLK_K SDLK_k
#define SDLK_L SDLK_l
#define SDLK_M SDLK_m
#define SDLK_N SDLK_n
#define SDLK_O SDLK_o
#define SDLK_P SDLK_p
#define SDLK_Q SDLK_q
#define SDLK_R SDLK_r
#define SDLK_S SDLK_s
#define SDLK_T SDLK_t
#define SDLK_U SDLK_u
#define SDLK_V SDLK_v
#define SDLK_W SDLK_w
#define SDLK_X SDLK_x
#define SDLK_Y SDLK_y
#define SDLK_Z SDLK_z

#define SDL_PIXELFORMAT_XBGR4444 SDL_PIXELFORMAT_BGR444
#define SDL_PIXELFORMAT_XBGR1555 SDL_PIXELFORMAT_BGR555
#define SDL_PIXELFORMAT_XBGR8888 SDL_PIXELFORMAT_BGR888
#define SDL_PIXELFORMAT_XRGB4444 SDL_PIXELFORMAT_RGB444
#define SDL_PIXELFORMAT_XRGB1555 SDL_PIXELFORMAT_RGB555
#define SDL_PIXELFORMAT_XRGB8888 SDL_PIXELFORMAT_RGB888

#define SDL_IO_SEEK_CUR RW_SEEK_CUR
#define SDL_IO_SEEK_END RW_SEEK_END
#define SDL_IO_SEEK_SET RW_SEEK_SET
#define SDL_IOFromConstMem SDL_RWFromConstMem
#define SDL_IOFromFile SDL_RWFromFile
#define SDL_IOFromMem SDL_RWFromMem
#define SDL_IOStream SDL_RWops
#define SDL_SeekIO SDL_RWseek
#define SDL_GetIOSize SDL_RWsize
#define SDL_TellIO SDL_RWtell

#define SDL_GetLogOutputFunction SDL_LogGetOutputFunction
#define SDL_GetLogPriority SDL_LogGetPriority
#define SDL_ResetLogPriorities SDL_LogResetPriorities
#define SDL_SetLogPriorities SDL_LogSetAllPriority
#define SDL_SetLogOutputFunction SDL_LogSetOutputFunction
#define SDL_SetLogPriority SDL_LogSetPriority
#define SDL_LOG_PRIORITY_COUNT SDL_NUM_LOG_PRIORITIES

#define SDL_ThreadID SDL_threadID

#define SDLC_SURFACE_BITSPERPIXEL(surface) surface->format->BitsPerPixel

inline bool SDLC_PushEvent(SDL_Event *event) { return SDL_PushEvent(event) == 1; }

inline
#ifdef USE_SDL1
    SDLKey
#else
    SDL_Keycode
#endif
    SDLC_EventKey(const SDL_Event &event)
{
	return event.key.keysym.sym;
}
inline SDL_Scancode SDLC_EventScancode(const SDL_Event &event) { return event.key.keysym.scancode; }

inline int SDLC_EventMotionIntX(const SDL_Event &event) { return event.motion.x; }
inline int SDLC_EventMotionIntY(const SDL_Event &event) { return event.motion.y; }
inline int SDLC_EventButtonIntX(const SDL_Event &event) { return event.button.x; }
inline int SDLC_EventButtonIntY(const SDL_Event &event) { return event.button.y; }

inline bool SDL_FillSurfaceRect(SDL_Surface *dst,
#ifdef USE_SDL1
    SDL_Rect *rect,
#else
    const SDL_Rect *rect,
#endif
    Uint32 color)
{
	return SDL_FillRect(dst, rect, color) == 0;
}

inline bool SDL_SetSurfaceClipRect(SDL_Surface *surface, const SDL_Rect *rect) { return SDL_SetClipRect(surface, rect) == SDL_TRUE; }
inline bool SDL_SetSurfaceColorKey(SDL_Surface *surface, bool enabled, Uint32 key)
{
	return SDL_SetColorKey(surface, enabled ? 1 : 0, key) == 0;
}

#ifndef USE_SDL1
inline bool SDLC_SetClipboardText(const char *text) { return SDL_SetClipboardText(text) == 0; }
inline bool SDLC_HasClipboardText() { return SDL_HasClipboardText() == SDL_TRUE; }

inline int SDLC_EventWheelIntX(const SDL_Event &event) { return event.wheel.x; }
inline int SDLC_EventWheelIntY(const SDL_Event &event) { return event.wheel.y; }

inline const SDL_GamepadAxisEvent &SDLC_EventGamepadAxis(const SDL_Event &event) { return event.caxis; }
inline const SDL_GamepadButtonEvent &SDLC_EventGamepadButton(const SDL_Event &event) { return event.cbutton; }
inline const SDL_GamepadDeviceEvent &SDLC_EventGamepadDevice(const SDL_Event &event) { return event.cdevice; }
inline bool SDL_GamepadHasButton(SDL_Gamepad *gamepad, SDL_GamepadButton button)
{
	return SDL_GameControllerHasButton(gamepad, button) == SDL_TRUE;
}
inline bool SDL_GetGamepadButton(SDL_Gamepad *gamepad, SDL_GamepadButton button)
{
	return SDL_GameControllerGetButton(gamepad, button) != 0;
}

inline bool SDL_GetRenderScale(SDL_Renderer *renderer, float *scaleX, float *scaleY)
{
	SDL_RenderGetScale(renderer, scaleX, scaleY);
	return true;
}

// For SDL1.2, these are defined in sdl2_to_1_2_backports.h
inline bool SDLC_StartTextInput([[maybe_unused]] SDL_Window *window)
{
	SDL_StartTextInput();
	return true;
}
inline bool SDLC_StopTextInput([[maybe_unused]] SDL_Window *window)
{
	SDL_StopTextInput();
	return true;
}
inline bool SDL_SetTextInputArea([[maybe_unused]] SDL_Window *window, SDL_Rect *rect, [[maybe_unused]] int cursor)
{
	SDL_SetTextInputRect(rect);
	return true;
}

inline bool SDLC_SetSurfacePalette(SDL_Surface *surface, SDL_Palette *palette)
{
	return SDL_SetSurfacePalette(surface, palette) == 0;
}

inline bool SDL_CursorVisible() { return SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE; }

// For SDL1.2, this is defined in sdl2_to_1_2_backports.h
inline bool SDLC_PointInRect(const SDL_Point *p, const SDL_Rect *r) { return SDL_PointInRect(p, r) == SDL_TRUE; }
#endif

inline bool SDLC_ShowCursor() { return SDL_ShowCursor(SDL_ENABLE) >= 0; }
inline bool SDLC_HideCursor() { return SDL_ShowCursor(SDL_DISABLE) >= 0; }

inline size_t SDL_ReadIO(SDL_IOStream *context, void *ptr, size_t size)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return SDL_RWread(context, ptr, 1, size);
#else
	return SDL_RWread(context, ptr, 1, static_cast<int>(size));
#endif
}
inline size_t SDL_WriteIO(SDL_IOStream *context, const void *ptr, size_t size)
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return SDL_RWwrite(context, ptr, 1, size);
#else
	return SDL_RWwrite(context, ptr, 1, static_cast<int>(size));
#endif
}

inline bool SDL_CloseIO(SDL_IOStream *iostr)
{
	return SDL_RWclose(iostr) == 0;
}

#define SDLC_ShowSimpleMessageBox(flags, title, message, window) SDL_ShowSimpleMessageBox(flags, title, message, window) == 0
#endif

#ifndef USE_SDL1
#define SDLC_KEYSTATE_LEFTCTRL SDL_SCANCODE_LCTRL
#define SDLC_KEYSTATE_RIGHTCTRL SDL_SCANCODE_RCTRL
#define SDLC_KEYSTATE_LEFTSHIFT SDL_SCANCODE_LSHIFT
#define SDLC_KEYSTATE_RIGHTSHIFT SDL_SCANCODE_RSHIFT
#define SDLC_KEYSTATE_LALT SDL_SCANCODE_LALT
#define SDLC_KEYSTATE_RALT SDL_SCANCODE_RALT
#define SDLC_KEYSTATE_UP SDL_SCANCODE_UP
#define SDLC_KEYSTATE_DOWN SDL_SCANCODE_DOWN
#define SDLC_KEYSTATE_LEFT SDL_SCANCODE_LEFT
#define SDLC_KEYSTATE_RIGHT SDL_SCANCODE_RIGHT
#else
#define SDLC_KEYSTATE_LEFTCTRL SDLK_LCTRL
#define SDLC_KEYSTATE_RIGHTCTRL SDLK_RCTRL
#define SDLC_KEYSTATE_LEFTSHIFT SDLK_LSHIFT
#define SDLC_KEYSTATE_RIGHTSHIFT SDLK_LSHIFT
#define SDLC_KEYSTATE_LALT SDLK_LALT
#define SDLC_KEYSTATE_RALT SDLK_RALT
#define SDLC_KEYSTATE_UP SDLK_UP
#define SDLC_KEYSTATE_DOWN SDLK_DOWN
#define SDLC_KEYSTATE_LEFT SDLK_LEFT
#define SDLC_KEYSTATE_RIGHT SDLK_RIGHT
#endif

#ifdef USE_SDL3
inline const bool *SDLC_GetKeyState()
#else
inline const Uint8 *SDLC_GetKeyState()
#endif
{
#ifndef USE_SDL1
	return SDL_GetKeyboardState(nullptr);
#else
	return SDL_GetKeyState(nullptr);
#endif
}

// Convert events to renderer coordinates.
// This is done automatically in SDL2 but not in SDL3 and SDL1.2.
inline bool SDLC_ConvertEventToRenderCoordinates(
#ifndef USE_SDL1
    SDL_Renderer *renderer,
#else
    void *,
#endif
    SDL_Event *event)
{
#ifdef USE_SDL3
	if (renderer != nullptr) {
		return SDL_ConvertEventToRenderCoordinates(renderer, event);
	}
#elif !defined(USE_SDL1)
	// No-op in SDL2.
#else
	if (event->type == SDL_MOUSEMOTION) {
		devilution::OutputToLogical(&event->motion.x, &event->motion.y);
	} else if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) {
		devilution::OutputToLogical(&event->button.x, &event->button.y);
	}
#endif
	return true;
}

// Sets the palette's colors and:
// SDL3 and SDL2: Points the surface's palette to the given palette if necessary.
// SDL1: Sets the surface's colors.
inline bool SDLC_SetSurfaceAndPaletteColors(SDL_Surface *surface, SDL_Palette *palette, SDL_Color *colors, int firstcolor, int ncolors)
{
#if defined(USE_SDL1)
	if (ncolors > (palette->ncolors - firstcolor)) {
		SDL_SetError("ncolors > (palette->ncolors - firstcolor)");
		return false;
	}
	if (colors != (palette->colors + firstcolor))
		SDL_memcpy(palette->colors + firstcolor, colors, ncolors * sizeof(*colors));

#if SDL1_VIDEO_MODE_BPP == 8
	// When the video surface is 8bit, we need to set the output palette as well.
	SDL_Surface *videoSurface = SDL_GetVideoSurface();
	SDL_SetColors(videoSurface, colors, firstcolor, ncolors);
	if (videoSurface == surface) return true;
#endif
	// In SDL1, the surface always has its own distinct palette, so we need to
	// update it as well.
	return SDL_SetPalette(surface, SDL_LOGPAL, colors, firstcolor, ncolors) != 0;
#elif defined(USE_SDL3)
	if (!SDL_SetPaletteColors(palette, colors, firstcolor, ncolors)) return false;
	if (SDL_GetSurfacePalette(surface) != palette) {
		if (!SDL_SetSurfacePalette(surface, palette)) return false;
	}
	return true;
#else
	if (SDL_SetPaletteColors(palette, colors, firstcolor, ncolors) < 0)
		return false;
	if (surface->format->palette != palette)
		return SDL_SetSurfacePalette(surface, palette) == 0;
	return true;
#endif
}
