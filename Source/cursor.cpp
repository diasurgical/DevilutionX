/**
 * @file cursor.cpp
 *
 * Implementation of cursor tracking functionality.
 */
#include "cursor.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_surface.h>
#else
#include <SDL.h>
#endif

#include <fmt/format.h>

#include "DiabloUI/diabloui.h"
#include "control.h"
#include "controls/control_mode.hpp"
#include "controls/plrctrls.h"
#include "doom.h"
#include "engine/backbuffer_state.hpp"
#include "engine/demomode.h"
#include "engine/point.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/trn.hpp"
#include "headless_mode.hpp"
#include "hwcursor.hpp"
#include "inv.h"
#include "levels/trigs.h"
#include "missiles.h"
#include "options.h"
#include "qol/itemlabels.h"
#include "qol/stash.h"
#include "towners.h"
#include "track.h"
#include "utils/attributes.h"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/palette_blending.hpp"
#include "utils/sdl_bilinear_scale.hpp"
#include "utils/surface_to_clx.hpp"
#include "utils/utf8.hpp"

#ifdef UNPACKED_MPQS
#include "engine/load_clx.hpp"
#else
#include "engine/load_cel.hpp"
#include "engine/load_file.hpp"
#include "utils/parse_int.hpp"
#endif

namespace devilution {
namespace {
/** Cursor images CEL */
OptionalOwnedClxSpriteList pCursCels;
OptionalOwnedClxSpriteList pCursCels2;

OptionalOwnedClxSpriteList *HalfSizeItemSprites;
OptionalOwnedClxSpriteList *HalfSizeItemSpritesRed;

bool IsValidMonsterForSelection(const Monster &monster)
{
	if (monster.hitPoints >> 6 <= 0)
		return false;
	if ((monster.flags & MFLAG_HIDDEN) != 0)
		return false;
	if (monster.isPlayerMinion())
		return false;
	return true;
}

bool TrySelectMonster(bool flipflag, Point tile, tl::function_ref<bool(const Monster &)> isValidMonster)
{
	auto checkPosition = [&](SelectionRegion selectionRegion, Displacement displacement) {
		const Point posToCheck = tile + displacement;
		if (!InDungeonBounds(posToCheck) || dMonster[posToCheck.x][posToCheck.y] == 0)
			return;
		const uint16_t monsterId = std::abs(dMonster[posToCheck.x][posToCheck.y]) - 1;
		const Monster &monster = Monsters[monsterId];
		if (IsTileLit(posToCheck) && HasAnyOf(monster.data().selectionRegion, selectionRegion) && isValidMonster(monster)) {
			cursPosition = posToCheck;
			pcursmonst = monsterId;
		}
	};

	if (!flipflag)
		checkPosition(SelectionRegion::Top, { 2, 1 });
	if (flipflag)
		checkPosition(SelectionRegion::Top, { 1, 2 });
	checkPosition(SelectionRegion::Top, { 2, 2 });
	if (!flipflag)
		checkPosition(SelectionRegion::Middle, { 1, 0 });
	if (flipflag)
		checkPosition(SelectionRegion::Middle, { 0, 1 });
	checkPosition(SelectionRegion::Bottom, { 0, 0 });
	checkPosition(SelectionRegion::Middle, { 1, 1 });
	return pcursmonst != -1;
}

bool TrySelectTowner(bool flipflag, Point tile)
{
	auto checkPosition = [&](Displacement displacement) {
		const Point posToCheck = tile + displacement;
		if (!InDungeonBounds(posToCheck) || dMonster[posToCheck.x][posToCheck.y] == 0)
			return;
		const uint16_t monsterId = std::abs(dMonster[posToCheck.x][posToCheck.y]) - 1;
		cursPosition = posToCheck;
		pcursmonst = monsterId;
	};
	if (!flipflag)
		checkPosition({ 1, 0 });
	if (flipflag)
		checkPosition({ 0, 1 });
	checkPosition({ 0, 0 });
	checkPosition({ 1, 1 });
	return pcursmonst != -1;
}

bool TrySelectPlayer(bool flipflag, const Point tile)
{
	if (!flipflag && tile.x + 1 < MAXDUNX && dPlayer[tile.x + 1][tile.y] != 0) {
		const uint8_t playerId = std::abs(dPlayer[tile.x + 1][tile.y]) - 1;
		Player &player = Players[playerId];
		if (&player != MyPlayer && player._pHitPoints != 0) {
			cursPosition = tile + Displacement { 1, 0 };
			PlayerUnderCursor = &player;
		}
	}
	if (flipflag && tile.y + 1 < MAXDUNY && dPlayer[tile.x][tile.y + 1] != 0) {
		const uint8_t playerId = std::abs(dPlayer[tile.x][tile.y + 1]) - 1;
		Player &player = Players[playerId];
		if (&player != MyPlayer && player._pHitPoints != 0) {
			cursPosition = tile + Displacement { 0, 1 };
			PlayerUnderCursor = &player;
		}
	}
	if (dPlayer[tile.x][tile.y] != 0) {
		const uint8_t playerId = std::abs(dPlayer[tile.x][tile.y]) - 1;
		Player &player = Players[playerId];
		if (&player != MyPlayer) {
			cursPosition = tile;
			PlayerUnderCursor = &player;
		}
	}
	if (TileContainsDeadPlayer(tile)) {
		for (const Player &player : Players) {
			if (player.position.tile == tile && &player != MyPlayer) {
				cursPosition = tile;
				PlayerUnderCursor = &player;
			}
		}
	}
	if (pcurs == CURSOR_RESURRECT) {
		for (int xx = -1; xx < 2; xx++) {
			for (int yy = -1; yy < 2; yy++) {
				if (TileContainsDeadPlayer(tile + Displacement { xx, yy })) {
					for (const Player &player : Players) {
						if (player.position.tile == tile + Displacement { xx, yy } && &player != MyPlayer) {
							cursPosition = tile + Displacement { xx, yy };
							PlayerUnderCursor = &player;
						}
					}
				}
			}
		}
	}
	if (tile.x + 1 < MAXDUNX && tile.y + 1 < MAXDUNY && dPlayer[tile.x + 1][tile.y + 1] != 0) {
		const uint8_t playerId = std::abs(dPlayer[tile.x + 1][tile.y + 1]) - 1;
		const Player &player = Players[playerId];
		if (&player != MyPlayer && player._pHitPoints != 0) {
			cursPosition = tile + Displacement { 1, 1 };
			PlayerUnderCursor = &player;
		}
	}

	return PlayerUnderCursor != nullptr;
}

/**
 * @brief Try find an object starting with the tile below the current tile (tall objects like doors)
 */
bool TrySelectObject(bool flipflag, Point tile)
{
	Point testPosition = tile + Direction::South;
	Object *object = FindObjectAtPosition(testPosition);

	if (object == nullptr || HasNoneOf(object->selectionRegion, SelectionRegion::Middle)) {
		// Either no object or can't interact from the test position, try the current tile
		testPosition = tile;
		object = FindObjectAtPosition(testPosition);

		if (object == nullptr || HasNoneOf(object->selectionRegion, SelectionRegion::Bottom)) {
			// Still no object (that could be activated from this position), try the tile to the bottom left or right
			//  (whichever is closest to the cursor as determined when we set flipflag earlier)
			testPosition = tile + (flipflag ? Direction::SouthWest : Direction::SouthEast);
			object = FindObjectAtPosition(testPosition);

			if (object != nullptr && HasNoneOf(object->selectionRegion, SelectionRegion::Middle)) {
				// Found an object but it's not in range, clear the pointer
				object = nullptr;
			}
		}
	}
	if (object == nullptr)
		return false;

	// found object that can be activated with the given cursor position
	cursPosition = testPosition;
	ObjectUnderCursor = object;
	return true;
}

bool TrySelectItem(bool flipflag, Point tile)
{
	if (!flipflag && tile.x + 1 < MAXDUNX && dItem[tile.x + 1][tile.y] > 0) {
		const uint8_t itemId = dItem[tile.x + 1][tile.y] - 1;
		if (HasAnyOf(Items[itemId].selectionRegion, SelectionRegion::Middle)) {
			cursPosition = tile + Displacement { 1, 0 };
			pcursitem = static_cast<int8_t>(itemId);
		}
	}
	if (flipflag && tile.y + 1 < MAXDUNY && dItem[tile.x][tile.y + 1] > 0) {
		const uint8_t itemId = dItem[tile.x][tile.y + 1] - 1;
		if (HasAnyOf(Items[itemId].selectionRegion, SelectionRegion::Middle)) {
			cursPosition = tile + Displacement { 0, 1 };
			pcursitem = static_cast<int8_t>(itemId);
		}
	}
	if (dItem[tile.x][tile.y] > 0) {
		const uint8_t itemId = dItem[tile.x][tile.y] - 1;
		if (HasAnyOf(Items[itemId].selectionRegion, SelectionRegion::Bottom)) {
			cursPosition = tile;
			pcursitem = static_cast<int8_t>(itemId);
		}
	}
	if (tile.x + 1 < MAXDUNX && tile.y + 1 < MAXDUNY && dItem[tile.x + 1][tile.y + 1] > 0) {
		const uint8_t itemId = dItem[tile.x + 1][tile.y + 1] - 1;
		if (HasAnyOf(Items[itemId].selectionRegion, SelectionRegion::Middle)) {
			cursPosition = tile + Displacement { 1, 1 };
			pcursitem = static_cast<int8_t>(itemId);
		}
	}
	return pcursitem != -1;
}

bool TrySelectPixelBased(Point tile)
{
	if (demo::IsRunning() || demo::IsRecording() || HeadlessMode) {
		// Recorded demos can run headless, but headless mode doesn't support loading sprites that are needed for pixel perfect selection
		// => Ensure demos are always compatible
		// => Never use sprites for selection when handling demos
		return false;
	}

	auto checkSprite = [](Point renderingTile, const ClxSprite sprite, Displacement renderingOffset) {
		const Point renderPosition = GetScreenPosition(renderingTile) + renderingOffset;
		Point spriteTopLeft = renderPosition - Displacement { 0, sprite.height() };
		Size spriteSize = { sprite.width(), sprite.height() };
		if (*GetOptions().Graphics.zoom) {
			spriteSize *= 2;
			spriteTopLeft *= 2;
		}
		const Rectangle spriteCoords = Rectangle(spriteTopLeft, spriteSize);
		if (!spriteCoords.contains(MousePosition))
			return false;
		Point pointInSprite = Point { 0, 0 } + (MousePosition - spriteCoords.position);
		if (*GetOptions().Graphics.zoom)
			pointInSprite /= 2;
		return IsPointWithinClx(pointInSprite, sprite);
	};

	auto convertFromRenderingToWorldTile = [](Point renderingPoint) {
		// Columns
		Displacement ret = Displacement(Direction::East) * renderingPoint.x;
		// Rows
		ret += Displacement(Direction::South) * renderingPoint.y / 2;
		if ((renderingPoint.y & 1) == 1)
			ret.deltaY += 1;
		return ret;
	};

	// Try to find the selected entity from rendered pixels.
	// We search the rendered rows/columns backwards, because the last rendered tile overrides previous rendered pixels.
	auto searchArea = PointsInRectangle(Rectangle { { -1, -1 }, { 3, 8 } });
	for (auto it = searchArea.rbegin(); it != searchArea.rend(); ++it) {
		const Point renderingColumnRaw = *it;
		const Point adjacentTile = tile + convertFromRenderingToWorldTile(renderingColumnRaw);
		if (!InDungeonBounds(adjacentTile))
			continue;

		int monsterId = dMonster[adjacentTile.x][adjacentTile.y];
		// Never select a monster if a target-player-only spell is selected
		if (monsterId != 0 && IsNoneOf(pcurs, CURSOR_HEALOTHER, CURSOR_RESURRECT)) {
			monsterId = std::abs(monsterId) - 1;
			if (leveltype == DTYPE_TOWN) {
				const Towner &towner = Towners[monsterId];
				const ClxSprite sprite = towner.currentSprite();
				const Displacement renderingOffset = towner.getRenderingOffset();
				if (checkSprite(adjacentTile, sprite, renderingOffset)) {
					cursPosition = adjacentTile;
					pcursmonst = monsterId;
					return true;
				}
			} else {
				const Monster &monster = Monsters[monsterId];
				if (IsTileLit(adjacentTile) && IsValidMonsterForSelection(monster)) {
					const ClxSprite sprite = monster.animInfo.currentSprite();
					const Displacement renderingOffset = monster.getRenderingOffset(sprite);
					if (checkSprite(adjacentTile, sprite, renderingOffset)) {
						cursPosition = adjacentTile;
						pcursmonst = monsterId;
						return true;
					}
				}
			}
		}

		const int8_t dPlayerValue = dPlayer[adjacentTile.x][adjacentTile.y];
		if (dPlayerValue != 0) {
			const uint8_t playerId = std::abs(dPlayerValue) - 1;
			if (playerId != MyPlayerId) {
				const Player &player = Players[playerId];
				const ClxSprite sprite = player.currentSprite();
				const Displacement renderingOffset = player.getRenderingOffset(sprite);
				if (checkSprite(adjacentTile, sprite, renderingOffset)) {
					cursPosition = adjacentTile;
					PlayerUnderCursor = &player;
					return true;
				}
			}
		}
		if (TileContainsDeadPlayer(adjacentTile)) {
			for (const Player &player : Players) {
				if (player.position.tile == adjacentTile && &player != MyPlayer) {
					const ClxSprite sprite = player.currentSprite();
					const Displacement renderingOffset = player.getRenderingOffset(sprite);
					if (checkSprite(adjacentTile, sprite, renderingOffset)) {
						cursPosition = adjacentTile;
						PlayerUnderCursor = &player;
						return true;
					}
				}
			}
		}

		Object *object = FindObjectAtPosition(adjacentTile);
		if (object != nullptr && object->canInteractWith()) {
			const ClxSprite sprite = object->currentSprite();
			const Displacement renderingOffset = object->getRenderingOffset(sprite, adjacentTile);
			if (checkSprite(adjacentTile, sprite, renderingOffset)) {
				cursPosition = adjacentTile;
				ObjectUnderCursor = object;
				return true;
			}
		}

		uint8_t itemId = dItem[adjacentTile.x][adjacentTile.y];
		if (itemId != 0) {
			itemId = itemId - 1;
			const Item &item = Items[itemId];
			const ClxSprite sprite = item.AnimInfo.currentSprite();
			const Displacement renderingOffset = item.getRenderingOffset(sprite);
			if (checkSprite(adjacentTile, sprite, renderingOffset)) {
				cursPosition = adjacentTile;
				pcursitem = static_cast<int8_t>(itemId);
				return true;
			}
		}
	}

	return false;
}

#ifndef UNPACKED_MPQS
std::vector<uint16_t> ReadWidths(AssetRef &&ref)
{
	const size_t len = ref.size();
	if (len == 0) {
		app_fatal("Missing widths");
	}
	const std::unique_ptr<char[]> data { new char[len] };

	AssetHandle handle = OpenAsset(std::move(ref));
	if (!handle.ok() || !handle.read(data.get(), len)) {
		app_fatal("Failed to load widths");
	}

	std::string_view str { data.get(), len };
	std::vector<uint16_t> result;
	while (!str.empty()) {
		const char *end;
		const ParseIntResult<uint16_t> parseResult = ParseInt<uint16_t>(str, std::numeric_limits<uint16_t>::min(),
		    std::numeric_limits<uint16_t>::max(), &end);
		if (!parseResult.has_value()) {
			app_fatal(StrCat("Failed to parse width value from: [", str, "]"));
		}
		result.push_back(parseResult.value());
		str.remove_prefix(end - str.data());
		while (!str.empty() && (str[0] == '\r' || str[0] == '\n')) {
			str.remove_prefix(1);
		}
	}
	return result;
}
#endif

} // namespace

/** Current highlighted monster */
int pcursmonst = -1;

/** inv_item value */
int8_t pcursinvitem;
/** StashItem value */
uint16_t pcursstashitem;
/** Current highlighted item */
int8_t pcursitem;
/** Current highlighted object */
Object *ObjectUnderCursor;
/** Current highlighted player */
const Player *PlayerUnderCursor;
/** Current highlighted tile position */
Point cursPosition;
/** Previously highlighted monster */
int pcurstemp;
/** Index of current cursor image */
int pcurs;

void InitCursor()
{
	assert(!pCursCels);
#ifdef UNPACKED_MPQS
	pCursCels = LoadClx("data\\inv\\objcurs.clx");
	pCursCels2 = LoadOptionalClx("data\\inv\\objcurs2.clx");
#else
	pCursCels = LoadCel("data\\inv\\objcurs", ReadWidths(FindAsset("data\\inv\\objcurs-widths.txt")).data());
	AssetRef ref = FindAsset("data\\inv\\objcurs2-widths.txt");
	if (ref.ok()) {
		pCursCels2 = LoadOptionalCel("data\\inv\\objcurs2", ReadWidths(std::move(ref)).data());
	}
#endif
	ClearCursor();
}

void FreeCursor()
{
	pCursCels = std::nullopt;
	pCursCels2 = std::nullopt;
	ClearCursor();
}

ClxSprite GetInvItemSprite(int cursId)
{
	assert(cursId > 0);
	const size_t numSprites = pCursCels->numSprites();
	if (static_cast<size_t>(cursId) <= numSprites) {
		return (*pCursCels)[cursId - 1];
	}
	assert(pCursCels2.has_value());
	assert(cursId - numSprites <= pCursCels2->numSprites());
	return (*pCursCels2)[cursId - numSprites - 1];
}

Size GetInvItemSize(int cursId)
{
	const ClxSprite sprite = GetInvItemSprite(cursId);
	return { sprite.width(), sprite.height() };
}

ClxSprite GetHalfSizeItemSprite(int cursId)
{
	return (*HalfSizeItemSprites[cursId])[0];
}

ClxSprite GetHalfSizeItemSpriteRed(int cursId)
{
	return (*HalfSizeItemSpritesRed[cursId])[0];
}

void CreateHalfSizeItemSprites()
{
	if (HalfSizeItemSprites != nullptr)
		return;
	const uint32_t numInvItems = pCursCels->numSprites() - (static_cast<uint32_t>(CURSOR_FIRSTITEM) - 1)
	    + (pCursCels2.has_value() ? pCursCels2->numSprites() : 0);
	HalfSizeItemSprites = new OptionalOwnedClxSpriteList[numInvItems];
	HalfSizeItemSpritesRed = new OptionalOwnedClxSpriteList[numInvItems];
	const uint8_t *redTrn = GetInfravisionTRN();

	constexpr int MaxWidth = 28 * 3;
	constexpr int MaxHeight = 28 * 3;
	OwnedSurface ownedItemSurface { MaxWidth, MaxHeight };
	OwnedSurface ownedHalfSurface { MaxWidth / 2, MaxHeight / 2 };

	const auto createHalfSize = [&, redTrn](const ClxSprite itemSprite, size_t outputIndex) {
		if (itemSprite.width() <= 28 && itemSprite.height() <= 28) {
			// Skip creating half-size sprites for 1x1 items because we always render them at full size anyway.
			return;
		}
		const Surface itemSurface = ownedItemSurface.subregion(0, 0, itemSprite.width(), itemSprite.height());
		const SDL_Rect itemSurfaceRect = MakeSdlRect(0, 0, itemSurface.w(), itemSurface.h());

		SDL_SetSurfaceClipRect(itemSurface.surface, &itemSurfaceRect);
		SDL_FillSurfaceRect(itemSurface.surface, nullptr, 1);
		ClxDraw(itemSurface, { 0, itemSurface.h() }, itemSprite);

		const Surface halfSurface = ownedHalfSurface.subregion(0, 0, itemSurface.w() / 2, itemSurface.h() / 2);
		const SDL_Rect halfSurfaceRect = MakeSdlRect(0, 0, halfSurface.w(), halfSurface.h());
		SDL_SetSurfaceClipRect(halfSurface.surface, &halfSurfaceRect);
		BilinearDownscaleByHalf8(itemSurface.surface, paletteTransparencyLookup, halfSurface.surface, 1);
		HalfSizeItemSprites[outputIndex].emplace(SurfaceToClx(halfSurface, 1, 1));

		SDL_FillSurfaceRect(itemSurface.surface, nullptr, 1);
		ClxDrawTRN(itemSurface, { 0, itemSurface.h() }, itemSprite, redTrn);
		BilinearDownscaleByHalf8(itemSurface.surface, paletteTransparencyLookup, halfSurface.surface, 1);
		HalfSizeItemSpritesRed[outputIndex].emplace(SurfaceToClx(halfSurface, 1, 1));
	};

	size_t outputIndex = 0;
	for (size_t i = static_cast<int>(CURSOR_FIRSTITEM) - 1, n = pCursCels->numSprites(); i < n; ++i, ++outputIndex) {
		createHalfSize((*pCursCels)[i], outputIndex);
	}
	if (pCursCels2.has_value()) {
		for (size_t i = 0, n = pCursCels2->numSprites(); i < n; ++i, ++outputIndex) {
			createHalfSize((*pCursCels2)[i], outputIndex);
		}
	}
}

void FreeHalfSizeItemSprites()
{
	if (HalfSizeItemSprites != nullptr) {
		delete[] HalfSizeItemSprites;
		HalfSizeItemSprites = nullptr;
		delete[] HalfSizeItemSpritesRed;
		HalfSizeItemSpritesRed = nullptr;
	}
}

void DrawItem(const Item &item, const Surface &out, Point position, ClxSprite clx)
{
	const bool usable = !IsInspectingPlayer() ? item._iStatFlag : InspectPlayer->CanUseItem(item);
	if (usable) {
		ClxDraw(out, position, clx);
	} else {
		ClxDrawTRN(out, position, clx, GetInfravisionTRN());
	}
}

void ResetCursor()
{
	NewCursor(pcurs);
}

void NewCursor(const Item &item)
{
	if (item.isEmpty()) {
		NewCursor(CURSOR_HAND);
	} else {
		NewCursor(item._iCurs + CURSOR_FIRSTITEM);
	}
}

void NewCursor(int cursId)
{
	if (pcurs >= CURSOR_FIRSTITEM && cursId > CURSOR_HAND && cursId < CURSOR_HOURGLASS) {
		if (!TryDropItem()) {
			return;
		}
	}

	if (cursId < CURSOR_HOURGLASS && MyPlayer != nullptr) {
		MyPlayer->HoldItem.clear();
	}
	pcurs = cursId;

	if (IsHardwareCursorEnabled() && ControlDevice == ControlTypes::KeyboardAndMouse) {
		if (!ArtCursor && cursId == CURSOR_NONE)
			return;

		const CursorInfo newCursor = ArtCursor
		    ? CursorInfo::UserInterfaceCursor()
		    : CursorInfo::GameCursor(cursId);
		if (newCursor != GetCurrentCursorInfo())
			SetHardwareCursor(newCursor);
	}
}

void DrawSoftwareCursor(const Surface &out, Point position, int cursId)
{
	const ClxSprite sprite = GetInvItemSprite(cursId);
	if (cursId >= CURSOR_FIRSTITEM && !MyPlayer->HoldItem.isEmpty()) {
		const auto &heldItem = MyPlayer->HoldItem;
		ClxDrawOutline(out, GetOutlineColor(heldItem, true), position, sprite);
		DrawItem(heldItem, out, position, sprite);
	} else {
		ClxDraw(out, position, sprite);
	}
}

void InitLevelCursor()
{
	NewCursor(CURSOR_HAND);
	cursPosition = ViewPosition;
	pcurstemp = -1;
	pcursmonst = -1;
	ObjectUnderCursor = nullptr;
	pcursitem = -1;
	pcursstashitem = StashStruct::EmptyCell;
	PlayerUnderCursor = nullptr;
	ClearCursor();
}

void CheckTown()
{
	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::TownPortal) {
			if (EntranceBoundaryContains(missile.position.tile, cursPosition)) {
				trigflag = true;
				InfoString = _("Town Portal");
				AddInfoBoxString(fmt::format(fmt::runtime(_("from {:s}")), Players[missile._misource]._pName));
				cursPosition = missile.position.tile;
			}
		}
	}
}

void CheckRportal()
{
	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::RedPortal) {
			if (EntranceBoundaryContains(missile.position.tile, cursPosition)) {
				trigflag = true;
				InfoString = _("Portal to");
				AddInfoBoxString(!setlevel ? _("The Unholy Altar") : _("level 15"));
				cursPosition = missile.position.tile;
			}
		}
	}
}

void DisplayTriggerInfo()
{
	CheckTrigForce();
	CheckTown();
	CheckRportal();
}

/**
 * @brief Adjusts mouse position based on panels
 */
void AlterMousePositionViaPanels(Point &screenPosition)
{
	if (CanPanelsCoverView()) {
		if (IsLeftPanelOpen()) {
			screenPosition.x -= GetScreenWidth() / 4;
		} else if (IsRightPanelOpen()) {
			screenPosition.x += GetScreenWidth() / 4;
		}
	}
}

/**
 * @brief If scrolling, offset the mousepos
 */
void AlterMousePositionViaScrolling(Point &screenPosition, Rectangle mainPanel)
{
	if (mainPanel.contains(MousePosition) && track_isscrolling()) {
		screenPosition.y = mainPanel.position.y - 1;
	}
}

/**
 * @brief Adjust based on current zoom
 */
void AlterMousePositionViaZoom(Point &screenPosition)
{
	if (*GetOptions().Graphics.zoom) {
		screenPosition.x /= 2;
		screenPosition.y /= 2;
	}
}

/**
 * @brief Adjust by player offset and tile grid alignment
 */
void AlterMousePositionViaPlayer(Point &screenPosition, const Player &myPlayer)
{
	int xo = 0;
	int yo = 0;
	CalcTileOffset(&xo, &yo);
	screenPosition.x += xo;
	screenPosition.y += yo;

	// Adjust for player walking
	if (myPlayer.isWalking()) {
		const Displacement offset = GetOffsetForWalking(myPlayer.AnimInfo, myPlayer._pdir, true);
		screenPosition.x -= offset.deltaX;
		screenPosition.y -= offset.deltaY;

		// Predict the next frame when walking to avoid input jitter
		const DisplacementOf<int16_t> offset2 = myPlayer.position.CalculateWalkingOffsetShifted8(myPlayer._pdir, myPlayer.AnimInfo);
		const DisplacementOf<int16_t> velocity = myPlayer.position.GetWalkingVelocityShifted8(myPlayer._pdir, myPlayer.AnimInfo);
		int fx = offset2.deltaX / 256;
		int fy = offset2.deltaY / 256;
		fx -= (offset2.deltaX + velocity.deltaX) / 256;
		fy -= (offset2.deltaY + velocity.deltaY) / 256;

		screenPosition.x -= fx;
		screenPosition.y -= fy;
	}
}

Point ConvertToTileGrid(Point &screenPosition)
{
	int columns = 0;
	int rows = 0;
	TilesInView(&columns, &rows);
	const int lrow = rows - RowsCoveredByPanel();

	// Center player tile on screen
	Point currentTile = ViewPosition;
	ShiftGrid(&currentTile, -columns / 2, -lrow / 2);

	// Align grid
	if ((columns % 2) == 0 && (lrow % 2) == 0) {
		screenPosition.y += TILE_HEIGHT / 2;
	} else if ((columns % 2) != 0 && (lrow % 2) != 0) {
		screenPosition.x -= TILE_WIDTH / 2;
	} else if ((columns % 2) != 0 && (lrow % 2) == 0) {
		currentTile.y++;
	}

	if (*GetOptions().Graphics.zoom) {
		screenPosition.y -= TILE_HEIGHT / 4;
	}

	const int tx = screenPosition.x / TILE_WIDTH;
	const int ty = screenPosition.y / TILE_HEIGHT;
	ShiftGrid(&currentTile, tx, ty);

	return currentTile;
}

/**
 * @brief Shift position to match diamond grid alignment
 */
void ShiftToDiamondGridAlignment(Point screenPosition, Point &tile, bool &flipflag)
{
	const int px = screenPosition.x % TILE_WIDTH;
	const int py = screenPosition.y % TILE_HEIGHT;

	const bool flipy = py < (px / 2);
	if (flipy) {
		tile.y--;
	}
	const bool flipx = py >= TILE_HEIGHT - (px / 2);
	if (flipx) {
		tile.x++;
	}

	tile.x = std::clamp(tile.x, 0, MAXDUNX - 1);
	tile.y = std::clamp(tile.y, 0, MAXDUNY - 1);

	flipflag = (flipy && flipx) || ((flipy || flipx) && px < TILE_WIDTH / 2);
}

/**
 * @brief While holding the button down we should retain target (but potentially lose it if it dies, goes out of view, etc)
 */
bool CheckMouseHold(const Point currentTile)
{
	if ((sgbMouseDown != CLICK_NONE || ControllerActionHeld != GameActionType_NONE) && IsNoneOf(LastPlayerAction, PlayerActionType::None, PlayerActionType::Attack, PlayerActionType::Spell)) {
		InvalidateTargets();

		if (pcursmonst == -1 && ObjectUnderCursor == nullptr && pcursitem == -1 && pcursinvitem == -1 && pcursstashitem == StashStruct::EmptyCell && PlayerUnderCursor == nullptr) {
			cursPosition = currentTile;
			DisplayTriggerInfo();
		}
		return true;
	}
	return false;
}

void ResetCursorInfo()
{
	pcurstemp = pcursmonst;
	pcursmonst = -1;
	ObjectUnderCursor = nullptr;
	pcursitem = -1;
	if (pcursinvitem != -1) {
		RedrawComponent(PanelDrawComponent::Belt);
	}
	pcursinvitem = -1;
	pcursstashitem = StashStruct::EmptyCell;
	PlayerUnderCursor = nullptr;
	ShowUniqueItemInfoBox = false;
	MainPanelFlag = false;
	trigflag = false;
}

bool CheckPlayerState(const Point currentTile, const Player &myPlayer)
{
	if (myPlayer._pInvincible) {
		return true;
	}
	if (!myPlayer.HoldItem.isEmpty() || SpellSelectFlag) {
		cursPosition = currentTile;
		return true;
	}
	return false;
}

bool CheckPanelsAndFlags(Rectangle mainPanel)
{
	if (mainPanel.contains(MousePosition)) {
		CheckPanelInfo();
		return true;
	}
	if (DoomFlag) {
		return true;
	}
	if (invflag && GetRightPanel().contains(MousePosition)) {
		pcursinvitem = CheckInvHLight();
		return true;
	}
	if (IsStashOpen && GetLeftPanel().contains(MousePosition)) {
		pcursstashitem = CheckStashHLight(MousePosition);
	}
	if (SpellbookFlag && GetRightPanel().contains(MousePosition)) {
		return true;
	}
	if (IsLeftPanelOpen() && GetLeftPanel().contains(MousePosition)) {
		return true;
	}
	return false;
}

bool CheckCursorActions(const Point currentTile, bool flipflag)
{
	if (pcurs == CURSOR_IDENTIFY) {
		ObjectUnderCursor = nullptr;
		pcursmonst = -1;
		pcursitem = -1;
		cursPosition = currentTile;
		return true;
	}

	if (TrySelectPixelBased(currentTile))
		return true;

	if (leveltype != DTYPE_TOWN) {
		// Never select a monster if a target-player-only spell is selected
		if (IsNoneOf(pcurs, CURSOR_HEALOTHER, CURSOR_RESURRECT)) {
			if (pcurstemp != -1 && TrySelectMonster(flipflag, currentTile, [](const Monster &monster) {
				    if (!IsValidMonsterForSelection(monster))
					    return false;
				    if (monster.getId() != static_cast<unsigned>(pcurstemp))
					    return false;
				    return true;
			    })) {
				// found a valid previous selected monster
				return true;
			}
			if (TrySelectMonster(flipflag, currentTile, IsValidMonsterForSelection)) {
				// found a valid monster
				return true;
			}
		}
	} else {
		if (TrySelectTowner(flipflag, currentTile)) {
			// found a towner
			return true;
		}
	}

	return TrySelectPlayer(flipflag, currentTile)
	    || TrySelectObject(flipflag, currentTile)
	    || TrySelectItem(flipflag, currentTile);
}

/**
 * @brief Checks for early return if an item is highlighted
 */
void CheckCursMove()
{
	if (IsItemLabelHighlighted())
		return;

	Point screenPosition = MousePosition;
	const Rectangle &mainPanel = GetMainPanel();

	AlterMousePositionViaPanels(screenPosition);
	AlterMousePositionViaScrolling(screenPosition, mainPanel);
	AlterMousePositionViaZoom(screenPosition);

	const Player &myPlayer = *MyPlayer;

	AlterMousePositionViaPlayer(screenPosition, myPlayer);

	bool flipflag = false;
	Point currentTile = ConvertToTileGrid(screenPosition);

	ShiftToDiamondGridAlignment(screenPosition, currentTile, flipflag);

	if (CheckMouseHold(currentTile)) return;

	ResetCursorInfo();

	if (CheckPlayerState(currentTile, myPlayer) || CheckPanelsAndFlags(mainPanel) || CheckCursorActions(currentTile, flipflag)) return;

	// update cursor position
	cursPosition = currentTile;
	DisplayTriggerInfo();
}

} // namespace devilution
