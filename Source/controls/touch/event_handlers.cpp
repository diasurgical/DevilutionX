#include "controls/touch/event_handlers.h"

#include "control.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "diablo.h"
#include "engine/render/primitive_render.hpp"
#include "engine/render/scrollrt.h"
#include "gmenu.h"
#include "inv.h"
#include "panels/spell_book.hpp"
#include "panels/spell_list.hpp"
#include "qol/stash.h"
#include "stores.h"
#include "utils/is_of.hpp"
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

void SimulateMouseMovement(const SDL_Event &event)
{
	Point position = ScaleToScreenCoordinates(event.tfinger.x, event.tfinger.y);

	bool isInMainPanel = GetMainPanel().contains(position);
	bool isInLeftPanel = GetLeftPanel().contains(position);
	bool isInRightPanel = GetRightPanel().contains(position);
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
	if (event.type == SDL_FINGERDOWN && gmenu_left_mouse(true))
		return true;
	if (event.type == SDL_FINGERMOTION && gmenu_on_mouse_move())
		return true;
	return event.type == SDL_FINGERUP && gmenu_left_mouse(false);
}

bool HandleStoreInteraction(const SDL_Event &event)
{
	if (!IsPlayerInStore())
		return false;
	if (event.type == SDL_FINGERDOWN)
		CheckStoreBtn();
	return true;
}

void HandleSpellBookInteraction(const SDL_Event &event)
{
	if (!SpellbookFlag)
		return;

	if (event.type == SDL_FINGERUP)
		CheckSBook();
}

bool HandleSpeedBookInteraction(const SDL_Event &event)
{
	if (!SpellSelectFlag)
		return false;
	if (event.type == SDL_FINGERUP)
		SetSpell();
	return true;
}

void HandleBottomPanelInteraction(const SDL_Event &event)
{
	if (!gbRunGame || !MyPlayer->HoldItem.isEmpty())
		return;

	ResetMainPanelButtons();

	if (event.type != SDL_FINGERUP) {
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

	if (event.type == SDL_FINGERDOWN)
		CheckChrBtns();
	else if (event.type == SDL_FINGERUP && CharPanelButtonActive)
		ReleaseChrBtns(false);
}

void HandleStashPanelInteraction(const SDL_Event &event)
{
	if (!IsStashOpen || !MyPlayer->HoldItem.isEmpty())
		return;

	if (event.type != SDL_FINGERUP) {
		CheckStashButtonPress(MousePosition);
	} else {
		CheckStashButtonRelease(MousePosition);
	}
}

SdlEventType GetDeactivateEventType()
{
	static SdlEventType customEventType = SDL_RegisterEvents(1);
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

	if (!IsAnyOf(event.type, SDL_FINGERDOWN, SDL_FINGERUP, SDL_FINGERMOTION)) {
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
		if (!VirtualGamepadState.isActive || !IsAnyOf(event.type, SDL_FINGERDOWN, SDL_FINGERUP, SDL_FINGERMOTION)) {
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

	switch (event.type) {
	case SDL_FINGERDOWN:
		return HandleFingerDown(event.tfinger);

	case SDL_FINGERUP:
		return HandleFingerUp(event.tfinger);

	case SDL_FINGERMOTION:
		return HandleFingerMotion(event.tfinger);

	default:
		return false;
	}
}

bool VirtualDirectionPadEventHandler::HandleFingerDown(const SDL_TouchFingerEvent &event)
{
	if (isActive)
		return false;

	float x = event.x;
	float y = event.y;

	Point touchCoordinates = ScaleToScreenCoordinates(x, y);
	if (!virtualDirectionPad->area.contains(touchCoordinates))
		return false;

	virtualDirectionPad->UpdatePosition(touchCoordinates);
	activeFinger = event.fingerId;
	isActive = true;
	return true;
}

bool VirtualDirectionPadEventHandler::HandleFingerUp(const SDL_TouchFingerEvent &event)
{
	if (!isActive || event.fingerId != activeFinger)
		return false;

	Point position = virtualDirectionPad->area.position;
	virtualDirectionPad->UpdatePosition(position);
	isActive = false;
	return true;
}

bool VirtualDirectionPadEventHandler::HandleFingerMotion(const SDL_TouchFingerEvent &event)
{
	if (!isActive || event.fingerId != activeFinger)
		return false;

	float x = event.x;
	float y = event.y;

	Point touchCoordinates = ScaleToScreenCoordinates(x, y);
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

	switch (event.type) {
	case SDL_FINGERDOWN:
		return HandleFingerDown(event.tfinger);

	case SDL_FINGERUP:
		return HandleFingerUp(event.tfinger);

	case SDL_FINGERMOTION:
		return HandleFingerMotion(event.tfinger);

	default:
		return false;
	}
}

bool VirtualButtonEventHandler::HandleFingerDown(const SDL_TouchFingerEvent &event)
{
	if (isActive)
		return false;

	float x = event.x;
	float y = event.y;

	Point touchCoordinates = ScaleToScreenCoordinates(x, y);
	if (!virtualButton->contains(touchCoordinates))
		return false;

	if (toggles)
		virtualButton->isHeld = !virtualButton->isHeld;
	else
		virtualButton->isHeld = true;

	virtualButton->didStateChange = true;
	activeFinger = event.fingerId;
	isActive = true;
	return true;
}

bool VirtualButtonEventHandler::HandleFingerUp(const SDL_TouchFingerEvent &event)
{
	if (!isActive || event.fingerId != activeFinger)
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
	if (!isActive || event.fingerId != activeFinger)
		return false;

	if (toggles)
		return true;

	float x = event.x;
	float y = event.y;
	Point touchCoordinates = ScaleToScreenCoordinates(x, y);

	bool wasHeld = virtualButton->isHeld;
	virtualButton->isHeld = virtualButton->contains(touchCoordinates);
	virtualButton->didStateChange = virtualButton->isHeld != wasHeld;

	return true;
}

} // namespace devilution
