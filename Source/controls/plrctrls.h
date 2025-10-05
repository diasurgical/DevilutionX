#pragma once
// Controller actions implementation

#include <cstddef>
#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#else
#include <SDL.h>
#endif

#include "controls/controller.h"
#include "controls/game_controls.h"
#include "player.h"

namespace devilution {

enum class BeltItemType : uint8_t {
	Healing,
	Mana,
};

extern GameActionType ControllerActionHeld;
extern bool StandToggle;

// Runs every frame.
// Handles menu movement.
void plrctrls_every_frame();

// Run after every game logic iteration.
// Handles player movement.
void plrctrls_after_game_logic();

// Runs at the end of CheckCursMove()
// Handles item, object, and monster auto-aim.
void plrctrls_after_check_curs_move();

// Moves the map if active, the cursor otherwise.
void HandleRightStickMotion();

// Whether we're in a dialog menu that the game handles natively with keyboard controls.
bool InGameMenu();

void SetPointAndClick(bool value);

bool IsPointAndClick();
bool IsMovementHandlerActive();

void DetectInputMethod(const SDL_Event &event, const ControllerButtonEvent &gamepadEvent);
void ProcessGameAction(const GameAction &action);

void UseBeltItem(BeltItemType type);

// Talk to towners, click on inv items, attack, etc.
void PerformPrimaryAction();

// Open chests, doors, pickup items.
void PerformSecondaryAction();
void UpdateSpellTarget(SpellID spell);
bool TryDropItem();
void InvalidateInventorySlot();
void FocusOnInventory();
void PerformSpellAction();
void QuickCast(size_t slot);

extern int speedspellcount;

} // namespace devilution
