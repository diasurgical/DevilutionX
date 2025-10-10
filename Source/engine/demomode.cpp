#include "engine/demomode.h"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <optional>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif
#endif

#include "controls/control_mode.hpp"
#include "controls/plrctrls.h"
#include "engine/events.hpp"
#include "game_mode.hpp"
#include "gmenu.h"
#include "headless_mode.hpp"
#include "menu.h"
#include "nthread.h"
#include "options.h"
#include "pfile.h"
#include "utils/console.h"
#include "utils/display.h"
#include "utils/endian_stream.hpp"
#include "utils/is_of.hpp"
#include "utils/paths.h"
#include "utils/str_cat.hpp"

namespace devilution {

// #define LOG_DEMOMODE_MESSAGES
// #define LOG_DEMOMODE_MESSAGES_MOUSEMOTION
// #define LOG_DEMOMODE_MESSAGES_RENDERING
// #define LOG_DEMOMODE_MESSAGES_GAMETICK

namespace {

constexpr uint8_t Version = 3;

enum class LoadingStatus : uint8_t {
	Success,
	FileNotFound,
	UnsupportedVersion,
};

struct MouseMotionEventData {
	uint16_t x;
	uint16_t y;
};

struct MouseButtonEventData {
	uint16_t x;
	uint16_t y;
	uint16_t mod;
	uint8_t button;
};

struct MouseWheelEventData {
	int16_t x;
	int16_t y;
	uint16_t mod;
};

struct KeyEventData {
	uint32_t sym;
	uint16_t mod;
};

struct DemoMsg {
	enum EventType : uint8_t {
		GameTick = 0,
		Rendering = 1,

		// Inputs:
		MinEvent = 8,

		QuitEvent = 8,
		MouseMotionEvent = 9,
		MouseButtonDownEvent = 10,
		MouseButtonUpEvent = 11,
		MouseWheelEvent = 12,
		KeyDownEvent = 13,
		KeyUpEvent = 14,

		MinCustomEvent = 64,
	};

	EventType type;
	uint8_t progressToNextGameTick;
	union {
		MouseMotionEventData motion;
		MouseButtonEventData button;
		MouseWheelEventData wheel;
		KeyEventData key;
	};

	[[nodiscard]] bool isEvent() const
	{
		return type >= MinEvent;
	}
};

FILE *DemoFile;
int DemoFileVersion;
int DemoNumber = -1;
std::optional<DemoMsg> CurrentDemoMessage;

bool Timedemo = false;
int RecordNumber = -1;
bool CreateDemoReference = false;

// These options affect gameplay and are stored in the demo file.
struct {
	uint8_t tickRate = 20;
	bool runInTown = false;
	bool theoQuest = false;
	bool cowQuest = false;
	bool autoGoldPickup = false;
	bool autoElixirPickup = false;
	bool autoOilPickup = false;
	bool autoPickupInTown = false;
	bool adriaRefillsMana = false;
	bool autoEquipWeapons = false;
	bool autoEquipArmor = false;
	bool autoEquipHelms = false;
	bool autoEquipShields = false;
	bool autoEquipJewelry = false;
	bool randomizeQuests = false;
	bool showItemLabels = false;
	bool autoRefillBelt = false;
	bool disableCripplingShrines = false;
	uint8_t numHealPotionPickup = 0;
	uint8_t numFullHealPotionPickup = 0;
	uint8_t numManaPotionPickup = 0;
	uint8_t numFullManaPotionPickup = 0;
	uint8_t numRejuPotionPickup = 0;
	uint8_t numFullRejuPotionPickup = 0;
} DemoSettings;

FILE *DemoRecording;
uint32_t DemoModeLastTick = 0;

int LogicTick = 0;
uint32_t StartTime = 0;

uint16_t DemoGraphicsWidth = 640;
uint16_t DemoGraphicsHeight = 480;

void ReadSettings(FILE *in, uint8_t version) // NOLINT(readability-identifier-length)
{
	DemoGraphicsWidth = ReadLE16(in);
	DemoGraphicsHeight = ReadLE16(in);
	if (version > 0) {
		DemoSettings.runInTown = ReadByte(in) != 0;
		DemoSettings.theoQuest = ReadByte(in) != 0;
		DemoSettings.cowQuest = ReadByte(in) != 0;
		DemoSettings.autoGoldPickup = ReadByte(in) != 0;
		DemoSettings.autoElixirPickup = ReadByte(in) != 0;
		DemoSettings.autoOilPickup = ReadByte(in) != 0;
		DemoSettings.autoPickupInTown = ReadByte(in) != 0;
		DemoSettings.adriaRefillsMana = ReadByte(in) != 0;
		DemoSettings.autoEquipWeapons = ReadByte(in) != 0;
		DemoSettings.autoEquipArmor = ReadByte(in) != 0;
		DemoSettings.autoEquipHelms = ReadByte(in) != 0;
		DemoSettings.autoEquipShields = ReadByte(in) != 0;
		DemoSettings.autoEquipJewelry = ReadByte(in) != 0;
		DemoSettings.randomizeQuests = ReadByte(in) != 0;
		DemoSettings.showItemLabels = ReadByte(in) != 0;
		DemoSettings.autoRefillBelt = ReadByte(in) != 0;
		DemoSettings.disableCripplingShrines = ReadByte(in) != 0;
		DemoSettings.numHealPotionPickup = ReadByte(in);
		DemoSettings.numFullHealPotionPickup = ReadByte(in);
		DemoSettings.numManaPotionPickup = ReadByte(in);
		DemoSettings.numFullManaPotionPickup = ReadByte(in);
		DemoSettings.numRejuPotionPickup = ReadByte(in);
		DemoSettings.numFullRejuPotionPickup = ReadByte(in);
	} else {
		DemoSettings = {};
	}

	std::string message = StrCat("⚙️\n", _("Resolution"), "=", DemoGraphicsWidth, "x", DemoGraphicsHeight);
	for (const auto &[key, value] : std::initializer_list<std::pair<std::string_view, bool>> {
	         { _("Run in Town"), DemoSettings.runInTown },
	         { _("Theo Quest"), DemoSettings.theoQuest },
	         { _("Cow Quest"), DemoSettings.cowQuest },
	         { _("Auto Gold Pickup"), DemoSettings.autoGoldPickup },
	         { _("Auto Elixir Pickup"), DemoSettings.autoGoldPickup },
	         { _("Auto Oil Pickup"), DemoSettings.autoOilPickup },
	         { _("Auto Pickup in Town"), DemoSettings.autoPickupInTown },
	         { _("Adria Refills Mana"), DemoSettings.adriaRefillsMana },
	         { _("Auto Equip Weapons"), DemoSettings.autoEquipWeapons },
	         { _("Auto Equip Armor"), DemoSettings.autoEquipArmor },
	         { _("Auto Equip Helms"), DemoSettings.autoEquipHelms },
	         { _("Auto Equip Shields"), DemoSettings.autoEquipShields },
	         { _("Auto Equip Jewelry"), DemoSettings.autoEquipJewelry },
	         { _("Randomize Quests"), DemoSettings.randomizeQuests },
	         { _("Show Item Labels"), DemoSettings.showItemLabels },
	         { _("Auto Refill Belt"), DemoSettings.autoRefillBelt },
	         { _("Disable Crippling Shrines"), DemoSettings.disableCripplingShrines } }) {
		StrAppend(message, "\n", key, "=", value ? "1" : "0");
	}
	for (const auto &[key, value] : std::initializer_list<std::pair<std::string_view, uint8_t>> {
	         { _("Heal Potion Pickup"), DemoSettings.numHealPotionPickup },
	         { _("Full Heal Potion Pickup"), DemoSettings.numFullHealPotionPickup },
	         { _("Mana Potion Pickup"), DemoSettings.numManaPotionPickup },
	         { _("Full Mana Potion Pickup"), DemoSettings.numFullManaPotionPickup },
	         { _("Rejuvenation Potion Pickup"), DemoSettings.numRejuPotionPickup },
	         { _("Full Rejuvenation Potion Pickup"), DemoSettings.numFullRejuPotionPickup } }) {
		StrAppend(message, "\n", key, "=", static_cast<int>(value));
	}
	Log("{}", message);
}

void WriteSettings(FILE *out)
{
	WriteLE16(out, gnScreenWidth);
	WriteLE16(out, gnScreenHeight);
	const Options &options = GetOptions();
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.runInTown));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.theoQuest));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.cowQuest));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.autoGoldPickup));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.autoElixirPickup));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.autoOilPickup));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.autoPickupInTown));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.adriaRefillsMana));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.autoEquipWeapons));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.autoEquipArmor));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.autoEquipHelms));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.autoEquipShields));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.autoEquipJewelry));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.randomizeQuests));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.showItemLabels));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.autoRefillBelt));
	WriteByte(out, static_cast<uint8_t>(*options.Gameplay.disableCripplingShrines));
	WriteByte(out, *options.Gameplay.numHealPotionPickup);
	WriteByte(out, *options.Gameplay.numFullHealPotionPickup);
	WriteByte(out, *options.Gameplay.numManaPotionPickup);
	WriteByte(out, *options.Gameplay.numFullManaPotionPickup);
	WriteByte(out, *options.Gameplay.numRejuPotionPickup);
	WriteByte(out, *options.Gameplay.numFullRejuPotionPickup);
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
bool CreateSdlEvent(const DemoMsg &dmsg, SDL_Event &event, uint16_t &modState)
{
	const uint8_t type = dmsg.type;
	switch (type) {
	case DemoMsg::MouseMotionEvent:
#ifdef USE_SDL3
		event.type = SDL_EVENT_MOUSE_MOTION;
		event.motion.state = 0;
		event.motion.xrel = 0.F;
		event.motion.yrel = 0.F;
#else
		event.type = SDL_MOUSEMOTION;
#endif
		event.motion.which = 0;
		event.motion.x = dmsg.motion.x;
		event.motion.y = dmsg.motion.y;
		return true;
	case DemoMsg::MouseButtonDownEvent:
	case DemoMsg::MouseButtonUpEvent:
#ifdef USE_SDL3
		event.type = type == DemoMsg::MouseButtonDownEvent ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
		event.button.down = type == DemoMsg::MouseButtonDownEvent;
		event.button.clicks = 1;
#else
		event.type = type == DemoMsg::MouseButtonDownEvent ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
		event.button.state = type == DemoMsg::MouseButtonDownEvent ? SDL_PRESSED : SDL_RELEASED;
#endif
		event.button.which = 0;
		event.button.button = dmsg.button.button;
		event.button.x = dmsg.button.x;
		event.button.y = dmsg.button.y;
		modState = dmsg.button.mod;
		return true;
	case DemoMsg::MouseWheelEvent:
#ifdef USE_SDL3
		event.type = SDL_EVENT_MOUSE_WHEEL;
#if SDL_VERSION_ATLEAST(3, 2, 12)
		event.wheel.integer_x = dmsg.wheel.x;
		event.wheel.integer_y = dmsg.wheel.y;
#else
		event.wheel.x = dmsg.wheel.x;
		event.wheel.y = dmsg.wheel.y;
#endif
		event.wheel.mouse_x = 0;
		event.wheel.mouse_y = 0;
#else
		event.type = SDL_MOUSEWHEEL;
#endif
		event.wheel.which = 0;
		event.wheel.x = dmsg.wheel.x;
		event.wheel.y = dmsg.wheel.y;
		modState = dmsg.wheel.mod;
		return true;
	case DemoMsg::KeyDownEvent:
	case DemoMsg::KeyUpEvent:
#ifdef USE_SDL3
		event.type = type == DemoMsg::KeyDownEvent ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
		event.key.down = type == DemoMsg::KeyDownEvent;
		event.key.scancode = SDL_GetScancodeFromKey(dmsg.key.sym, nullptr);
		event.key.key = dmsg.key.sym;
		event.key.mod = dmsg.key.mod;
#else
		event.type = type == DemoMsg::KeyDownEvent ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.state = type == DemoMsg::KeyDownEvent ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.scancode = SDL_GetScancodeFromKey(dmsg.key.sym);
		event.key.keysym.sym = dmsg.key.sym;
		event.key.keysym.mod = dmsg.key.mod;
#endif
		return true;
	default:
		if (type >= DemoMsg::MinCustomEvent) {
			CustomEventToSdlEvent(event, static_cast<interface_mode>(type - DemoMsg::MinCustomEvent));
			return true;
		}
		event.type = static_cast<SDL_EventType>(0);
		LogWarn("Unsupported demo event (type={})", type);
		return false;
	}
}
#else
SDLKey Sdl2ToSdl1Key(uint32_t key)
{
	if ((key & (1 << 30)) != 0) {
		constexpr uint32_t Keys1Start = 57;
		constexpr SDLKey Keys1[] {
			SDLK_CAPSLOCK, SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6,
			SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_F11, SDLK_F12,
			SDLK_PRINTSCREEN, SDLK_SCROLLLOCK, SDLK_PAUSE, SDLK_INSERT, SDLK_HOME,
			SDLK_PAGEUP, SDLK_DELETE, SDLK_END, SDLK_PAGEDOWN, SDLK_RIGHT, SDLK_LEFT,
			SDLK_DOWN, SDLK_UP, SDLK_NUMLOCKCLEAR, SDLK_KP_DIVIDE, SDLK_KP_MULTIPLY,
			SDLK_KP_MINUS, SDLK_KP_PLUS, SDLK_KP_ENTER, SDLK_KP_1, SDLK_KP_2,
			SDLK_KP_3, SDLK_KP_4, SDLK_KP_5, SDLK_KP_6, SDLK_KP_7, SDLK_KP_8,
			SDLK_KP_9, SDLK_KP_0, SDLK_KP_PERIOD
		};
		constexpr uint32_t Keys2Start = 224;
		constexpr SDLKey Keys2[] {
			SDLK_LCTRL, SDLK_LSHIFT, SDLK_LALT, SDLK_LGUI, SDLK_RCTRL, SDLK_RSHIFT,
			SDLK_RALT, SDLK_RGUI, SDLK_MODE
		};
		const uint32_t scancode = key & ~(1 << 30);
		if (scancode >= Keys1Start) {
			if (scancode < Keys1Start + sizeof(Keys1) / sizeof(Keys1[0]))
				return Keys1[scancode - Keys1Start];
			if (scancode >= Keys2Start && scancode < Keys2Start + sizeof(Keys2) / sizeof(Keys2[0]))
				return Keys2[scancode - Keys2Start];
		}
		LogWarn("Demo: unknown key {:d}", key);
		return SDLK_UNKNOWN;
	}
	if (key <= 122) {
		return static_cast<SDLKey>(key);
	}
	LogWarn("Demo: unknown key {:d}", key);
	return SDLK_UNKNOWN;
}

uint8_t Sdl2ToSdl1MouseButton(uint8_t button)
{
	switch (button) {
	case 4:
		return SDL_BUTTON_X1;
	case 5:
		return SDL_BUTTON_X2;
	default:
		return button;
	}
}

bool CreateSdlEvent(const DemoMsg &dmsg, SDL_Event &event, uint16_t &modState)
{
	const uint8_t type = dmsg.type;
	switch (type) {
	case DemoMsg::MouseMotionEvent:
		event.type = SDL_MOUSEMOTION;
		event.motion.which = 0;
		event.motion.x = dmsg.motion.x;
		event.motion.y = dmsg.motion.y;
		return true;
	case DemoMsg::MouseButtonDownEvent:
	case DemoMsg::MouseButtonUpEvent:
		event.type = type == DemoMsg::MouseButtonDownEvent ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
		event.button.which = 0;
		event.button.button = Sdl2ToSdl1MouseButton(dmsg.button.button);
		event.button.state = type == DemoMsg::MouseButtonDownEvent ? SDL_PRESSED : SDL_RELEASED;
		event.button.x = dmsg.button.x;
		event.button.y = dmsg.button.y;
		modState = dmsg.button.mod;
		return true;
	case DemoMsg::MouseWheelEvent:
		if (dmsg.wheel.y == 0) {
			LogWarn("Demo: unsupported event (mouse wheel y == 0)");
			return false;
		}
		event.type = SDL_MOUSEBUTTONDOWN;
		event.button.which = 0;
		event.button.button = dmsg.wheel.y > 0 ? SDL_BUTTON_WHEELUP : SDL_BUTTON_WHEELDOWN;
		modState = dmsg.wheel.mod;
		return true;
	case DemoMsg::KeyDownEvent:
	case DemoMsg::KeyUpEvent:
		event.type = type == DemoMsg::KeyDownEvent ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.which = 0;
		event.key.state = type == DemoMsg::KeyDownEvent ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.sym = Sdl2ToSdl1Key(dmsg.key.sym);
		event.key.keysym.mod = static_cast<SDL_Keymod>(dmsg.key.mod);
		return true;
	default:
		if (type >= DemoMsg::MinCustomEvent) {
			CustomEventToSdlEvent(event, static_cast<interface_mode>(type - DemoMsg::MinCustomEvent));
			return true;
		}
		event.type = static_cast<SDL_EventType>(0);
		LogWarn("Demo: unsupported event (type={:x})", type);
		return false;
	}
}
#endif

uint8_t MapPreV2DemoMsgEventType(uint16_t type)
{
	switch (type) {
	case 0x100:
		return DemoMsg::QuitEvent;
	case 0x300:
		return DemoMsg::KeyDownEvent;
	case 0x301:
		return DemoMsg::KeyUpEvent;
	case 0x400:
		return DemoMsg::MouseMotionEvent;
	case 0x401:
		return DemoMsg::MouseButtonDownEvent;
	case 0x402:
		return DemoMsg::MouseButtonUpEvent;
	case 0x403:
		return DemoMsg::MouseWheelEvent;

	default:
		if (type < 0x8000) { // SDL_USEREVENT
			app_fatal(StrCat("Unknown event ", type));
		}
		return DemoMsg::MinCustomEvent + (type - 0x8000);
	}
}

void LogDemoMessage(const DemoMsg &dmsg)
{
#ifdef LOG_DEMOMODE_MESSAGES
	const uint8_t progressToNextGameTick = dmsg.progressToNextGameTick;
	switch (dmsg.type) {
	case DemoMsg::GameTick:
#ifdef LOG_DEMOMODE_MESSAGES_GAMETICK
		Log("⏲️  GameTick {:>3}", progressToNextGameTick);
#endif
		break;
	case DemoMsg::Rendering:
#ifdef LOG_DEMOMODE_MESSAGES_RENDERING
		Log("🖼️  Rendering {:>3}", progressToNextGameTick);
#endif
		break;
	case DemoMsg::MouseMotionEvent:
#ifdef LOG_DEMOMODE_MESSAGES_MOUSEMOTION
		Log("🖱️  Message {:>3} MOUSEMOTION {} {}", progressToNextGameTick,
		    dmsg.motion.x, dmsg.motion.y);
#endif
		break;
	case DemoMsg::MouseButtonDownEvent:
	case DemoMsg::MouseButtonUpEvent:
		Log("🖱️  Message {:>3} {} {} {} {} 0x{:x}", progressToNextGameTick,
		    dmsg.type == DemoMsg::MouseButtonDownEvent ? "MOUSEBUTTONDOWN" : "MOUSEBUTTONUP",
		    dmsg.button.button, dmsg.button.x, dmsg.button.y, dmsg.button.mod);
		break;
	case DemoMsg::MouseWheelEvent:
		Log("🖱️  Message {:>3} MOUSEWHEEL {} {} 0x{:x}", progressToNextGameTick,
		    dmsg.wheel.x, dmsg.wheel.y, dmsg.wheel.mod);
		break;
	case DemoMsg::KeyDownEvent:
	case DemoMsg::KeyUpEvent:
		Log("🔤 Message {:>3} {} 0x{:x} 0x{:x}", progressToNextGameTick,
		    dmsg.type == DemoMsg::KeyDownEvent ? "KEYDOWN" : "KEYUP",
		    dmsg.key.sym, dmsg.key.mod);
		break;
	case DemoMsg::QuitEvent:
		Log("❎  Message {:>3} QUIT", progressToNextGameTick);
		break;
	default:
		Log("📨  Message {:>3} USEREVENT {}", progressToNextGameTick, static_cast<uint8_t>(dmsg.type));
		break;
	}
#endif // LOG_DEMOMODE_MESSAGES
}

void CloseDemoFile()
{
	if (DemoFile != nullptr) {
		std::fclose(DemoFile);
		DemoFile = nullptr;
	}
}

LoadingStatus OpenDemoFile(int demoNumber)
{
	CloseDemoFile();
	const std::string path = StrCat(paths::PrefPath(), "demo_", demoNumber, ".dmo");
	DemoFile = OpenFile(path.c_str(), "rb");
	if (DemoFile == nullptr) {
		return LoadingStatus::FileNotFound;
	}
	DemoFileVersion = ReadByte(DemoFile);
	if (DemoFileVersion > Version) {
		return LoadingStatus::UnsupportedVersion;
	}
	DemoNumber = demoNumber;

	gSaveNumber = ReadLE32(DemoFile);
	ReadSettings(DemoFile, DemoFileVersion);

	return LoadingStatus::Success;
}

std::optional<DemoMsg> ReadDemoMessage()
{
	const uint8_t typeNum = DemoFileVersion >= 2 ? ReadByte(DemoFile) : ReadLE32(DemoFile);

	if (std::feof(DemoFile) != 0) {
		CloseDemoFile();
		return std::nullopt;
	}

	// Events with the high bit 1 are Rendering events with the rest of the bits used
	// to encode `progressToNextGameTick` inline.
	if ((typeNum & 0b10000000) != 0) {
		DemoModeLastTick = SDL_GetTicks();
		return DemoMsg { DemoMsg::Rendering, static_cast<uint8_t>(typeNum & 0b01111111u), {} };
	}
	const uint8_t progressToNextGameTick = ReadByte(DemoFile);

	switch (typeNum) {
	case DemoMsg::GameTick:
	case DemoMsg::Rendering:
		DemoModeLastTick = SDL_GetTicks();
		return DemoMsg { static_cast<DemoMsg::EventType>(typeNum), progressToNextGameTick, {} };
	default: {
		const uint8_t eventType = DemoFileVersion >= 2 ? typeNum : MapPreV2DemoMsgEventType(static_cast<uint16_t>(ReadLE32(DemoFile)));
		DemoMsg result { static_cast<DemoMsg::EventType>(eventType), progressToNextGameTick, {} };
		switch (eventType) {
		case DemoMsg::MouseMotionEvent: {
			result.motion.x = ReadLE16(DemoFile);
			result.motion.y = ReadLE16(DemoFile);
		} break;
		case DemoMsg::MouseButtonDownEvent:
		case DemoMsg::MouseButtonUpEvent: {
			result.button.button = ReadByte(DemoFile);
			result.button.x = ReadLE16(DemoFile);
			result.button.y = ReadLE16(DemoFile);
			result.button.mod = ReadLE16(DemoFile);
		} break;
		case DemoMsg::MouseWheelEvent: {
			result.wheel.x = DemoFileVersion >= 2 ? ReadLE16<int16_t>(DemoFile) : static_cast<int16_t>(ReadLE32<int32_t>(DemoFile));
			result.wheel.y = DemoFileVersion >= 2 ? ReadLE16<int16_t>(DemoFile) : static_cast<int16_t>(ReadLE32<int32_t>(DemoFile));
			result.wheel.mod = ReadLE16(DemoFile);
		} break;
		case DemoMsg::KeyDownEvent:
		case DemoMsg::KeyUpEvent: {
			result.key.sym = static_cast<SDL_Keycode>(ReadLE32(DemoFile));
			result.key.mod = static_cast<SDL_Keymod>(ReadLE16(DemoFile));
		} break;
		case DemoMsg::QuitEvent: // SDL_QUIT
			break;
		default:
			if (eventType < DemoMsg::MinCustomEvent) {
				app_fatal(StrCat("Unknown event ", eventType));
			}
			break;
		}
		DemoModeLastTick = SDL_GetTicks();
		return result;
	} break;
	}
}

void WriteDemoMsgHeader(DemoMsg::EventType type)
{
	if (type == DemoMsg::Rendering && ProgressToNextGameTick <= 127) {
		WriteByte(DemoRecording, ProgressToNextGameTick | 0b10000000);
		return;
	}
	WriteByte(DemoRecording, type);
	WriteByte(DemoRecording, ProgressToNextGameTick);
}

} // namespace

namespace demo {

void InitPlayBack(int demoNumber, bool timedemo)
{
	Timedemo = timedemo;
	ControlMode = ControlTypes::KeyboardAndMouse;

	const LoadingStatus status = OpenDemoFile(demoNumber);
	switch (status) {
	case LoadingStatus::Success:
		return;
	case LoadingStatus::FileNotFound:
		printInConsole("Demo file not found");
		break;
	case LoadingStatus::UnsupportedVersion:
		printInConsole("Unsupported Demo version");
		break;
	}
	printNewlineInConsole();
	diablo_quit(1);
}

void InitRecording(int recordNumber, bool createDemoReference)
{
	RecordNumber = recordNumber;
	CreateDemoReference = createDemoReference;
}
void OverrideOptions()
{
#ifndef USE_SDL1
	GetOptions().Graphics.fitToScreen.SetValue(false);
#endif
#if SDL_VERSION_ATLEAST(2, 0, 0)
	GetOptions().Graphics.hardwareCursor.SetValue(false);
#endif
	if (Timedemo) {
		GetOptions().Graphics.frameRateControl.SetValue(FrameRateControl::None);
	}
	forceResolution = Size(DemoGraphicsWidth, DemoGraphicsHeight);

	Options &options = GetOptions();
	options.Gameplay.runInTown.SetValue(DemoSettings.runInTown);
	options.Gameplay.theoQuest.SetValue(DemoSettings.theoQuest);
	options.Gameplay.cowQuest.SetValue(DemoSettings.cowQuest);
	options.Gameplay.autoGoldPickup.SetValue(DemoSettings.autoGoldPickup);
	options.Gameplay.autoElixirPickup.SetValue(DemoSettings.autoElixirPickup);
	options.Gameplay.autoOilPickup.SetValue(DemoSettings.autoOilPickup);
	options.Gameplay.autoPickupInTown.SetValue(DemoSettings.autoPickupInTown);
	options.Gameplay.adriaRefillsMana.SetValue(DemoSettings.adriaRefillsMana);
	options.Gameplay.autoEquipWeapons.SetValue(DemoSettings.autoEquipWeapons);
	options.Gameplay.autoEquipArmor.SetValue(DemoSettings.autoEquipArmor);
	options.Gameplay.autoEquipHelms.SetValue(DemoSettings.autoEquipHelms);
	options.Gameplay.autoEquipShields.SetValue(DemoSettings.autoEquipShields);
	options.Gameplay.autoEquipJewelry.SetValue(DemoSettings.autoEquipJewelry);
	options.Gameplay.randomizeQuests.SetValue(DemoSettings.randomizeQuests);
	options.Gameplay.showItemLabels.SetValue(DemoSettings.showItemLabels);
	options.Gameplay.autoRefillBelt.SetValue(DemoSettings.autoRefillBelt);
	options.Gameplay.disableCripplingShrines.SetValue(DemoSettings.disableCripplingShrines);
	options.Gameplay.numHealPotionPickup.SetValue(DemoSettings.numHealPotionPickup);
	options.Gameplay.numFullHealPotionPickup.SetValue(DemoSettings.numFullHealPotionPickup);
	options.Gameplay.numManaPotionPickup.SetValue(DemoSettings.numManaPotionPickup);
	options.Gameplay.numFullManaPotionPickup.SetValue(DemoSettings.numFullManaPotionPickup);
	options.Gameplay.numRejuPotionPickup.SetValue(DemoSettings.numRejuPotionPickup);
	options.Gameplay.numFullRejuPotionPickup.SetValue(DemoSettings.numFullRejuPotionPickup);
}

bool IsRunning()
{
	return DemoNumber != -1;
}

bool IsRecording()
{
	return RecordNumber != -1;
}

bool GetRunGameLoop(bool &drawGame, bool &processInput)
{
	if (CurrentDemoMessage == std::nullopt && DemoFile != nullptr)
		CurrentDemoMessage = ReadDemoMessage();
	if (CurrentDemoMessage == std::nullopt)
		app_fatal("Demo queue empty");

	const DemoMsg &dmsg = *CurrentDemoMessage;

	if (CurrentDemoMessage->isEvent())
		app_fatal("Unexpected event demo message in GetRunGameLoop");
	LogDemoMessage(dmsg);
	if (Timedemo) {
		// disable additional rendering to speedup replay
		drawGame = dmsg.type == DemoMsg::GameTick && !HeadlessMode;
	} else {
		const int currentTickCount = SDL_GetTicks();
		const int ticksElapsed = currentTickCount - DemoModeLastTick;
		const bool tickDue = ticksElapsed >= gnTickDelay;
		drawGame = false;
		if (tickDue) {
			if (dmsg.type == DemoMsg::GameTick) {
				DemoModeLastTick = currentTickCount;
			}
		} else {
			int32_t fraction = ticksElapsed * AnimationInfo::baseValueFraction / gnTickDelay;
			fraction = std::clamp<int32_t>(fraction, 0, AnimationInfo::baseValueFraction);
			const uint8_t progressToNextGameTick = static_cast<uint8_t>(fraction);
			if (dmsg.type == DemoMsg::GameTick || dmsg.progressToNextGameTick > progressToNextGameTick) {
				// we are ahead of the replay => add a additional rendering for smoothness
				if (gbRunGame && PauseMode == 0 && (gbIsMultiplayer || !gmenu_is_active()) && gbProcessPlayers) // if game is not running or paused there is no next gametick in the near future
					ProgressToNextGameTick = progressToNextGameTick;
				processInput = false;
				drawGame = true;
				return false;
			}
		}
	}
	ProgressToNextGameTick = dmsg.progressToNextGameTick;
	const bool isGameTick = dmsg.type == DemoMsg::GameTick;
	CurrentDemoMessage = std::nullopt;
	if (isGameTick)
		LogicTick++;
	return isGameTick;
}

bool FetchMessage(SDL_Event *event, uint16_t *modState)
{
	if (CurrentEventHandler == DisableInputEventHandler)
		return false;

	SDL_Event e;
	if (
#ifdef USE_SDL3
	    SDL_PollEvent(&e)
#else
	    SDL_PollEvent(&e) != 0
#endif
	) {
		if (e.type ==
#ifdef USE_SDL3
		    SDL_EVENT_QUIT
#else
		    SDL_QUIT
#endif
		) {
			*event = e;
			return true;
		}
		if (e.type ==
#ifdef USE_SDL3
		    SDL_EVENT_KEY_DOWN
#else
		    SDL_KEYDOWN
#endif
		) {
			const SDL_Keycode key =
#ifdef USE_SDL3
			    e.key.key;
#else
			    e.key.keysym.sym;
#endif
			if (key == SDLK_ESCAPE) {
				CloseDemoFile();
				CurrentDemoMessage = std::nullopt;
				DemoNumber = -1;
				Timedemo = false;
				last_tick = SDL_GetTicks();
			} else if (IsAnyOf(key, SDLK_KP_PLUS, SDLK_PLUS) && sgGameInitInfo.nTickRate < 255) {
				sgGameInitInfo.nTickRate++;
				GetOptions().Gameplay.tickRate.SetValue(sgGameInitInfo.nTickRate);
				gnTickDelay = 1000 / sgGameInitInfo.nTickRate;
			} else if (IsAnyOf(key, SDLK_KP_MINUS, SDLK_MINUS) && sgGameInitInfo.nTickRate > 1) {
				sgGameInitInfo.nTickRate--;
				GetOptions().Gameplay.tickRate.SetValue(sgGameInitInfo.nTickRate);
				gnTickDelay = 1000 / sgGameInitInfo.nTickRate;
			}
		}
	}

	if (CurrentDemoMessage == std::nullopt && DemoFile != nullptr)
		CurrentDemoMessage = ReadDemoMessage();
	if (CurrentDemoMessage != std::nullopt) {
		const DemoMsg &dmsg = *CurrentDemoMessage;
		LogDemoMessage(dmsg);
		if (dmsg.isEvent()) {
			const bool hasEvent = CreateSdlEvent(dmsg, *event, *modState);
			ProgressToNextGameTick = dmsg.progressToNextGameTick;
			CurrentDemoMessage = std::nullopt;
			return hasEvent;
		}
	}

	return false;
}

void RecordGameLoopResult(bool runGameLoop)
{
	WriteDemoMsgHeader(runGameLoop ? DemoMsg::GameTick : DemoMsg::Rendering);

	if (runGameLoop && !IsRunning())
		LogicTick++;
}

void RecordMessage(const SDL_Event &event, uint16_t modState)
{
	if (!gbRunGame || DemoRecording == nullptr)
		return;
	if (CurrentEventHandler == DisableInputEventHandler)
		return;
	switch (event.type) {
#ifdef USE_SDL3
	case SDL_EVENT_MOUSE_MOTION:
#else
	case SDL_MOUSEMOTION:
#endif
		WriteDemoMsgHeader(DemoMsg::MouseMotionEvent);
		WriteLE16(DemoRecording, event.motion.x);
		WriteLE16(DemoRecording, event.motion.y);
		break;
#ifdef USE_SDL3
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
#else
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
#endif
#ifdef USE_SDL1
		if (event.button.button == SDL_BUTTON_WHEELUP || event.button.button == SDL_BUTTON_WHEELDOWN) {
			WriteDemoMsgHeader(DemoMsg::MouseWheelEvent);
			WriteLE16(DemoRecording, 0);
			WriteLE16(DemoRecording, event.button.button == SDL_BUTTON_WHEELUP ? 1 : -1);
			WriteLE16(DemoRecording, modState);
		} else {
#endif
			WriteDemoMsgHeader(
#ifdef USE_SDL3
			    event.button.down
#else
		    event.type == SDL_MOUSEBUTTONDOWN
#endif
			        ? DemoMsg::MouseButtonDownEvent
			        : DemoMsg::MouseButtonUpEvent);
			WriteByte(DemoRecording, event.button.button);
			WriteLE16(DemoRecording, event.button.x);
			WriteLE16(DemoRecording, event.button.y);
			WriteLE16(DemoRecording, modState);
#ifdef USE_SDL1
		}
#endif
		break;
#ifndef USE_SDL1
#ifdef USE_SDL3
	case SDL_EVENT_MOUSE_WHEEL:
#else
	case SDL_MOUSEWHEEL:
#endif
		WriteDemoMsgHeader(DemoMsg::MouseWheelEvent);

#ifdef USE_SDL3
		int wheelX, wheelY;
#if SDL_VERSION_ATLEAST(3, 2, 12)
		wheelX = event.wheel.integer_x;
		wheelY = event.wheel.integer_y;
#else
		wheelX = event.wheel.x;
		wheelY = event.wheel.y;
#endif
		if (wheelX < std::numeric_limits<int16_t>::min()
		    || wheelX > std::numeric_limits<int16_t>::max()
		    || wheelY < std::numeric_limits<int16_t>::min()
		    || wheelY > std::numeric_limits<int16_t>::max()) {
			app_fatal(StrCat("Mouse wheel event integer_x/y out of int16_t range. x=",
			    wheelX, " y=", wheelY));
		}
		WriteLE16(DemoRecording, wheelX);
		WriteLE16(DemoRecording, wheelY);
#else
		if (event.wheel.x < std::numeric_limits<int16_t>::min()
		    || event.wheel.x > std::numeric_limits<int16_t>::max()
		    || event.wheel.y < std::numeric_limits<int16_t>::min()
		    || event.wheel.y > std::numeric_limits<int16_t>::max()) {
			app_fatal(StrCat("Mouse wheel event x/y out of int16_t range. x=",
			    event.wheel.x, " y=", event.wheel.y));
		}
		WriteLE16(DemoRecording, event.wheel.x);
		WriteLE16(DemoRecording, event.wheel.y);
#endif
		WriteLE16(DemoRecording, modState);
		break;
#endif
#ifdef USE_SDL3
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
		WriteDemoMsgHeader(event.key.down ? DemoMsg::KeyDownEvent : DemoMsg::KeyUpEvent);
		WriteLE32(DemoRecording, static_cast<uint32_t>(event.key.key));
		WriteLE16(DemoRecording, static_cast<uint16_t>(event.key.mod));
		break;
#else
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		WriteDemoMsgHeader(event.type == SDL_KEYDOWN ? DemoMsg::KeyDownEvent : DemoMsg::KeyUpEvent);
		WriteLE32(DemoRecording, static_cast<uint32_t>(event.key.keysym.sym));
		WriteLE16(DemoRecording, static_cast<uint16_t>(event.key.keysym.mod));
		break;
#endif
#ifndef USE_SDL1
#ifndef USE_SDL3
	case SDL_WINDOWEVENT:
		if (event.window.type == SDL_WINDOWEVENT_CLOSE) {
			WriteDemoMsgHeader(DemoMsg::QuitEvent);
		}
		break;
#endif
#endif
#ifdef USE_SDL3
	case SDL_EVENT_QUIT:
#else
	case SDL_QUIT:
#endif
		WriteDemoMsgHeader(DemoMsg::QuitEvent);
		break;
	default:
		if (IsCustomEvent(event.type)) {
			WriteDemoMsgHeader(static_cast<DemoMsg::EventType>(
			    DemoMsg::MinCustomEvent + static_cast<uint8_t>(GetCustomEvent(event))));
		}
		break;
	}
}

void NotifyGameLoopStart()
{
	LogicTick = 0;

	if (IsRunning()) {
		StartTime = SDL_GetTicks();
	}

	if (IsRecording()) {
		const std::string path = StrCat(paths::PrefPath(), "demo_", RecordNumber, ".dmo");
		DemoRecording = OpenFile(path.c_str(), "wb");
		if (DemoRecording == nullptr) {
			RecordNumber = -1;
			LogError("Failed to open {} for writing", path);
			return;
		}
		WriteByte(DemoRecording, Version);
		WriteLE32(DemoRecording, gSaveNumber);
		WriteSettings(DemoRecording);
	}
}

void NotifyGameLoopEnd()
{
	if (IsRecording()) {
		std::fclose(DemoRecording);
		DemoRecording = nullptr;
		if (CreateDemoReference)
			pfile_write_hero_demo(RecordNumber);

		RecordNumber = -1;
		CreateDemoReference = false;
	}

	if (IsRunning() && !HeadlessMode) {
		const float seconds = (SDL_GetTicks() - StartTime) / 1000.0F;
		Log("{} frames, {:.2f} seconds: {:.1f} fps", LogicTick, seconds, LogicTick / seconds);
		gbRunGameResult = false;
		gbRunGame = false;

		HeroCompareResult compareResult = pfile_compare_hero_demo(DemoNumber, false);
		switch (compareResult.status) {
		case HeroCompareResult::ReferenceNotFound:
			Log("Timedemo: No final comparison because reference is not present.");
			break;
		case HeroCompareResult::Same:
			Log("Timedemo: Same outcome as initial run. :)");
			break;
		case HeroCompareResult::Difference:
			Log("Timedemo: Different outcome than initial run. ;(\n{}", compareResult.message);
			break;
		}
	}
}

uint32_t SimulateMillisecondsSinceStartup()
{
	return LogicTick * 50;
}

} // namespace demo

} // namespace devilution
