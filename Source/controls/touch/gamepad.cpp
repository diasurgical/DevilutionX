#include <cmath>

#include <SDL.h>

#include "control.h"
#include "controls/touch/event_handlers.h"
#include "controls/touch/gamepad.h"
#include "quests.h"
#include "utils/display.h"
#include "utils/ui_fwd.h"

namespace devilution {

VirtualGamepad VirtualGamepadState;

namespace {

constexpr float Pi = 3.141592653589793F;

int roundToInt(float value)
{
	return static_cast<int>(round(value));
}

constexpr bool PointsUp(float angle)
{
	constexpr float UpAngle = Pi / 2;
	constexpr float MinAngle = UpAngle - 3 * Pi / 8;
	constexpr float MaxAngle = UpAngle + 3 * Pi / 8;
	return MinAngle <= angle && angle <= MaxAngle;
}

constexpr bool PointsDown(float angle)
{
	constexpr float DownAngle = -Pi / 2;
	constexpr float MinAngle = DownAngle - 3 * Pi / 8;
	constexpr float MaxAngle = DownAngle + 3 * Pi / 8;
	return MinAngle <= angle && angle <= MaxAngle;
}

constexpr bool PointsLeft(float angle)
{
	constexpr float MaxAngle = Pi - 3 * Pi / 8;
	constexpr float MinAngle = -Pi + 3 * Pi / 8;
	return !(MinAngle < angle && angle < MaxAngle);
}

constexpr bool PointsRight(float angle)
{
	constexpr float MinAngle = -3 * Pi / 8;
	constexpr float MaxAngle = 3 * Pi / 8;
	return MinAngle <= angle && angle <= MaxAngle;
}

} // namespace

void InitializeVirtualGamepad()
{
	const float sqrt2 = sqrtf(2);

	int screenPixels = std::min(gnScreenWidth, gnScreenHeight);
	int inputMargin = screenPixels / 10;
	int menuButtonWidth = screenPixels / 10;
	int directionPadSize = screenPixels / 4;
	int padButtonSize = roundToInt(1.1f * screenPixels / 10);
	int padButtonSpacing = inputMargin / 3;

	float hdpi;
	float vdpi;
	int displayIndex = SDL_GetWindowDisplayIndex(ghMainWnd);
	if (SDL_GetDisplayDPI(displayIndex, nullptr, &hdpi, &vdpi) == 0) {
		int clientWidth;
		int clientHeight;
		if (renderer != nullptr)
			SDL_GetRendererOutputSize(renderer, &clientWidth, &clientHeight);
		else
			SDL_GetWindowSize(ghMainWnd, &clientWidth, &clientHeight);

		hdpi *= static_cast<float>(gnScreenWidth) / clientWidth;
		vdpi *= static_cast<float>(gnScreenHeight) / clientHeight;

		float dpi = std::min(hdpi, vdpi);
		inputMargin = roundToInt(0.25f * dpi);
		menuButtonWidth = roundToInt(0.2f * dpi);
		directionPadSize = roundToInt(dpi);
		padButtonSize = roundToInt(0.3f * dpi);
		padButtonSpacing = roundToInt(0.1f * dpi);
	}

	int menuPanelTopMargin = 30;
	int menuPanelButtonSpacing = 4;
	Size menuPanelButtonSize = { 64, 62 };
	int rightMarginMenuButton4 = menuPanelButtonSpacing + menuPanelButtonSize.width;
	int rightMarginMenuButton3 = rightMarginMenuButton4 + menuPanelButtonSpacing + menuPanelButtonSize.width;
	int rightMarginMenuButton2 = rightMarginMenuButton3 + menuPanelButtonSpacing + menuPanelButtonSize.width;
	int rightMarginMenuButton1 = rightMarginMenuButton2 + menuPanelButtonSpacing + menuPanelButtonSize.width;

	int padButtonAreaWidth = roundToInt(sqrt2 * (padButtonSize + padButtonSpacing));

	int padButtonRight = gnScreenWidth - inputMargin - padButtonSize / 2;
	int padButtonLeft = padButtonRight - padButtonAreaWidth;
	int padButtonBottom = gnScreenHeight - inputMargin - padButtonSize / 2;
	int padButtonTop = padButtonBottom - padButtonAreaWidth;

	Rectangle &charButtonArea = VirtualGamepadState.menuPanel.charButton.area;
	charButtonArea.position.x = gnScreenWidth - rightMarginMenuButton1 * menuButtonWidth / menuPanelButtonSize.width;
	charButtonArea.position.y = menuPanelTopMargin * menuButtonWidth / menuPanelButtonSize.width;
	charButtonArea.size.width = menuButtonWidth;
	charButtonArea.size.height = menuPanelButtonSize.height * menuButtonWidth / menuPanelButtonSize.width;

	Rectangle &questsButtonArea = VirtualGamepadState.menuPanel.questsButton.area;
	questsButtonArea.position.x = gnScreenWidth - rightMarginMenuButton2 * menuButtonWidth / menuPanelButtonSize.width;
	questsButtonArea.position.y = menuPanelTopMargin * menuButtonWidth / menuPanelButtonSize.width;
	questsButtonArea.size.width = menuButtonWidth;
	questsButtonArea.size.height = menuPanelButtonSize.height * menuButtonWidth / menuPanelButtonSize.width;

	Rectangle &inventoryButtonArea = VirtualGamepadState.menuPanel.inventoryButton.area;
	inventoryButtonArea.position.x = gnScreenWidth - rightMarginMenuButton3 * menuButtonWidth / menuPanelButtonSize.width;
	inventoryButtonArea.position.y = menuPanelTopMargin * menuButtonWidth / menuPanelButtonSize.width;
	inventoryButtonArea.size.width = menuButtonWidth;
	inventoryButtonArea.size.height = menuPanelButtonSize.height * menuButtonWidth / menuPanelButtonSize.width;

	Rectangle &mapButtonArea = VirtualGamepadState.menuPanel.mapButton.area;
	mapButtonArea.position.x = gnScreenWidth - rightMarginMenuButton4 * menuButtonWidth / menuPanelButtonSize.width;
	mapButtonArea.position.y = menuPanelTopMargin * menuButtonWidth / menuPanelButtonSize.width;
	mapButtonArea.size.width = menuButtonWidth;
	mapButtonArea.size.height = menuPanelButtonSize.height * menuButtonWidth / menuPanelButtonSize.width;

	Rectangle &menuPanelArea = VirtualGamepadState.menuPanel.area;
	menuPanelArea.position.x = gnScreenWidth - 399 * menuButtonWidth / menuPanelButtonSize.width;
	menuPanelArea.position.y = 0;
	menuPanelArea.size.width = 399 * menuButtonWidth / menuPanelButtonSize.width;
	menuPanelArea.size.height = 162 * menuButtonWidth / menuPanelButtonSize.width;

	VirtualDirectionPad &directionPad = VirtualGamepadState.directionPad;
	Circle &directionPadArea = directionPad.area;
	directionPadArea.position.x = inputMargin + directionPadSize / 2;
	directionPadArea.position.y = gnScreenHeight - inputMargin - directionPadSize / 2;
	directionPadArea.radius = directionPadSize / 2;
	directionPad.position = directionPadArea.position;

	int standButtonDiagonalOffset = directionPadArea.radius + padButtonSpacing / 2 + padButtonSize / 2;
	int standButtonOffset = roundToInt(standButtonDiagonalOffset / sqrt2);
	Circle &standButtonArea = VirtualGamepadState.standButton.area;
	standButtonArea.position.x = directionPadArea.position.x - standButtonOffset;
	standButtonArea.position.y = directionPadArea.position.y + standButtonOffset;
	standButtonArea.radius = padButtonSize / 2;

	Circle &primaryActionButtonArea = VirtualGamepadState.primaryActionButton.area;
	primaryActionButtonArea.position.x = padButtonRight;
	primaryActionButtonArea.position.y = (padButtonTop + padButtonBottom) / 2;
	primaryActionButtonArea.radius = padButtonSize / 2;

	Circle &secondaryActionButtonArea = VirtualGamepadState.secondaryActionButton.area;
	secondaryActionButtonArea.position.x = (padButtonLeft + padButtonRight) / 2;
	secondaryActionButtonArea.position.y = padButtonTop;
	secondaryActionButtonArea.radius = padButtonSize / 2;

	Circle &spellActionButtonArea = VirtualGamepadState.spellActionButton.area;
	spellActionButtonArea.position.x = padButtonLeft;
	spellActionButtonArea.position.y = (padButtonTop + padButtonBottom) / 2;
	spellActionButtonArea.radius = padButtonSize / 2;

	Circle &cancelButtonArea = VirtualGamepadState.cancelButton.area;
	cancelButtonArea.position.x = (padButtonLeft + padButtonRight) / 2;
	cancelButtonArea.position.y = padButtonBottom;
	cancelButtonArea.radius = padButtonSize / 2;

	VirtualPadButton &healthButton = VirtualGamepadState.healthButton;
	Circle &healthButtonArea = healthButton.area;
	healthButtonArea.position.x = directionPad.area.position.x - (padButtonSize + padButtonSpacing) / 2;
	healthButtonArea.position.y = directionPad.area.position.y - (directionPadSize + padButtonSize + padButtonSpacing) / 2;
	healthButtonArea.radius = padButtonSize / 2;
	healthButton.isUsable = []() { return !CharFlag && !QuestLogIsOpen; };

	VirtualPadButton &manaButton = VirtualGamepadState.manaButton;
	Circle &manaButtonArea = manaButton.area;
	manaButtonArea.position.x = directionPad.area.position.x + (padButtonSize + padButtonSpacing) / 2;
	manaButtonArea.position.y = directionPad.area.position.y - (directionPadSize + padButtonSize + padButtonSpacing) / 2;
	manaButtonArea.radius = padButtonSize / 2;
	manaButton.isUsable = []() { return !CharFlag && !QuestLogIsOpen; };
}

void ActivateVirtualGamepad()
{
	VirtualGamepadState.isActive = true;
}

void DeactivateVirtualGamepad()
{
	VirtualGamepadState.Deactivate();
	DeactivateTouchEventHandlers();
}

void VirtualGamepad::Deactivate()
{
	isActive = false;

	menuPanel.Deactivate();
	directionPad.Deactivate();
	standButton.Deactivate();

	primaryActionButton.Deactivate();
	secondaryActionButton.Deactivate();
	spellActionButton.Deactivate();
	cancelButton.Deactivate();

	healthButton.Deactivate();
	manaButton.Deactivate();
}

void VirtualMenuPanel::Deactivate()
{
	charButton.Deactivate();
	questsButton.Deactivate();
	inventoryButton.Deactivate();
	mapButton.Deactivate();
}

void VirtualDirectionPad::UpdatePosition(Point touchCoordinates)
{
	position = touchCoordinates;

	Displacement diff = position - area.position;
	if (diff == Displacement { 0, 0 }) {
		isUpPressed = false;
		isDownPressed = false;
		isLeftPressed = false;
		isRightPressed = false;
		return;
	}

	if (!area.contains(position)) {
		int x = diff.deltaX;
		int y = diff.deltaY;
		float dist = sqrtf(static_cast<float>(x * x + y * y));
		x = roundToInt(x * area.radius / dist);
		y = roundToInt(y * area.radius / dist);
		position.x = area.position.x + x;
		position.y = area.position.y + y;
	}

	float angle = atan2f(static_cast<float>(-diff.deltaY), static_cast<float>(diff.deltaX));

	isUpPressed = PointsUp(angle);
	isDownPressed = PointsDown(angle);
	isLeftPressed = PointsLeft(angle);
	isRightPressed = PointsRight(angle);
}

void VirtualDirectionPad::Deactivate()
{
	position = area.position;
	isUpPressed = false;
	isDownPressed = false;
	isLeftPressed = false;
	isRightPressed = false;
}

void VirtualButton::Deactivate()
{
	isHeld = false;
	didStateChange = false;
}

} // namespace devilution
