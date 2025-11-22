#include "controls/touch/event_handlers.h"

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_version.h>
#else
#include <SDL.h>
#endif

#include "control.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "diablo.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/scrollrt.h"
#include "game_mode.hpp"
#include "gmenu.h"
#include "inv.h"
#include "panels/spell_book.hpp"
#include "panels/spell_list.hpp"
#include "qol/stash.h"
#include "stores.h"
#include "utils/is_of.hpp"
#include "utils/sdl_compat.h"
#include "utils/ui_fwd.h"

namespace devilution {

namespace {

#if SDL_VERSION_ATLEAST(2, 0, 0)
using SdlEventType = uint16_t;
#else
using SdlEventType = uint8_t;
#endif

VirtualGamepadEventHandler Handler(&VirtualGamepadState);

Point ScaleToScreenCoordinates(float x, float y)
{
	return Point {
		(int)round(x * gnScreenWidth),
		(int)round(y * gnScreenHeight)
	};
}

constexpr bool IsFingerDown(const SDL_Event &event)
{
	return event.type == SDL_EVENT_FINGER_DOWN;
}

constexpr bool IsFingerUp(const SDL_Event &event)
{
	return event.type == SDL_EVENT_FINGER_UP;
}

constexpr bool IsFingerMotion(const SDL_Event &event)
{
	return event.type == SDL_EVENT_FINGER_MOTION;
}

constexpr SDL_FingerID FingerId(const SDL_TouchFingerEvent &event)
{
#ifdef USE_SDL3
	return event.fingerID;
#else
	return event.fingerId;
#endif
}

void SimulateMouseMovement(const SDL_Event &event)
{
	const Point position = ScaleToScreenCoordinates(event.tfinger.x, event.tfinger.y);

	const bool isInMainPanel = GetMainPanel().contains(position);
	const bool isInLeftPanel = GetLeftPanel().contains(position);
	const bool isInRightPanel = GetRightPanel().contains(position);
	if (IsStashOpen) {
		if (!SpellSelectFlag && !isInMainPanel && !isInLeftPanel && !isInRightPanel)
			return;
	} else if (invflag) {
		if (!SpellSelectFlag && !isInMainPanel && !isInRightPanel)
			return;
	}

	MousePosition = position;

	SetPointAndClick(true);

	InvalidateInventorySlot();
}

bool HandleGameMenuInteraction(const SDL_Event &event)
{
	if (!gmenu_is_active())
		return false;
	if (IsFingerDown(event) && gmenu_left_mouse(true))
		return true;
	if (IsFingerMotion(event) && gmenu_on_mouse_move())
		return true;
	return IsFingerUp(event) && gmenu_left_mouse(false);
}

bool HandleStoreInteraction(const SDL_Event &event)
{
	if (!IsPlayerInStore())
		return false;
	if (IsFingerDown(event))
		CheckStoreBtn();
	return true;
}

void HandleSpellBookInteraction(const SDL_Event &event)
{
	if (!SpellbookFlag)
		return;

	if (IsFingerUp(event))
		CheckSBook();
}

bool HandleSpeedBookInteraction(const SDL_Event &event)
{
	if (!SpellSelectFlag)
		return false;
	if (IsFingerUp(event))
		SetSpell();
	return true;
}

void HandleBottomPanelInteraction(const SDL_Event &event)
{
	if (!gbRunGame || !MyPlayer->HoldItem.isEmpty())
		return;

	ResetMainPanelButtons();

	if (!IsFingerUp(event)) {
		SpellSelectFlag = true;
		CheckMainPanelButton();
		SpellSelectFlag = false;
	} else {
		CheckMainPanelButton();
		if (MainPanelButtonDown)
			CheckMainPanelButtonUp();
	}
}

void HandleCharacterPanelInteraction(const SDL_Event &event)
{
	if (!CharFlag)
		return;

	if (IsFingerDown(event))
		CheckChrBtns();
	else if (IsFingerUp(event) && CharPanelButtonActive)
		ReleaseChrBtns(false);
}

void HandleStashPanelInteraction(const SDL_Event &event)
{
	if (!IsStashOpen || !MyPlayer->HoldItem.isEmpty())
		return;

	if (!IsFingerUp(event)) {
		CheckStashButtonPress(MousePosition);
	} else {
		CheckStashButtonRelease(MousePosition);
	}
}

SdlEventType GetDeactivateEventType()
{
	static const SdlEventType customEventType = SDL_RegisterEvents(1);
	return customEventType;
}

bool IsDeactivateEvent(const SDL_Event &event)
{
	return event.type == GetDeactivateEventType();
}

} // namespace

void HandleTouchEvent(const SDL_Event &event)
{
	SetPointAndClick(false);

	if (Handler.Handle(event)) {
		return;
	}

	if (!IsFingerDown(event) && !IsFingerUp(event) && !IsFingerMotion(event)) {
		return;
	}

	SimulateMouseMovement(event);

	if (HandleGameMenuInteraction(event))
		return;

	if (HandleStoreInteraction(event))
		return;

	if (HandleSpeedBookInteraction(event))
		return;

	HandleSpellBookInteraction(event);
	HandleBottomPanelInteraction(event);
	HandleCharacterPanelInteraction(event);
	HandleStashPanelInteraction(event);
}

void DeactivateTouchEventHandlers()
{
	SDL_Event event;
	event.type = GetDeactivateEventType();
	HandleTouchEvent(event);
}

bool VirtualGamepadEventHandler::Handle(const SDL_Event &event)
{
	if (!IsDeactivateEvent(event)) {
		if (!VirtualGamepadState.isActive || (!IsFingerDown(event) && !IsFingerUp(event) && !IsFingerMotion(event))) {
			VirtualGamepadState.primaryActionButton.didStateChange = false;
			VirtualGamepadState.secondaryActionButton.didStateChange = false;
			VirtualGamepadState.spellActionButton.didStateChange = false;
			VirtualGamepadState.cancelButton.didStateChange = false;
			return false;
		}
	}

	if (charMenuButtonEventHandler.Handle(event))
		return true;

	if (questsMenuButtonEventHandler.Handle(event))
		return true;

	if (inventoryMenuButtonEventHandler.Handle(event))
		return true;

	if (mapMenuButtonEventHandler.Handle(event))
		return true;

	if (directionPadEventHandler.Handle(event))
		return true;

	if (leveltype != DTYPE_TOWN && standButtonEventHandler.Handle(event))
		return true;

	if (primaryActionButtonEventHandler.Handle(event))
		return true;

	if (secondaryActionButtonEventHandler.Handle(event))
		return true;

	if (spellActionButtonEventHandler.Handle(event))
		return true;

	if (cancelButtonEventHandler.Handle(event))
		return true;

	if (healthButtonEventHandler.Handle(event))
		return true;

	if (manaButtonEventHandler.Handle(event))
		return true;

	return false;
}

bool VirtualDirectionPadEventHandler::Handle(const SDL_Event &event)
{
	if (IsDeactivateEvent(event)) {
		isActive = false;
		return false;
	}

	if (IsFingerDown(event)) return HandleFingerDown(event.tfinger);
	if (IsFingerUp(event)) return HandleFingerUp(event.tfinger);
	if (IsFingerMotion(event)) return HandleFingerMotion(event.tfinger);
	return false;
}

bool VirtualDirectionPadEventHandler::HandleFingerDown(const SDL_TouchFingerEvent &event)
{
	if (isActive)
		return false;

	const float x = event.x;
	const float y = event.y;

	const Point touchCoordinates = ScaleToScreenCoordinates(x, y);
	if (!virtualDirectionPad->area.contains(touchCoordinates))
		return false;

	virtualDirectionPad->UpdatePosition(touchCoordinates);
	activeFinger = FingerId(event);
	isActive = true;
	return true;
}

bool VirtualDirectionPadEventHandler::HandleFingerUp(const SDL_TouchFingerEvent &event)
{
	if (!isActive || FingerId(event) != activeFinger)
		return false;

	const Point position = virtualDirectionPad->area.position;
	virtualDirectionPad->UpdatePosition(position);
	isActive = false;
	return true;
}

bool VirtualDirectionPadEventHandler::HandleFingerMotion(const SDL_TouchFingerEvent &event)
{
	if (!isActive || FingerId(event) != activeFinger)
		return false;

	const float x = event.x;
	const float y = event.y;

	const Point touchCoordinates = ScaleToScreenCoordinates(x, y);
	virtualDirectionPad->UpdatePosition(touchCoordinates);
	return true;
}

bool VirtualButtonEventHandler::Handle(const SDL_Event &event)
{
	if (IsDeactivateEvent(event)) {
		isActive = false;
		return false;
	}

	if (!virtualButton->isUsable()) {
		virtualButton->didStateChange = virtualButton->isHeld;
		virtualButton->isHeld = false;
		return false;
	}

	virtualButton->didStateChange = false;

	if (IsFingerDown(event)) return HandleFingerDown(event.tfinger);
	if (IsFingerUp(event)) return HandleFingerUp(event.tfinger);
	if (IsFingerMotion(event)) return HandleFingerMotion(event.tfinger);
	return false;
}

bool VirtualButtonEventHandler::HandleFingerDown(const SDL_TouchFingerEvent &event)
{
	if (isActive)
		return false;

	const float x = event.x;
	const float y = event.y;

	const Point touchCoordinates = ScaleToScreenCoordinates(x, y);
	if (!virtualButton->contains(touchCoordinates))
		return false;

	if (toggles)
		virtualButton->isHeld = !virtualButton->isHeld;
	else
		virtualButton->isHeld = true;

	virtualButton->didStateChange = true;
	activeFinger = FingerId(event);
	isActive = true;
	return true;
}

bool VirtualButtonEventHandler::HandleFingerUp(const SDL_TouchFingerEvent &event)
{
	if (!isActive || FingerId(event) != activeFinger)
		return false;

	if (!toggles) {
		if (virtualButton->isHeld)
			virtualButton->didStateChange = true;
		virtualButton->isHeld = false;
	}

	isActive = false;
	return true;
}

bool VirtualButtonEventHandler::HandleFingerMotion(const SDL_TouchFingerEvent &event)
{
	if (!isActive || FingerId(event) != activeFinger)
		return false;

	if (toggles)
		return true;

	const float x = event.x;
	const float y = event.y;
	const Point touchCoordinates = ScaleToScreenCoordinates(x, y);

	const bool wasHeld = virtualButton->isHeld;
	virtualButton->isHeld = virtualButton->contains(touchCoordinates);
	virtualButton->didStateChange = virtualButton->isHeld != wasHeld;

	return true;
}

} // namespace devilution
