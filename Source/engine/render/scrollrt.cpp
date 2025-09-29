/**
 * @file scrollrt.cpp
 *
 * Implementation of functionality for rendering the dungeons, monsters and calling other render routines.
 */
#include "engine/render/scrollrt.h"

#include <cstdint>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "dead.h"
#include "doom.h"
#include "engine/backbuffer_state.hpp"
#include "engine/dx.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/dun_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/trn.hpp"
#include "error.h"
#include "gmenu.h"
#include "help.h"
#include "hwcursor.hpp"
#include "init.h"
#include "inv.h"
#include "lighting.h"
#include "minitext.h"
#include "missiles.h"
#include "nthread.h"
#include "options.h"
#include "panels/charpanel.hpp"
#include "plrmsg.h"
#include "qol/chatlog.h"
#include "qol/floatingnumbers.h"
#include "qol/itemlabels.h"
#include "qol/monhealthbar.h"
#include "qol/stash.h"
#include "qol/xpbar.h"
#include "stores.h"
#include "towners.h"
#include "utils/bitset2d.hpp"
#include "utils/display.h"
#include "utils/endian_read.hpp"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

#ifndef USE_SDL1
#include "controls/touch/renderers.h"
#endif

#ifdef _DEBUG
#include "debug.h"
#endif

#ifdef DUN_RENDER_STATS
#include "utils/format_int.hpp"
#endif

namespace devilution {

/**
 * Specifies the current light entry.
 */
int LightTableIndex;

bool AutoMapShowItems;

// DevilutionX extension.
extern void DrawControllerModifierHints(const Surface &out);

bool frameflag;

namespace {

/**
 * @brief Hash algorithm for point
 */
struct PointHash {
	std::size_t operator()(Point const &s) const noexcept
	{
		return s.x ^ (s.y << 1);
	}
};

/**
 * @brief Contains all Missile at rendering position
 */
std::unordered_multimap<Point, Missile *, PointHash> MissilesAtRenderingTile;

/**
 * @brief Could the missile (at the next game tick) collide? This method is a simplified version of CheckMissileCol (for example without random).
 */
bool CouldMissileCollide(Point tile, bool checkPlayerAndMonster)
{
	if (!InDungeonBounds(tile))
		return true;
	if (checkPlayerAndMonster) {
		if (dMonster[tile.x][tile.y] > 0)
			return true;
		if (dPlayer[tile.x][tile.y] > 0)
			return true;
	}

	return IsMissileBlockedByTile(tile);
}

void UpdateMissilePositionForRendering(Missile &m, int progress)
{
	DisplacementOf<int64_t> velocity = m.position.velocity;
	velocity *= progress;
	velocity /= AnimationInfo::baseValueFraction;
	Displacement pixelsTravelled = (m.position.traveled + Displacement { static_cast<int>(velocity.deltaX), static_cast<int>(velocity.deltaY) }) >> 16;
	Displacement tileOffset = pixelsTravelled.screenToMissile();

	// calculcate the future missile position
	m.position.tileForRendering = m.position.start + tileOffset;
	m.position.offsetForRendering = pixelsTravelled + tileOffset.worldToScreen();
}

void UpdateMissileRendererData(Missile &m)
{
	m.position.tileForRendering = m.position.tile;
	m.position.offsetForRendering = m.position.offset;

	const MissileMovementDistribution missileMovement = GetMissileData(m._mitype).movementDistribution;
	// don't calculate missile position if they don't move
	if (missileMovement == MissileMovementDistribution::Disabled || m.position.velocity == Displacement {})
		return;

	int progress = ProgressToNextGameTick;
	UpdateMissilePositionForRendering(m, progress);

	// In some cases this calculcated position is invalid.
	// For example a missile shouldn't move inside a wall.
	// In this case the game logic don't advance the missile position and removes the missile or shows an explosion animation at the old position.
	// For the animation distribution logic this means we are not allowed to move to a tile where the missile could collide, cause this could be a invalid position.

	// If we are still at the current tile, this tile was already checked and is a valid tile
	if (m.position.tileForRendering == m.position.tile)
		return;

	// If no collision can happen at the new tile we can advance
	if (!CouldMissileCollide(m.position.tileForRendering, missileMovement == MissileMovementDistribution::Blockable))
		return;

	// The new tile could be invalid, so don't advance to it.
	// We search the last offset that is in the old (valid) tile.
	// Implementation note: If someone knows the correct math to calculate this without the loop, I would really appreciate it.
	while (m.position.tile != m.position.tileForRendering) {
		progress -= 1;

		if (progress <= 0) {
			m.position.tileForRendering = m.position.tile;
			m.position.offsetForRendering = m.position.offset;
			return;
		}

		UpdateMissilePositionForRendering(m, progress);
	}
}

void UpdateMissilesRendererData()
{
	MissilesAtRenderingTile.clear();

	for (auto &m : Missiles) {
		UpdateMissileRendererData(m);
		MissilesAtRenderingTile.insert(std::make_pair(m.position.tileForRendering, &m));
	}
}

/**
 * @brief Keeps track of which tiles have been rendered already.
 */
Bitset2d<MAXDUNX, MAXDUNY> dRendered;

int lastFpsUpdateInMs;

const char *const PlayerModeNames[] = {
	"standing",
	"walking (1)",
	"walking (2)",
	"walking (3)",
	"attacking (melee)",
	"attacking (ranged)",
	"blocking",
	"getting hit",
	"dying",
	"casting a spell",
	"changing levels",
	"quitting"
};

Rectangle PrevCursorRect;

void BlitCursor(uint8_t *dst, uint32_t dstPitch, uint8_t *src, uint32_t srcPitch, uint32_t srcWidth, uint32_t srcHeight)
{
	for (std::uint32_t i = 0; i < srcHeight; ++i, src += srcPitch, dst += dstPitch) {
		memcpy(dst, src, srcWidth);
	}
}

/**
 * @brief Remove the cursor from the buffer
 */
void UndrawCursor(const Surface &out)
{
	DrawnCursor &cursor = GetDrawnCursor();
	BlitCursor(&out[cursor.rect.position], out.pitch(), cursor.behindBuffer, cursor.rect.size.width, cursor.rect.size.width, cursor.rect.size.height);
	PrevCursorRect = cursor.rect;
}

bool ShouldShowCursor()
{
	if (ControlMode == ControlTypes::KeyboardAndMouse)
		return true;
	if (pcurs == CURSOR_TELEPORT)
		return true;
	if (invflag)
		return true;
	if (chrflag && MyPlayer->_pStatPts > 0)
		return true;

	return false;
}

/**
 * @brief Blit CL2 sprite, and apply lighting, to the given buffer at the given coordinates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
void ClxDrawLight(const Surface &out, Point position, ClxSprite clx, int lightTableIndex)
{
	if (lightTableIndex != 0) {
		ClxDrawTRN(out, position, clx, LightTables[lightTableIndex].data());
	} else {
		ClxDraw(out, position, clx);
	}
}

/**
 * @brief Blit CL2 sprite, and apply lighting and transparency blending, to the given buffer at the given coordinates
 * @param out Output buffer
 * @param position Target buffer coordinate
 * @param clx CLX frame
 */
void ClxDrawLightBlended(const Surface &out, Point position, ClxSprite clx, int lightTableIndex)
{
	if (lightTableIndex != 0) {
		ClxDrawBlendedTRN(out, position, clx, LightTables[lightTableIndex].data());
	} else {
		ClxDrawBlended(out, position, clx);
	}
}

/**
 * @brief Save the content behind the cursor to a temporary buffer, then draw the cursor.
 */
void DrawCursor(const Surface &out)
{
	DrawnCursor &cursor = GetDrawnCursor();
	if (IsHardwareCursor()) {
		SetHardwareCursorVisible(ShouldShowCursor());
		cursor.rect.size = { 0, 0 };
		return;
	}

	if (pcurs <= CURSOR_NONE || !ShouldShowCursor()) {
		cursor.rect.size = { 0, 0 };
		return;
	}

	Size cursSize = GetInvItemSize(pcurs);
	if (cursSize.width == 0 || cursSize.height == 0) {
		cursor.rect.size = { 0, 0 };
		return;
	}

	constexpr auto Clip = [](int &pos, int &length, int posEnd) {
		if (pos + length <= 0 || pos >= posEnd) {
			pos = 0;
			length = 0;
		} else if (pos < 0) {
			length += pos;
			pos = 0;
		} else if (pos + length > posEnd) {
			length = posEnd - pos;
		}
	};

	// Copy the buffer before the item cursor and its 1px outline are drawn to a temporary buffer.
	const int outlineWidth = !MyPlayer->HoldItem.isEmpty() ? 1 : 0;
	Displacement offset = !MyPlayer->HoldItem.isEmpty() ? Displacement { cursSize / 2 } : Displacement { 0 };
	Point cursPosition = MousePosition - offset;

	Rectangle &rect = cursor.rect;
	rect.position.x = cursPosition.x - outlineWidth;
	rect.size.width = cursSize.width + 2 * outlineWidth;
	Clip(rect.position.x, rect.size.width, out.w());

	rect.position.y = cursPosition.y - outlineWidth;
	rect.size.height = cursSize.height + 2 * outlineWidth;
	Clip(rect.position.y, rect.size.height, out.h());

	if (rect.size.width == 0 || rect.size.height == 0)
		return;

	BlitCursor(cursor.behindBuffer, rect.size.width, &out[rect.position], out.pitch(), rect.size.width, rect.size.height);
	DrawSoftwareCursor(out, cursPosition + Displacement { 0, cursSize.height - 1 }, pcurs);
}

/**
 * @brief Render a missile sprite
 * @param out Output buffer
 * @param missile Pointer to Missile struct
 * @param targetBufferPosition Output buffer coordinate
 * @param pre Is the sprite in the background
 */
void DrawMissilePrivate(const Surface &out, const Missile &missile, Point targetBufferPosition, bool pre, int lightTableIndex)
{
	if (missile._miPreFlag != pre || !missile._miDrawFlag)
		return;

	const Point missileRenderPosition { targetBufferPosition + missile.position.offsetForRendering - Displacement { missile._miAnimWidth2, 0 } };
	const ClxSprite sprite = (*missile._miAnimData)[missile._miAnimFrame - 1];
	if (missile._miUniqTrans != 0)
		ClxDrawTRN(out, missileRenderPosition, sprite, Monsters[missile._misource].uniqueMonsterTRN.get());
	else if (missile._miLightFlag)
		ClxDrawLight(out, missileRenderPosition, sprite, lightTableIndex);
	else
		ClxDraw(out, missileRenderPosition, sprite);
}

/**
 * @brief Render a missile sprites for a given tile
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param pre Is the sprite in the background
 */
void DrawMissile(const Surface &out, Point tilePosition, Point targetBufferPosition, bool pre, int lightTableIndex)
{
	const auto range = MissilesAtRenderingTile.equal_range(tilePosition);
	for (auto it = range.first; it != range.second; it++) {
		DrawMissilePrivate(out, *it->second, targetBufferPosition, pre, lightTableIndex);
	}
}

/**
 * @brief Render a monster sprite
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param monster Monster reference
 */
void DrawMonster(const Surface &out, Point tilePosition, Point targetBufferPosition, const Monster &monster)
{
	if (!monster.animInfo.sprites) {
		Log("Draw Monster \"{}\": NULL Cel Buffer", monster.name());
		return;
	}

	const ClxSprite sprite = monster.animInfo.currentSprite();

	if (!IsTileLit(tilePosition)) {
		ClxDrawTRN(out, targetBufferPosition, sprite, GetInfravisionTRN());
		return;
	}
	uint8_t *trn = nullptr;
	if (monster.isUnique())
		trn = monster.uniqueMonsterTRN.get();
	if (monster.mode == MonsterMode::Petrified)
		trn = GetStoneTRN();
	if (MyPlayer->_pInfraFlag && LightTableIndex > 8)
		trn = GetInfravisionTRN();
	if (trn != nullptr)
		ClxDrawTRN(out, targetBufferPosition, sprite, trn);
	else
		ClxDrawLight(out, targetBufferPosition, sprite, LightTableIndex);
}

/**
 * @brief Helper for rendering a specific player icon (Mana Shield or Reflect)
 */
void DrawPlayerIconHelper(const Surface &out, MissileGraphicID missileGraphicId, Point position, bool lighting, bool infraVision)
{
	position.x -= GetMissileSpriteData(missileGraphicId).animWidth2;

	const ClxSprite sprite = (*GetMissileSpriteData(missileGraphicId).sprites).list()[0];

	if (!lighting) {
		ClxDraw(out, position, sprite);
		return;
	}

	if (infraVision) {
		ClxDrawTRN(out, position, sprite, GetInfravisionTRN());
		return;
	}

	ClxDrawLight(out, position, sprite, LightTableIndex);
}

/**
 * @brief Helper for rendering player icons (Mana Shield and Reflect)
 * @param out Output buffer
 * @param player Player reference
 * @param position Output buffer coordinates
 * @param infraVision Should infravision be applied
 */
void DrawPlayerIcons(const Surface &out, const Player &player, Point position, bool infraVision)
{
	if (player.pManaShield)
		DrawPlayerIconHelper(out, MissileGraphicID::ManaShield, position, &player != MyPlayer, infraVision);
	if (player.wReflections > 0)
		DrawPlayerIconHelper(out, MissileGraphicID::Reflect, position + Displacement { 0, 16 }, &player != MyPlayer, infraVision);
}

/**
 * @brief Render a player sprite
 * @param out Output buffer
 * @param player Player reference
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawPlayer(const Surface &out, const Player &player, Point tilePosition, Point targetBufferPosition)
{
	if (!IsTileLit(tilePosition) && !MyPlayer->_pInfraFlag && !MyPlayer->isOnArenaLevel() && leveltype != DTYPE_TOWN) {
		return;
	}

	const ClxSprite sprite = player.previewCelSprite ? *player.previewCelSprite : player.AnimInfo.currentSprite();

	Point spriteBufferPosition = targetBufferPosition - Displacement { CalculateWidth2(sprite.width()), 0 };

	if (static_cast<size_t>(pcursplr) < Players.size() && &player == &Players[pcursplr])
		ClxDrawOutlineSkipColorZero(out, 165, spriteBufferPosition, sprite);

	if (&player == MyPlayer && IsNoneOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
		ClxDraw(out, spriteBufferPosition, sprite);
		DrawPlayerIcons(out, player, targetBufferPosition, false);
		return;
	}

	if (!IsTileLit(tilePosition) || ((MyPlayer->_pInfraFlag || MyPlayer->isOnArenaLevel()) && LightTableIndex > 8)) {
		ClxDrawTRN(out, spriteBufferPosition, sprite, GetInfravisionTRN());
		DrawPlayerIcons(out, player, targetBufferPosition, true);
		return;
	}

	int l = LightTableIndex;
	if (LightTableIndex < 5)
		LightTableIndex = 0;
	else
		LightTableIndex -= 5;

	ClxDrawLight(out, spriteBufferPosition, sprite, LightTableIndex);
	DrawPlayerIcons(out, player, targetBufferPosition, false);

	LightTableIndex = l;
}

/**
 * @brief Render a player sprite
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawDeadPlayer(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	dFlags[tilePosition.x][tilePosition.y] &= ~DungeonFlag::DeadPlayer;

	for (Player &player : Players) {
		if (player.plractive && player._pHitPoints == 0 && player.isOnActiveLevel() && player.position.tile == tilePosition) {
			dFlags[tilePosition.x][tilePosition.y] |= DungeonFlag::DeadPlayer;
			const Point playerRenderPosition { targetBufferPosition };
			DrawPlayer(out, player, tilePosition, playerRenderPosition);
		}
	}
}

/**
 * @brief Render an object sprite
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param pre Is the sprite in the background
 */
void DrawObject(const Surface &out, Point tilePosition, Point targetBufferPosition, bool pre)
{
	if (LightTableIndex >= LightsMax) {
		return;
	}

	Object *object = FindObjectAtPosition(tilePosition);
	if (object == nullptr) {
		return;
	}

	const Object &objectToDraw = *object;
	if (objectToDraw._oPreFlag != pre) {
		return;
	}

	const ClxSprite sprite = (*objectToDraw._oAnimData)[objectToDraw._oAnimFrame - 1];

	Point screenPosition = targetBufferPosition - Displacement { CalculateWidth2(sprite.width()), 0 };
	if (objectToDraw.position != tilePosition) {
		// drawing a large or offset object, calculate the correct position for the center of the sprite
		Displacement worldOffset = objectToDraw.position - tilePosition;
		screenPosition -= worldOffset.worldToScreen();
	}

	if (&objectToDraw == ObjectUnderCursor) {
		ClxDrawOutlineSkipColorZero(out, 194, screenPosition, sprite);
	}
	if (objectToDraw.applyLighting) {
		ClxDrawLight(out, screenPosition, sprite, LightTableIndex);
	} else {
		ClxDraw(out, screenPosition, sprite);
	}
}

static void DrawDungeon(const Surface & /*out*/, Point /*tilePosition*/, Point /*targetBufferPosition*/);

/**
 * @brief Render a cell
 * @param out Target buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinates
 */
void DrawCell(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	const uint16_t levelPieceId = dPiece[tilePosition.x][tilePosition.y];
	const MICROS *pMap = &DPieceMicros[levelPieceId];

	const uint8_t *tbl = LightTables[LightTableIndex].data();
#ifdef _DEBUG
	if (DebugPath && MyPlayer->IsPositionInPath(tilePosition))
		tbl = GetPauseTRN();
#endif

	bool transparency = TileHasAny(levelPieceId, TileProperties::Transparent) && TransList[dTransVal[tilePosition.x][tilePosition.y]];
#ifdef _DEBUG
	if ((SDL_GetModState() & KMOD_ALT) != 0)
		transparency = false;
#endif
	const bool foliage = !TileHasAny(levelPieceId, TileProperties::Solid);

	const auto getFirstTileMaskLeft = [=](TileType tile) -> MaskType {
		if (transparency) {
			switch (tile) {
			case TileType::LeftTrapezoid:
			case TileType::TransparentSquare:
				return TileHasAny(levelPieceId, TileProperties::TransparentLeft)
				    ? MaskType::Left
				    : MaskType::Solid;
			case TileType::LeftTriangle:
				return MaskType::Solid;
			default:
				return MaskType::Transparent;
			}
		}
		if (foliage)
			return MaskType::LeftFoliage;
		return MaskType::Solid;
	};

	const auto getFirstTileMaskRight = [=](TileType tile) -> MaskType {
		if (transparency) {
			switch (tile) {
			case TileType::RightTrapezoid:
			case TileType::TransparentSquare:
				return TileHasAny(levelPieceId, TileProperties::TransparentRight)
				    ? MaskType::Right
				    : MaskType::Solid;
			case TileType::RightTriangle:
				return MaskType::Solid;
			default:
				return MaskType::Transparent;
			}
		}
		if (foliage)
			return MaskType::RightFoliage;
		return MaskType::Solid;
	};

	// The first micro tile may be rendered with a foliage mask.
	// Only `TransparentSquare` tiles are rendered when `foliage` is true.
	{
		{
			const LevelCelBlock levelCelBlock { pMap->mt[0] };
			const TileType tileType = levelCelBlock.type();
			const MaskType maskType = getFirstTileMaskLeft(tileType);
			if (levelCelBlock.hasValue()) {
				if (maskType != MaskType::LeftFoliage || tileType == TileType::TransparentSquare) {
					RenderTile(out, targetBufferPosition,
					    levelCelBlock, maskType, tbl);
				}
			}
		}
		{
			const LevelCelBlock levelCelBlock { pMap->mt[1] };
			const TileType tileType = levelCelBlock.type();
			const MaskType maskType = getFirstTileMaskRight(tileType);
			if (levelCelBlock.hasValue()) {
				if (transparency || !foliage || levelCelBlock.type() == TileType::TransparentSquare) {
					if (maskType != MaskType::RightFoliage || tileType == TileType::TransparentSquare) {
						RenderTile(out, targetBufferPosition + Displacement { TILE_WIDTH / 2, 0 },
						    levelCelBlock, maskType, tbl);
					}
				}
			}
		}
		targetBufferPosition.y -= TILE_HEIGHT;
	}

	for (uint_fast8_t i = 2, n = MicroTileLen; i < n; i += 2) {
		{
			const LevelCelBlock levelCelBlock { pMap->mt[i] };
			if (levelCelBlock.hasValue()) {
				RenderTile(out, targetBufferPosition,
				    levelCelBlock,
				    transparency ? MaskType::Transparent : MaskType::Solid, tbl);
			}
		}
		{
			const LevelCelBlock levelCelBlock { pMap->mt[i + 1] };
			if (levelCelBlock.hasValue()) {
				RenderTile(out, targetBufferPosition + Displacement { TILE_WIDTH / 2, 0 },
				    levelCelBlock,
				    transparency ? MaskType::Transparent : MaskType::Solid, tbl);
			}
		}
		targetBufferPosition.y -= TILE_HEIGHT;
	}
}

/**
 * @brief Render a floor tile.
 * @param out Target buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinate
 */
void DrawFloor(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	LightTableIndex = dLight[tilePosition.x][tilePosition.y];

	const uint8_t *tbl = LightTables[LightTableIndex].data();
#ifdef _DEBUG
	if (DebugPath && MyPlayer->IsPositionInPath(tilePosition))
		tbl = GetPauseTRN();
#endif

	const uint16_t levelPieceId = dPiece[tilePosition.x][tilePosition.y];
	{
		const LevelCelBlock levelCelBlock { DPieceMicros[levelPieceId].mt[0] };
		if (levelCelBlock.hasValue()) {
			RenderTile(out, targetBufferPosition,
			    levelCelBlock, MaskType::Solid, tbl);
		}
	}
	{
		const LevelCelBlock levelCelBlock { DPieceMicros[levelPieceId].mt[1] };
		if (levelCelBlock.hasValue()) {
			RenderTile(out, targetBufferPosition + Displacement { TILE_WIDTH / 2, 0 },
			    levelCelBlock, MaskType::Solid, tbl);
		}
	}
}

/**
 * @brief Draw item for a given tile
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param pre Is the sprite in the background
 */
void DrawItem(const Surface &out, Point tilePosition, Point targetBufferPosition, bool pre)
{
	int8_t bItem = dItem[tilePosition.x][tilePosition.y];

	if (bItem <= 0)
		return;

	auto &item = Items[bItem - 1];
	if (item._iPostDraw == pre)
		return;

	const ClxSprite sprite = item.AnimInfo.currentSprite();
	int px = targetBufferPosition.x - CalculateWidth2(sprite.width());
	const Point position { px, targetBufferPosition.y };
	if (stextflag == TalkID::None && (bItem - 1 == pcursitem || AutoMapShowItems)) {
		ClxDrawOutlineSkipColorZero(out, GetOutlineColor(item, false), position, sprite);
	}
	ClxDrawLight(out, position, sprite, LightTableIndex);
	if (item.AnimInfo.isLastFrame() || item._iCurs == ICURS_MAGIC_ROCK)
		AddItemToLabelQueue(bItem - 1, position);
}

/**
 * @brief Check if and how a monster should be rendered
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawMonsterHelper(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	int mi = dMonster[tilePosition.x][tilePosition.y];
	bool isNegativeMonster = mi < 0;
	mi = abs(mi) - 1;

	if (leveltype == DTYPE_TOWN) {
		if (isNegativeMonster)
			return;
		auto &towner = Towners[mi];
		int px = targetBufferPosition.x - CalculateWidth2(towner._tAnimWidth);
		const Point position { px, targetBufferPosition.y };
		const ClxSprite sprite = towner.currentSprite();
		if (mi == pcursmonst) {
			ClxDrawOutlineSkipColorZero(out, 166, position, sprite);
		}
		ClxDraw(out, position, sprite);
		return;
	}

	if (!IsTileLit(tilePosition) && !MyPlayer->_pInfraFlag)
		return;

	if (static_cast<size_t>(mi) >= MaxMonsters) {
		Log("Draw Monster: tried to draw illegal monster {}", mi);
		return;
	}

	const auto &monster = Monsters[mi];
	if ((monster.flags & MFLAG_HIDDEN) != 0) {
		return;
	}

	const ClxSprite sprite = monster.animInfo.currentSprite();

	Displacement offset = {};
	if (monster.isWalking()) {
		bool isSideWalkingToLeft = monster.mode == MonsterMode::MoveSideways && monster.direction == Direction::West;
		if (isNegativeMonster && !isSideWalkingToLeft)
			return;
		if (!isNegativeMonster && isSideWalkingToLeft)
			return;
		offset = GetOffsetForWalking(monster.animInfo, monster.direction);
		if (isSideWalkingToLeft)
			offset -= Displacement { 64, 0 };
	} else if (isNegativeMonster) {
		return;
	}

	const Point monsterRenderPosition { targetBufferPosition + offset - Displacement { CalculateWidth2(sprite.width()), 0 } };
	if (mi == pcursmonst) {
		ClxDrawOutlineSkipColorZero(out, 233, monsterRenderPosition, sprite);
	}
	DrawMonster(out, tilePosition, monsterRenderPosition, monster);
}

/**
 * @brief Check if and how a player should be rendered
 * @param out Output buffer
 * @param player Player reference
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawPlayerHelper(const Surface &out, const Player &player, Point tilePosition, Point targetBufferPosition)
{
	Displacement offset = {};
	if (player.isWalking()) {
		offset = GetOffsetForWalking(player.AnimInfo, player._pdir);
	}

	const Point playerRenderPosition { targetBufferPosition + offset };

	DrawPlayer(out, player, tilePosition, playerRenderPosition);
}

/**
 * @brief Render object sprites
 * @param out Target buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinates
 */
void DrawDungeon(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	assert(InDungeonBounds(tilePosition));

	if (dRendered.test(tilePosition.x, tilePosition.y))
		return;
	dRendered.set(tilePosition.x, tilePosition.y);

	LightTableIndex = dLight[tilePosition.x][tilePosition.y];

	DrawCell(out, tilePosition, targetBufferPosition);

	int8_t bDead = dCorpse[tilePosition.x][tilePosition.y];
	int8_t bMap = dTransVal[tilePosition.x][tilePosition.y];

#ifdef _DEBUG
	if (DebugVision && IsTileLit(tilePosition)) {
		ClxDraw(out, targetBufferPosition, (*pSquareCel)[0]);
	}
#endif

	if (MissilePreFlag) {
		DrawMissile(out, tilePosition, targetBufferPosition, true, LightTableIndex);
	}

	if (LightTableIndex < LightsMax && bDead != 0) {
		Corpse &corpse = Corpses[(bDead & 0x1F) - 1];
		const Point position { targetBufferPosition.x - CalculateWidth2(corpse.width), targetBufferPosition.y };
		const ClxSprite sprite = corpse.spritesForDirection(static_cast<Direction>((bDead >> 5) & 7))[corpse.frame];
		if (corpse.translationPaletteIndex != 0) {
			const uint8_t *trn = Monsters[corpse.translationPaletteIndex - 1].uniqueMonsterTRN.get();
			ClxDrawTRN(out, position, sprite, trn);
		} else {
			ClxDrawLight(out, position, sprite, LightTableIndex);
		}
	}
	DrawObject(out, tilePosition, targetBufferPosition, true);
	DrawItem(out, tilePosition, targetBufferPosition, true);

	if (TileContainsDeadPlayer(tilePosition)) {
		DrawDeadPlayer(out, tilePosition, targetBufferPosition);
	}
	int8_t playerId = dPlayer[tilePosition.x][tilePosition.y];
	if (static_cast<size_t>(playerId - 1) < Players.size()) {
		DrawPlayerHelper(out, Players[playerId - 1], tilePosition, targetBufferPosition);
	}
	if (dMonster[tilePosition.x][tilePosition.y] != 0) {
		DrawMonsterHelper(out, tilePosition, targetBufferPosition);
	}
	DrawMissile(out, tilePosition, targetBufferPosition, false, LightTableIndex);
	DrawObject(out, tilePosition, targetBufferPosition, false);
	DrawItem(out, tilePosition, targetBufferPosition, false);

	if (leveltype != DTYPE_TOWN) {
		char bArch = dSpecial[tilePosition.x][tilePosition.y];
		if (bArch != 0) {
			bool transparency = TransList[bMap];
#ifdef _DEBUG
			// Turn transparency off here for debugging
			transparency = transparency && (SDL_GetModState() & KMOD_ALT) == 0;
#endif
			if (transparency) {
				ClxDrawLightBlended(out, targetBufferPosition, (*pSpecialCels)[bArch - 1], LightTableIndex);
			} else {
				ClxDrawLight(out, targetBufferPosition, (*pSpecialCels)[bArch - 1], LightTableIndex);
			}
		}
	} else {
		// Tree leaves should always cover player when entering or leaving the tile,
		// So delay the rendering until after the next row is being drawn.
		// This could probably have been better solved by sprites in screen space.
		if (tilePosition.x > 0 && tilePosition.y > 0 && targetBufferPosition.y > TILE_HEIGHT) {
			char bArch = dSpecial[tilePosition.x - 1][tilePosition.y - 1];
			if (bArch != 0) {
				ClxDraw(out, targetBufferPosition + Displacement { 0, -TILE_HEIGHT }, (*pSpecialCels)[bArch - 1]);
			}
		}
	}
}

/**
 * @brief Render a row of tiles
 * @param out Buffer to render to
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinates
 * @param rows Number of rows
 * @param columns Tile in a row
 */
void DrawFloor(const Surface &out, Point tilePosition, Point targetBufferPosition, int rows, int columns)
{
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			if (InDungeonBounds(tilePosition)) {
				if (!TileHasAny(dPiece[tilePosition.x][tilePosition.y], TileProperties::Solid))
					DrawFloor(out, tilePosition, targetBufferPosition);
			} else {
				world_draw_black_tile(out, targetBufferPosition.x, targetBufferPosition.y);
			}
			tilePosition += Direction::East;
			targetBufferPosition.x += TILE_WIDTH;
		}
		// Return to start of row
		tilePosition += Displacement(Direction::West) * columns;
		targetBufferPosition.x -= columns * TILE_WIDTH;

		// Jump to next row
		targetBufferPosition.y += TILE_HEIGHT / 2;
		if ((i & 1) != 0) {
			tilePosition.x++;
			columns--;
			targetBufferPosition.x += TILE_WIDTH / 2;
		} else {
			tilePosition.y++;
			columns++;
			targetBufferPosition.x -= TILE_WIDTH / 2;
		}
	}
}

bool IsWall(Point position)
{
	return TileHasAny(dPiece[position.x][position.y], TileProperties::Solid) || dSpecial[position.x][position.y] != 0;
}

/**
 * @brief Render a row of tile
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Buffer coordinates
 * @param rows Number of rows
 * @param columns Tile in a row
 */
void DrawTileContent(const Surface &out, Point tilePosition, Point targetBufferPosition, int rows, int columns)
{
	// Keep evaluating until MicroTiles can't affect screen
	rows += MicroTileLen;
	dRendered.reset();

	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++) {
			if (InDungeonBounds(tilePosition)) {
#ifdef _DEBUG
				DebugCoordsMap[tilePosition.x + tilePosition.y * MAXDUNX] = targetBufferPosition;
#endif
				if (tilePosition.x + 1 < MAXDUNX && tilePosition.y - 1 >= 0 && targetBufferPosition.x + TILE_WIDTH <= gnScreenWidth) {
					// Render objects behind walls first to prevent sprites, that are moving
					// between tiles, from poking through the walls as they exceed the tile bounds.
					// A proper fix for this would probably be to layout the sceen and render by
					// sprite screen position rather than tile position.
					if (IsWall(tilePosition) && (IsWall(tilePosition + Displacement { 1, 0 }) || (tilePosition.x > 0 && IsWall(tilePosition + Displacement { -1, 0 })))) { // Part of a wall aligned on the x-axis
						if (IsTileNotSolid(tilePosition + Displacement { 1, -1 }) && IsTileNotSolid(tilePosition + Displacement { 0, -1 })) {                              // Has walkable area behind it
							DrawDungeon(out, tilePosition + Direction::East, { targetBufferPosition.x + TILE_WIDTH, targetBufferPosition.y });
						}
					}
				}
				DrawDungeon(out, tilePosition, targetBufferPosition);
			}
			tilePosition += Direction::East;
			targetBufferPosition.x += TILE_WIDTH;
		}
		// Return to start of row
		tilePosition += Displacement(Direction::West) * columns;
		targetBufferPosition.x -= columns * TILE_WIDTH;

		// Jump to next row
		targetBufferPosition.y += TILE_HEIGHT / 2;
		if ((i & 1) != 0) {
			tilePosition.x++;
			columns--;
			targetBufferPosition.x += TILE_WIDTH / 2;
		} else {
			tilePosition.y++;
			columns++;
			targetBufferPosition.x -= TILE_WIDTH / 2;
		}
	}
}

/**
 * @brief Scale up the top left part of the buffer 2x.
 */
void Zoom(const Surface &out)
{
	int viewportWidth = out.w();
	int viewportOffsetX = 0;
	if (CanPanelsCoverView()) {
		if (IsLeftPanelOpen()) {
			viewportWidth -= SidePanelSize.width;
			viewportOffsetX = SidePanelSize.width;
		} else if (IsRightPanelOpen()) {
			viewportWidth -= SidePanelSize.width;
		}
	}

	// We round to even for the source width and height.
	// If the width / height was odd, we copy just one extra pixel / row later on.
	const int srcWidth = (viewportWidth + 1) / 2;
	const int doubleableWidth = viewportWidth / 2;
	const int srcHeight = (out.h() + 1) / 2;
	const int doubleableHeight = out.h() / 2;

	uint8_t *src = out.at(srcWidth - 1, srcHeight - 1);
	uint8_t *dst = out.at(viewportOffsetX + viewportWidth - 1, out.h() - 1);
	const bool oddViewportWidth = (viewportWidth % 2) == 1;

	for (int hgt = 0; hgt < doubleableHeight; hgt++) {
		// Double the pixels in the line.
		for (int i = 0; i < doubleableWidth; i++) {
			*dst-- = *src;
			*dst-- = *src;
			--src;
		}

		// Copy a single extra pixel if the output width is odd.
		if (oddViewportWidth) {
			*dst-- = *src;
			--src;
		}

		// Skip the rest of the source line.
		src -= (out.pitch() - srcWidth);

		// Double the line.
		memcpy(dst - out.pitch() + 1, dst + 1, viewportWidth);

		// Skip the rest of the destination line.
		dst -= 2 * out.pitch() - viewportWidth;
	}
	if ((out.h() % 2) == 1) {
		memcpy(dst - out.pitch() + 1, dst + 1, viewportWidth);
	}
}

Displacement tileOffset;
Displacement tileShift;
int tileColums;
int tileRows;

void CalcFirstTilePosition(Point &position, Displacement &offset)
{
	// Adjust by player offset and tile grid alignment
	Player &myPlayer = *MyPlayer;
	offset = tileOffset;
	if (myPlayer.isWalking())
		offset += GetOffsetForWalking(myPlayer.AnimInfo, myPlayer._pdir, true);

	position += tileShift;

	// Skip rendering parts covered by the panels
	if (CanPanelsCoverView() && (IsLeftPanelOpen() || IsRightPanelOpen())) {
		int multiplier = (*sgOptions.Graphics.zoom) ? 1 : 2;
		position += Displacement(Direction::East) * multiplier;
		offset.deltaX += -TILE_WIDTH * multiplier / 2 / 2;

		if (IsLeftPanelOpen() && !*sgOptions.Graphics.zoom) {
			offset.deltaX += SidePanelSize.width;
			// SidePanelSize.width accounted for in Zoom()
		}
	}

	// Draw areas moving in and out of the screen
	if (myPlayer.isWalking()) {
		switch (myPlayer._pdir) {
		case Direction::North:
		case Direction::NorthEast:
			offset.deltaY -= TILE_HEIGHT;
			position += Direction::North;
			break;
		case Direction::SouthWest:
		case Direction::West:
			offset.deltaX -= TILE_WIDTH;
			position += Direction::West;
			break;
		case Direction::NorthWest:
			offset.deltaX -= TILE_WIDTH / 2;
			offset.deltaY -= TILE_HEIGHT / 2;
			position += Direction::NorthWest;
		default:
			break;
		}
	}
}

/**
 * @brief Configure render and process screen rows
 * @param fullOut Buffer to render to
 * @param position First tile of view in dPiece coordinate
 * @param offset Amount to offset the rendering in screen space
 */
void DrawGame(const Surface &fullOut, Point position, Displacement offset)
{
	// Limit rendering to the view area
	const Surface &out = !*sgOptions.Graphics.zoom
	    ? fullOut.subregionY(0, gnViewportHeight)
	    : fullOut.subregionY(0, (gnViewportHeight + 1) / 2);

	int columns = tileColums;
	int rows = tileRows;

	// Skip rendering parts covered by the panels
	if (CanPanelsCoverView() && (IsLeftPanelOpen() || IsRightPanelOpen())) {
		columns -= (*sgOptions.Graphics.zoom) ? 2 : 4;
	}

	UpdateMissilesRendererData();

	// Draw areas moving in and out of the screen
	if (MyPlayer->isWalking()) {
		switch (MyPlayer->_pdir) {
		case Direction::NoDirection:
			break;
		case Direction::North:
		case Direction::South:
			rows += 2;
			break;
		case Direction::NorthEast:
			columns++;
			rows += 2;
			break;
		case Direction::East:
		case Direction::West:
			columns++;
			break;
		case Direction::SouthEast:
		case Direction::SouthWest:
		case Direction::NorthWest:
			columns++;
			rows++;
			break;
		}
	}

#ifdef DUN_RENDER_STATS
	DunRenderStats.clear();
#endif

	DrawFloor(out, position, Point {} + offset, rows, columns);
	DrawTileContent(out, position, Point {} + offset, rows, columns);

	if (*sgOptions.Graphics.zoom) {
		Zoom(fullOut.subregionY(0, gnViewportHeight));
	}

#ifdef DUN_RENDER_STATS
	std::vector<std::pair<DunRenderType, size_t>> sortedStats(DunRenderStats.begin(), DunRenderStats.end());
	std::sort(sortedStats.begin(), sortedStats.end(),
	    [](const std::pair<DunRenderType, size_t> &a, const std::pair<DunRenderType, size_t> &b) {
		    return a.first.maskType == b.first.maskType
		        ? static_cast<uint8_t>(a.first.tileType) < static_cast<uint8_t>(b.first.tileType)
		        : static_cast<uint8_t>(a.first.maskType) < static_cast<uint8_t>(b.first.maskType);
	    });
	Point pos { 100, 20 };
	for (size_t i = 0; i < sortedStats.size(); ++i) {
		const auto &stat = sortedStats[i];
		DrawString(out, StrCat(i, "."), Rectangle(pos, Size { 20, 16 }), { UiFlags::AlignRight });
		DrawString(out, MaskTypeToString(stat.first.maskType), { pos.x + 24, pos.y });
		DrawString(out, TileTypeToString(stat.first.tileType), { pos.x + 184, pos.y });
		DrawString(out, FormatInteger(stat.second), Rectangle({ pos.x + 354, pos.y }, Size(40, 16)), { UiFlags::AlignRight });
		pos.y += 16;
	}
#endif
}

/**
 * @brief Start rendering of screen, town variation
 * @param out Buffer to render to
 * @param startPosition Center of view in dPiece coordinates
 */
void DrawView(const Surface &out, Point startPosition)
{
#ifdef _DEBUG
	DebugCoordsMap.clear();
#endif
	Displacement offset = {};
	CalcFirstTilePosition(startPosition, offset);
	DrawGame(out, startPosition, offset);
	if (AutomapActive) {
		DrawAutomap(out.subregionY(0, gnViewportHeight));
	}
#ifdef _DEBUG
	bool debugGridTextNeeded = IsDebugGridTextNeeded();
	if (debugGridTextNeeded || DebugGrid) {
		// force redrawing or debug stuff stays on panel on 640x480 resolution
		RedrawEverything();
		char debugGridTextBuffer[10];
		bool megaTiles = IsDebugGridInMegatiles();

		for (auto m : DebugCoordsMap) {
			Point dunCoords = { m.first % MAXDUNX, m.first / MAXDUNX };
			if (megaTiles && (dunCoords.x % 2 == 1 || dunCoords.y % 2 == 1))
				continue;
			Point pixelCoords = m.second;
			if (megaTiles)
				pixelCoords += Displacement { 0, TILE_HEIGHT / 2 };
			if (*sgOptions.Graphics.zoom)
				pixelCoords *= 2;
			if (debugGridTextNeeded && GetDebugGridText(dunCoords, debugGridTextBuffer)) {
				Size tileSize = { TILE_WIDTH, TILE_HEIGHT };
				if (*sgOptions.Graphics.zoom)
					tileSize *= 2;
				DrawString(out, debugGridTextBuffer, { pixelCoords - Displacement { 0, tileSize.height }, tileSize }, { UiFlags::ColorRed | UiFlags::AlignCenter | UiFlags::VerticalCenter });
			}
			if (DebugGrid) {
				auto DrawDebugSquare = [&out](Point center, Displacement hor, Displacement ver, uint8_t col) {
					auto DrawLine = [&out](Point from, Point to, uint8_t col) {
						int dx = to.x - from.x;
						int dy = to.y - from.y;
						int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
						float ix = dx / (float)steps;
						float iy = dy / (float)steps;
						float sx = from.x;
						float sy = from.y;

						for (int i = 0; i <= steps; i++, sx += ix, sy += iy)
							out.SetPixel({ (int)sx, (int)sy }, col);
					};
					DrawLine(center - hor, center + ver, col);
					DrawLine(center + hor, center + ver, col);
					DrawLine(center - hor, center - ver, col);
					DrawLine(center + hor, center - ver, col);
				};

				Displacement hor = { TILE_WIDTH / 2, 0 };
				Displacement ver = { 0, TILE_HEIGHT / 2 };
				if (*sgOptions.Graphics.zoom) {
					hor *= 2;
					ver *= 2;
				}
				Point center = pixelCoords + hor - ver;

				if (megaTiles) {
					hor *= 2;
					ver *= 2;
				}

				uint8_t col = PAL16_BEIGE;

				DrawDebugSquare(center, hor, ver, col);
			}
		}
	}
#endif
	DrawItemNameLabels(out);
	DrawMonsterHealthBar(out);
	DrawFloatingNumbers(out, startPosition, offset);

	if (stextflag != TalkID::None && !qtextflag)
		DrawSText(out);
	if (invflag) {
		DrawInv(out);
	} else if (sbookflag) {
		DrawSpellBook(out);
	}

	DrawDurIcon(out);

	if (chrflag) {
		DrawChr(out);
	} else if (QuestLogIsOpen) {
		DrawQuestLog(out);
	} else if (IsStashOpen) {
		DrawStash(out);
	}
	DrawLevelUpIcon(out);
	if (ShowUniqueItemInfoBox) {
		DrawUniqueInfo(out);
	}
	if (qtextflag) {
		DrawQText(out);
	}
	if (spselflag) {
		DrawSpellList(out);
	}
	if (DropGoldFlag) {
		DrawGoldSplit(out);
	}
	DrawGoldWithdraw(out);
	if (HelpFlag) {
		DrawHelp(out);
	}
	if (ChatLogFlag) {
		DrawChatLog(out);
	}
	if (IsDiabloMsgAvailable()) {
		DrawDiabloMsg(out);
	}
	if (MyPlayerIsDead) {
		RedBack(out);
	} else if (PauseMode != 0) {
		gmenu_draw_pause(out);
	}

	DrawControllerModifierHints(out);
	DrawPlrMsg(out);
	gmenu_draw(out);
	doom_draw(out);
	DrawInfoBox(out);
	control_update_life_mana(); // Update life/mana totals before rendering any portion of the flask.
	DrawLifeFlaskUpper(out);
	DrawManaFlaskUpper(out);
}

/**
 * @brief Display the current average FPS over 1 sec
 */
void DrawFPS(const Surface &out)
{
	static int framesSinceLastUpdate = 0;
	static string_view formatted {};

	if (!frameflag || !gbActive) {
		return;
	}

	framesSinceLastUpdate++;
	uint32_t runtimeInMs = SDL_GetTicks();
	uint32_t msSinceLastUpdate = runtimeInMs - lastFpsUpdateInMs;
	if (msSinceLastUpdate >= 1000) {
		lastFpsUpdateInMs = runtimeInMs;
		constexpr int FpsPow10 = 10;
		const int fps = 1000 * FpsPow10 * framesSinceLastUpdate / msSinceLastUpdate;
		framesSinceLastUpdate = 0;

		static char buf[15] {};
		const char *end = fps >= 100 * FpsPow10
		    ? BufCopy(buf, fps / FpsPow10, " FPS")
		    : BufCopy(buf, fps / FpsPow10, ".", fps % FpsPow10, " FPS");
		formatted = { buf, static_cast<string_view::size_type>(end - buf) };
	};
	DrawString(out, formatted, Point { 8, 68 }, { UiFlags::ColorRed });
}

/**
 * @brief Update part of the screen from the back buffer
 * @param x Back buffer coordinate
 * @param y Back buffer coordinate
 * @param w Back buffer coordinate
 * @param h Back buffer coordinate
 */
void DoBlitScreen(int x, int y, int w, int h)
{
#ifdef DEBUG_DO_BLIT_SCREEN
	const Surface &out = GlobalBackBuffer();
	const uint8_t debugColor = PAL8_RED;
	DrawHorizontalLine(out, Point(x, y), w, debugColor);
	DrawHorizontalLine(out, Point(x, y + h - 1), w, debugColor);
	DrawVerticalLine(out, Point(x, y), h, debugColor);
	DrawVerticalLine(out, Point(x + w - 1, y), h, debugColor);
#endif
	SDL_Rect srcRect = MakeSdlRect(x, y, w, h);
	SDL_Rect dstRect = MakeSdlRect(x, y, w, h);
	BltFast(&srcRect, &dstRect);
}

/**
 * @brief Check render pipeline and update individual screen parts
 * @param out Output surface.
 * @param dwHgt Section of screen to update from top to bottom
 * @param drawDesc Render info box
 * @param drawHp Render health bar
 * @param drawMana Render mana bar
 * @param drawSbar Render belt
 * @param drawBtn Render panel buttons
 */
void DrawMain(const Surface &out, int dwHgt, bool drawDesc, bool drawHp, bool drawMana, bool drawSbar, bool drawBtn)
{
	if (!gbActive || RenderDirectlyToOutputSurface) {
		return;
	}

	assert(dwHgt >= 0 && dwHgt <= gnScreenHeight);

	if (dwHgt > 0) {
		DoBlitScreen(0, 0, gnScreenWidth, dwHgt);
	}
	if (dwHgt < gnScreenHeight) {
		const Point mainPanelPosition = GetMainPanel().position;
		if (drawSbar) {
			DoBlitScreen(mainPanelPosition.x + 204, mainPanelPosition.y + 5, 232, 28);
		}
		if (drawDesc) {
			if (talkflag) {
				// When chat input is displayed, the belt is hidden and the chat moves up.
				DoBlitScreen(mainPanelPosition.x + 171, mainPanelPosition.y + 6, 298, 116);
			} else {
				DoBlitScreen(mainPanelPosition.x + InfoBoxTopLeft.deltaX, mainPanelPosition.y + InfoBoxTopLeft.deltaY,
				    InfoBoxSize.width, InfoBoxSize.height);
			}
		}
		if (drawMana) {
			DoBlitScreen(mainPanelPosition.x + 460, mainPanelPosition.y, 88, 72);
			DoBlitScreen(mainPanelPosition.x + 564, mainPanelPosition.y + 64, 56, 56);
		}
		if (drawHp) {
			DoBlitScreen(mainPanelPosition.x + 96, mainPanelPosition.y, 88, 72);
		}
		if (drawBtn) {
			DoBlitScreen(mainPanelPosition.x + 8, mainPanelPosition.y + 7, 74, 114);
			DoBlitScreen(mainPanelPosition.x + 559, mainPanelPosition.y + 7, 74, 48);
			if (gbIsMultiplayer) {
				DoBlitScreen(mainPanelPosition.x + 86, mainPanelPosition.y + 91, 34, 32);
				DoBlitScreen(mainPanelPosition.x + 526, mainPanelPosition.y + 91, 34, 32);
			}
		}
		if (PrevCursorRect.size.width != 0 && PrevCursorRect.size.height != 0) {
			DoBlitScreen(PrevCursorRect.position.x, PrevCursorRect.position.y, PrevCursorRect.size.width, PrevCursorRect.size.height);
		}
		Rectangle &cursorRect = GetDrawnCursor().rect;
		if (cursorRect.size.width != 0 && cursorRect.size.height != 0) {
			DoBlitScreen(cursorRect.position.x, cursorRect.position.y, cursorRect.size.width, cursorRect.size.height);
		}
	}
}

} // namespace

Displacement GetOffsetForWalking(const AnimationInfo &animationInfo, const Direction dir, bool cameraMode /*= false*/)
{
	// clang-format off
	//                                           South,        SouthWest,    West,         NorthWest,    North,        NorthEast,     East,         SouthEast,
	constexpr Displacement StartOffset[8]    = { {   0, -32 }, {  32, -16 }, {  64,   0 }, {   0,   0 }, {   0,   0 }, {  0,    0 },  { -64,   0 }, { -32, -16 } };
	constexpr Displacement MovingOffset[8]   = { {   0,  32 }, { -32,  16 }, { -64,   0 }, { -32, -16 }, {   0, -32 }, {  32, -16 },  {  64,   0 }, {  32,  16 } };
	// clang-format on

	uint8_t animationProgress = animationInfo.getAnimationProgress();
	Displacement offset = MovingOffset[static_cast<size_t>(dir)];
	offset *= animationProgress;
	offset /= AnimationInfo::baseValueFraction;

	if (cameraMode) {
		offset = -offset;
	} else {
		offset += StartOffset[static_cast<size_t>(dir)];
	}

	return offset;
}

void ClearCursor() // CODE_FIX: this was supposed to be in cursor.cpp
{
	PrevCursorRect = {};
}

void ShiftGrid(int *x, int *y, int horizontal, int vertical)
{
	*x += vertical + horizontal;
	*y += vertical - horizontal;
}

int RowsCoveredByPanel()
{
	auto &mainPanelSize = GetMainPanel().size;
	if (GetScreenWidth() <= mainPanelSize.width) {
		return 0;
	}

	int rows = mainPanelSize.height / TILE_HEIGHT;
	if (*sgOptions.Graphics.zoom) {
		rows /= 2;
	}

	return rows;
}

void CalcTileOffset(int *offsetX, int *offsetY)
{
	uint16_t screenWidth = GetScreenWidth();
	uint16_t viewportHeight = GetViewportHeight();

	int x;
	int y;

	if (!*sgOptions.Graphics.zoom) {
		x = screenWidth % TILE_WIDTH;
		y = viewportHeight % TILE_HEIGHT;
	} else {
		x = (screenWidth / 2) % TILE_WIDTH;
		y = (viewportHeight / 2) % TILE_HEIGHT;
	}

	if (x != 0)
		x = (TILE_WIDTH - x) / 2;
	if (y != 0)
		y = (TILE_HEIGHT - y) / 2;

	*offsetX = x;
	*offsetY = y;
}

void TilesInView(int *rcolumns, int *rrows)
{
	uint16_t screenWidth = GetScreenWidth();
	uint16_t viewportHeight = GetViewportHeight();

	int columns = screenWidth / TILE_WIDTH;
	if ((screenWidth % TILE_WIDTH) != 0) {
		columns++;
	}
	int rows = viewportHeight / TILE_HEIGHT;
	if ((viewportHeight % TILE_HEIGHT) != 0) {
		rows++;
	}

	if (*sgOptions.Graphics.zoom) {
		// Half the number of tiles, rounded up
		if ((columns & 1) != 0) {
			columns++;
		}
		columns /= 2;
		if ((rows & 1) != 0) {
			rows++;
		}
		rows /= 2;
	}

	*rcolumns = columns;
	*rrows = rows;
}

void CalcViewportGeometry()
{
	const int zoomFactor = *sgOptions.Graphics.zoom ? 2 : 1;
	const int screenWidth = GetScreenWidth() / zoomFactor;
	const int screenHeight = GetScreenHeight() / zoomFactor;
	const int panelHeight = GetMainPanel().size.height / zoomFactor;
	const int pixelsToPanel = screenHeight - panelHeight;
	Point playerPosition { screenWidth / 2, pixelsToPanel / 2 };

	if (*sgOptions.Graphics.zoom)
		playerPosition.y += TILE_HEIGHT / 4;

	const int tilesToTop = (playerPosition.y + TILE_HEIGHT - 1) / TILE_HEIGHT;
	const int tilesToLeft = (playerPosition.x + TILE_WIDTH - 1) / TILE_WIDTH;

	// Location of the center of the tile from which to start rendering, relative to the viewport origin
	Point startPosition = playerPosition - Displacement { tilesToLeft * TILE_WIDTH, tilesToTop * TILE_HEIGHT };

	// Position of the tile from which to start rendering in tile space,
	// relative to the tile the player character occupies
	tileShift = { 0, 0 };
	tileShift += Displacement(Direction::North) * tilesToTop;
	tileShift += Displacement(Direction::West) * tilesToLeft;

	// The rendering loop expects to start on a row with fewer columns
	if (tilesToLeft * TILE_WIDTH >= playerPosition.x) {
		startPosition += Displacement { TILE_WIDTH / 2, -TILE_HEIGHT / 2 };
		tileShift += Displacement(Direction::NorthEast);
	} else if (tilesToTop * TILE_HEIGHT < playerPosition.y) {
		// There is one row above the current row that needs to be rendered,
		// but we skip to the row above it because it has too many columns
		startPosition += Displacement { 0, -TILE_HEIGHT };
		tileShift += Displacement(Direction::North);
	}

	// Location of the bottom-left corner of the bounding box around the
	// tile from which to start rendering, relative to the viewport origin
	tileOffset = { startPosition.x - TILE_WIDTH / 2, startPosition.y + TILE_HEIGHT / 2 - 1 };

	// Compute the number of rows to be rendered as well as
	// the number of columns to be rendered in the first row
	const int viewportHeight = GetViewportHeight() / zoomFactor;
	const Point renderStart = startPosition - Displacement { TILE_WIDTH / 2, TILE_HEIGHT / 2 };
	tileRows = (viewportHeight - renderStart.y + TILE_HEIGHT / 2 - 1) / (TILE_HEIGHT / 2);
	tileColums = (screenWidth - renderStart.x + TILE_WIDTH - 1) / TILE_WIDTH;
}

extern SDL_Surface *PalSurface;

void ClearScreenBuffer()
{
	if (HeadlessMode)
		return;

	assert(PalSurface != nullptr);
	SDL_FillRect(PalSurface, nullptr, 0);
}

#ifdef _DEBUG
void ScrollView()
{
	if (!MyPlayer->HoldItem.isEmpty())
		return;

	if (MousePosition.x < 20) {
		if (dmaxPosition.y - 1 <= ViewPosition.y || dminPosition.x >= ViewPosition.x) {
			if (dmaxPosition.y - 1 > ViewPosition.y) {
				ViewPosition.y++;
			}
			if (dminPosition.x < ViewPosition.x) {
				ViewPosition.x--;
			}
		} else {
			ViewPosition.y++;
			ViewPosition.x--;
		}
	}
	if (MousePosition.x > gnScreenWidth - 20) {
		if (dmaxPosition.x - 1 <= ViewPosition.x || dminPosition.y >= ViewPosition.y) {
			if (dmaxPosition.x - 1 > ViewPosition.x) {
				ViewPosition.x++;
			}
			if (dminPosition.y < ViewPosition.y) {
				ViewPosition.y--;
			}
		} else {
			ViewPosition.y--;
			ViewPosition.x++;
		}
	}
	if (MousePosition.y < 20) {
		if (dminPosition.y >= ViewPosition.y || dminPosition.x >= ViewPosition.x) {
			if (dminPosition.y < ViewPosition.y) {
				ViewPosition.y--;
			}
			if (dminPosition.x < ViewPosition.x) {
				ViewPosition.x--;
			}
		} else {
			ViewPosition.x--;
			ViewPosition.y--;
		}
	}
	if (MousePosition.y > gnScreenHeight - 20) {
		if (dmaxPosition.y - 1 <= ViewPosition.y || dmaxPosition.x - 1 <= ViewPosition.x) {
			if (dmaxPosition.y - 1 > ViewPosition.y) {
				ViewPosition.y++;
			}
			if (dmaxPosition.x - 1 > ViewPosition.x) {
				ViewPosition.x++;
			}
		} else {
			ViewPosition.x++;
			ViewPosition.y++;
		}
	}
}
#endif

void EnableFrameCount()
{
	frameflag = true;
	lastFpsUpdateInMs = SDL_GetTicks();
}

void scrollrt_draw_game_screen()
{
	if (HeadlessMode)
		return;

	int hgt = 0;

	if (IsRedrawEverything()) {
		RedrawComplete();
		hgt = gnScreenHeight;
	}

	const Surface &out = GlobalBackBuffer();
	UndrawCursor(out);
	DrawMain(out, hgt, false, false, false, false, false);
	DrawCursor(out);

	RenderPresent();
}

void DrawAndBlit()
{
	if (!gbRunGame || HeadlessMode) {
		return;
	}

	int hgt = 0;
	bool drawHealth = IsRedrawComponent(PanelDrawComponent::Health);
	bool drawMana = IsRedrawComponent(PanelDrawComponent::Mana);
	bool drawControlButtons = IsRedrawComponent(PanelDrawComponent::ControlButtons);
	bool drawBelt = IsRedrawComponent(PanelDrawComponent::Belt);
	bool drawChatInput = talkflag;
	bool drawInfoBox = false;
	bool drawCtrlPan = false;

	const Rectangle &mainPanel = GetMainPanel();

	if (gnScreenWidth > mainPanel.size.width || IsRedrawEverything()) {
		drawHealth = true;
		drawMana = true;
		drawControlButtons = true;
		drawBelt = true;
		drawInfoBox = false;
		drawCtrlPan = true;
		hgt = gnScreenHeight;
	} else if (IsRedrawViewport()) {
		drawInfoBox = true;
		drawCtrlPan = false;
		hgt = gnViewportHeight;
	}

	const Surface &out = GlobalBackBuffer();
	UndrawCursor(out);

	nthread_UpdateProgressToNextGameTick();

	DrawView(out, ViewPosition);
	if (drawCtrlPan) {
		DrawCtrlPan(out);
	}
	if (drawHealth) {
		DrawLifeFlaskLower(out);
	}
	if (drawMana) {
		DrawManaFlaskLower(out);

		DrawSpell(out);
	}
	if (drawControlButtons) {
		DrawCtrlBtns(out);
	}
	if (drawBelt) {
		DrawInvBelt(out);
	}
	if (drawChatInput) {
		DrawTalkPan(out);
	}
	DrawXPBar(out);
	if (*sgOptions.Gameplay.showHealthValues)
		DrawFlaskValues(out, { mainPanel.position.x + 134, mainPanel.position.y + 28 }, MyPlayer->_pHitPoints >> 6, MyPlayer->_pMaxHP >> 6);
	if (*sgOptions.Gameplay.showManaValues)
		DrawFlaskValues(out, { mainPanel.position.x + mainPanel.size.width - 138, mainPanel.position.y + 28 }, MyPlayer->_pMana >> 6, MyPlayer->_pMaxMana >> 6);

	DrawCursor(out);

	DrawFPS(out);

	DrawMain(out, hgt, drawInfoBox, drawHealth, drawMana, drawBelt, drawControlButtons);

	RedrawComplete();
	for (PanelDrawComponent component : enum_values<PanelDrawComponent>()) {
		if (IsRedrawComponent(component)) {
			RedrawComponentComplete(component);
		}
	}

	RenderPresent();
}

} // namespace devilution
