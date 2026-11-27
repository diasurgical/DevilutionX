/**
 * @file cursor.h
 *
 * Interface of cursor tracking functionality.
 */
#pragma once

#include "cursor_defs.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/point.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"
#include "utils/attributes.h"

namespace devilution {

extern int pcursmonst;
extern int8_t pcursinvitem;
extern uint16_t pcursstashitem;
extern int8_t pcursitem;

struct Object; // Defined in objects.h
extern Object *ObjectUnderCursor;

struct Player; // Defined in player.h
extern const Player *PlayerUnderCursor;
extern Point cursPosition;
extern DVL_API_FOR_TEST int pcurs;

void InitCursor();
void FreeCursor();
void ResetCursor();

struct Item;
/**
 * @brief Use the item sprite as the cursor (or show the default hand cursor if the item isEmpty)
 */
void NewCursor(const Item &item);

void NewCursor(int cursId);

void InitLevelCursor();
void CheckRportal();
void CheckTown();
void CheckCursMove();

void DrawSoftwareCursor(const Surface &out, Point position, int cursId);

void DrawItem(const Item &item, const Surface &out, Point position, ClxSprite clx);

/** Returns the sprite for the given inventory index. Missing custom cursors fall back to the first vanilla item graphic. */
ClxSprite GetInvItemSprite(int cursId);

ClxSprite GetHalfSizeItemSprite(int cursId);
ClxSprite GetHalfSizeItemSpriteRed(int cursId);
void CreateHalfSizeItemSprites();
void FreeHalfSizeItemSprites();

/** Returns the width and height for an inventory index. */
Size GetInvItemSize(int cursId);

/** Registers a custom cursor sprite and returns the _iCurs value to use.
 * Custom IDs start at ItemCAnimTblSize (the number of entries in ItemCAnimTbl).
 * Fails if the id would exceed uint8_t (same storage as Item::_iCurs). */
int RegisterCustomCursorGraphic(OwnedClxSpriteList sprite);

/** Frees all custom cursor sprites. */
void FreeCustomCursorSprites();

} // namespace devilution
