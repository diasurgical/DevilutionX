/**
 * @file scrollrt.cpp
 *
 * Implementation of functionality for rendering the dungeons, monsters and calling other render routines.
 */
#include "engine/render/scrollrt.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include <ankerl/unordered_dense.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "controls/control_mode.hpp"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "dead.h"
#include "diablo_msg.hpp"
#include "doom.h"
#include "engine/backbuffer_state.hpp"
#include "engine/displacement.hpp"
#include "engine/dx.h"
#include "engine/point.hpp"
#include "engine/render/dun_render.hpp"
#include "engine/render/renderer.h"
#include "engine/render/text_render.hpp"
#include "engine/trn.hpp"
#include "engine/world_tile.hpp"
#include "game_mode.hpp"
#include "gmenu.h"
#include "headless_mode.hpp"
#include "help.h"
#include "hwcursor.hpp"
#include "init.hpp"
#include "inv.h"
#include "levels/dun_tile.hpp"
#include "levels/gendung.h"
#include "levels/tile_properties.hpp"
#include "lighting.h"
#include "lua/lua_event.hpp"
#include "minitext.h"
#include "missiles.h"
#include "nthread.h"
#include "options.h"
#include "panels/charpanel.hpp"
#include "panels/console.hpp"
#include "panels/partypanel.hpp"
#include "panels/spell_list.hpp"
#include "plrmsg.h"
#include "qol/chatlog.h"
#include "qol/floatingnumbers.h"
#include "qol/itemlabels.h"
#include "qol/monhealthbar.h"
#include "qol/stash.h"
#include "qol/visual_store.h"
#include "qol/xpbar.h"
#include "stores.h"
#include "towners.h"
#include "utils/attributes.h"
#include "utils/display.h"
#include "utils/is_of.hpp"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"
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

enum OutlineColors : uint8_t {
	OutlineColorsPlayer1 = (PAL16_ORANGE + 7),
	OutlineColorsPlayer2 = (PAL16_YELLOW + 7),
	OutlineColorsPlayer3 = (PAL16_RED + 7),
	OutlineColorsPlayer4 = (PAL16_BLUE + 7),
	OutlineColorsObject = (PAL16_YELLOW + 2),
	OutlineColorsTowner = (PAL16_BEIGE + 6),
	OutlineColorsMonster = (PAL16_RED + 9),
};

bool AutoMapShowItems;

// DevilutionX extension.
extern void DrawControllerModifierHints();

bool frameflag;

namespace {

constexpr auto RightFrameDisplacement = Displacement { DunFrameWidth, 0 };

[[nodiscard]] DVL_ALWAYS_INLINE bool IsFloor(Point tilePosition)
{
	return !TileHasAny(tilePosition, TileProperties::Solid | TileProperties::BlockMissile);
}

[[nodiscard]] DVL_ALWAYS_INLINE bool IsWall(Point tilePosition)
{
	return !IsFloor(tilePosition) || dSpecial[tilePosition.x][tilePosition.y] != 0;
}

/**
 * @brief Contains all Missile at rendering position
 */
ankerl::unordered_dense::map<WorldTilePosition, std::vector<Missile *>> MissilesAtRenderingTile;

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
	const Displacement pixelsTravelled = (m.position.traveled + Displacement { static_cast<int>(velocity.deltaX), static_cast<int>(velocity.deltaY) }) >> 16;
	const Displacement tileOffset = pixelsTravelled.screenToMissile();

	// calculate the future missile position
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

	// In some cases this calculated position is invalid.
	// For example a missile shouldn't move inside a wall.
	// In this case the game logic don't advance the missile position and removes the missile or shows an explosion animation at the old position.
	// For the animation distribution logic this means we are not allowed to move to a tile where the missile could collide, because this could be a invalid position.

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
		MissilesAtRenderingTile[m.position.tileForRendering].push_back(&m);
	}
}

uint32_t lastFpsUpdateInMs;

bool ShouldShowCursor()
{
	if (ControlMode == ControlTypes::KeyboardAndMouse)
		return true;
	if (pcurs == CURSOR_TELEPORT)
		return true;
	if (invflag)
		return true;
	if (CharFlag && MyPlayer->_pStatPts > 0)
		return true;

	return false;
}

/**
 * @brief Update cursor visibility for both hardware and software cursor paths.
 */
void UpdateCursorVisibility()
{
	if (IsHardwareCursor()) {
		SetHardwareCursorVisible(ShouldShowCursor());
		return;
	}

	// The renderer composites the cursor internally in EndFrame().
	GetRenderer().SetCursorVisible(ShouldShowCursor());
}

/**
 * @brief Render a missile sprite
 * @param out Output buffer
 * @param missile Pointer to Missile struct
 * @param targetBufferPosition Output buffer coordinate
 * @param pre Is the sprite in the background
 */
void DrawMissilePrivate(const Missile &missile, Point targetBufferPosition, bool pre, int lightTableIndex)
{
	if (missile._miPreFlag != pre || !missile._miDrawFlag)
		return;

	const Point missileRenderPosition { targetBufferPosition + missile.position.offsetForRendering - Displacement { missile._miAnimWidth2, 0 } };
	const ClxSprite sprite = (*missile._miAnimData)[missile._miAnimFrame - 1];
	if (missile._miUniqTrans != 0) {
		GetRenderer().DrawClxTRN(missileRenderPosition, sprite, Monsters[missile._misource].uniqueMonsterTRN.get());
	} else if (missile._miLightFlag) {
		GetRenderer().DrawClxLit(missileRenderPosition, sprite, lightTableIndex);
	} else {
		GetRenderer().DrawClx(missileRenderPosition, sprite);
	}
}

/**
 * @brief Render a missile sprites for a given tile
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param pre Is the sprite in the background
 */
void DrawMissile(WorldTilePosition tilePosition, Point targetBufferPosition, bool pre, int lightTableIndex)
{
	const auto it = MissilesAtRenderingTile.find(tilePosition);
	if (it == MissilesAtRenderingTile.end()) return;
	for (Missile *missile : it->second) {
		DrawMissilePrivate(*missile, targetBufferPosition, pre, lightTableIndex);
	}
}

/**
 * @brief Render a monster sprite
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param monster Monster reference
 */
void DrawMonster(Point tilePosition, Point targetBufferPosition, const Monster &monster, int lightTableIndex)
{
	if (!monster.animInfo.sprites) {
		Log("Draw Monster \"{}\": NULL Cel Buffer", monster.name());
		return;
	}

	const ClxSprite sprite = monster.animInfo.currentSprite();

	if (!IsTileLit(tilePosition)) {
		GetRenderer().DrawClxTRN(targetBufferPosition, sprite, GetInfravisionTRN());
		return;
	}
	uint8_t *trn = nullptr;
	if (monster.isUnique())
		trn = monster.uniqueMonsterTRN.get();
	if (monster.mode == MonsterMode::Petrified)
		trn = GetStoneTRN();
	if (MyPlayer->_pInfraFlag && lightTableIndex > 8)
		trn = GetInfravisionTRN();
	if (trn != nullptr)
		GetRenderer().DrawClxTRN(targetBufferPosition, sprite, trn);
	else
		GetRenderer().DrawClxLit(targetBufferPosition, sprite, lightTableIndex);
}

/**
 * @brief Helper for rendering a specific player icon (Mana Shield or Reflect)
 */
void DrawPlayerIconHelper(MissileGraphicID missileGraphicId, Point position, const Player &player, bool infraVision, int lightTableIndex)
{
	const bool lighting = &player != MyPlayer;

	if (player.isWalking())
		position += GetOffsetForWalking(player.AnimInfo, player._pdir);

	position.x -= GetMissileSpriteData(missileGraphicId).animWidth2;

	const ClxSprite sprite = (*GetMissileSpriteData(missileGraphicId).sprites).list()[0];

	if (!lighting) {
		GetRenderer().DrawClx(position, sprite);
		return;
	}

	if (infraVision) {
		GetRenderer().DrawClxTRN(position, sprite, GetInfravisionTRN());
		return;
	}

	GetRenderer().DrawClxLit(position, sprite, lightTableIndex);
}

/**
 * @brief Helper for rendering player icons (Mana Shield and Reflect)
 * @param player Player reference
 * @param position Output buffer coordinates
 * @param infraVision Should infravision be applied
 */
void DrawPlayerIcons(const Player &player, Point position, bool infraVision, int lightTableIndex)
{
	if (player.pManaShield)
		DrawPlayerIconHelper(MissileGraphicID::ManaShield, position, player, infraVision, lightTableIndex);
	if (player.wReflections > 0)
		DrawPlayerIconHelper(MissileGraphicID::Reflect, position + Displacement { 0, 16 }, player, infraVision, lightTableIndex);
}

uint8_t GetPlayerOutlineColor(int id)
{
	static constexpr uint8_t PlayerOutlineColors[] = {
		OutlineColorsPlayer1,
		OutlineColorsPlayer2,
		OutlineColorsPlayer3,
		OutlineColorsPlayer4,
	};

	if (id < 0 || id >= static_cast<int>(SDL_arraysize(PlayerOutlineColors)))
		return OutlineColorsPlayer1;

	return PlayerOutlineColors[id];
}

/**
 * @brief Render a player sprite
 * @param out Output buffer
 * @param player Player reference
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawPlayer(const Player &player, Point tilePosition, Point targetBufferPosition, int lightTableIndex)
{
	if (!IsTileLit(tilePosition) && !MyPlayer->_pInfraFlag && !MyPlayer->isOnArenaLevel() && leveltype != DTYPE_TOWN) {
		return;
	}

	const ClxSprite sprite = player.currentSprite();
	const Point spriteBufferPosition = targetBufferPosition + player.getRenderingOffset(sprite);

	if (&player == PlayerUnderCursor)
		GetRenderer().DrawClxOutline(GetPlayerOutlineColor(player.getId()), spriteBufferPosition, sprite, true);

	if (&player == MyPlayer && IsNoneOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
		GetRenderer().DrawClx(spriteBufferPosition, sprite);
		DrawPlayerIcons(player, targetBufferPosition, /*infraVision=*/false, lightTableIndex);
		return;
	}

	if (!IsTileLit(tilePosition) || ((MyPlayer->_pInfraFlag || MyPlayer->isOnArenaLevel()) && lightTableIndex > 8)) {
		GetRenderer().DrawClxTRN(spriteBufferPosition, sprite, GetInfravisionTRN());
		DrawPlayerIcons(player, targetBufferPosition, /*infraVision=*/true, lightTableIndex);
		return;
	}

	lightTableIndex = std::max(lightTableIndex - 5, 0);
	GetRenderer().DrawClxLit(spriteBufferPosition, sprite, lightTableIndex);
	DrawPlayerIcons(player, targetBufferPosition, /*infraVision=*/false, lightTableIndex);
}

/**
 * @brief Render a player sprite
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawDeadPlayer(Point tilePosition, Point targetBufferPosition, int lightTableIndex)
{
	dFlags[tilePosition.x][tilePosition.y] &= ~DungeonFlag::DeadPlayer;

	for (const Player &player : Players) {
		if (player.plractive && player.hasNoLife() && player.isOnActiveLevel() && player.position.tile == tilePosition) {
			dFlags[tilePosition.x][tilePosition.y] |= DungeonFlag::DeadPlayer;
			const Point playerRenderPosition { targetBufferPosition };
			DrawPlayer(player, tilePosition, playerRenderPosition, lightTableIndex);
		}
	}
}

/**
 * @brief Render an object sprite
 * @param out Output buffer
 * @param objectToDraw Dungeone object to draw
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param pre Is the sprite in the background
 */
void DrawObject(const Object &objectToDraw, Point tilePosition, Point targetBufferPosition, int lightTableIndex)
{
	const ClxSprite sprite = objectToDraw.currentSprite();

	const Point screenPosition = targetBufferPosition + objectToDraw.getRenderingOffset(sprite, tilePosition);

	if (&objectToDraw == ObjectUnderCursor) {
		GetRenderer().DrawClxOutline(OutlineColorsObject, screenPosition, sprite, true);
	}
	if (objectToDraw.applyLighting) {
		GetRenderer().DrawClxLit(screenPosition, sprite, lightTableIndex);
	} else {
		GetRenderer().DrawClx(screenPosition, sprite);
	}
}

static void DrawDungeon(Point /*tilePosition*/, Point /*targetBufferPosition*/);

/**
 * @brief Render a cell
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinates
 */
void DrawCell(Point tilePosition, Point targetBufferPosition, int lightTableIndex)
{
	const uint16_t levelPieceId = dPiece[tilePosition.x][tilePosition.y];

	bool transparency = TileHasAny(tilePosition, TileProperties::Transparent) && TransList[dTransVal[tilePosition.x][tilePosition.y]];
#ifdef _DEBUG
	int walkpathIdx = -1;
	Point originalTargetBufferPosition;
	if (DebugPath) {
		walkpathIdx = MyPlayer->GetPositionPathIndex(tilePosition);
		if (walkpathIdx != -1) {
			originalTargetBufferPosition = targetBufferPosition;
		}
	}
	if ((SDL_GetModState() & SDL_KMOD_ALT) != 0) {
		transparency = false;
	}
#endif

	const bool isFloor = IsFloor(tilePosition);
	const TileProperties tileProperties = SOLData[levelPieceId];
	GetRenderer().DrawLevelTile(targetBufferPosition,
	    levelPieceId, lightTableIndex, transparency, tileProperties, isFloor);

#ifdef _DEBUG
	if (DebugPath && walkpathIdx != -1) {
		DrawString(StrCat(walkpathIdx),
		    Rectangle(originalTargetBufferPosition + Displacement { 0, -TILE_HEIGHT }, Size { TILE_WIDTH, TILE_HEIGHT }),
		    TextRenderOptions {
		        .flags = UiFlags::AlignCenter | UiFlags::VerticalCenter
		            | (IsTileSolid(tilePosition) ? UiFlags::ColorYellow : UiFlags::ColorWhite) });
	}
#endif
}

/**
 * @brief Render a floor tile.
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinate
 */
void DrawFloorTile(Point tilePosition, Point targetBufferPosition)
{
	const int lightTableIndex = dLight[tilePosition.x][tilePosition.y];
	const uint16_t levelPieceId = dPiece[tilePosition.x][tilePosition.y];
	GetRenderer().DrawFloorLevelTile(targetBufferPosition, levelPieceId, lightTableIndex);
}

/**
 * @brief Draw item for a given tile
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 * @param pre Is the sprite in the background
 */
void DrawItem(int8_t itemIndex, Point targetBufferPosition, int lightTableIndex)
{
	const Item &item = Items[itemIndex];
	const ClxSprite sprite = item.AnimInfo.currentSprite();
	const Point position = targetBufferPosition + item.getRenderingOffset(sprite);
	if (!IsPlayerInStore() && (itemIndex == pcursitem || AutoMapShowItems)) {
		GetRenderer().DrawClxOutline(GetOutlineColor(item, false), position, sprite, true);
	}
	GetRenderer().DrawClxLit(position, sprite, lightTableIndex);
	if (item.AnimInfo.isLastFrame() || item._iCurs == ICURS_MAGIC_ROCK)
		AddItemToLabelQueue(itemIndex, position);
}

/**
 * @brief Check if and how a monster should be rendered
 * @param out Output buffer
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Output buffer coordinates
 */
void DrawMonsterHelper(Point tilePosition, Point targetBufferPosition, int lightTableIndex)
{
	int mi = dMonster[tilePosition.x][tilePosition.y];

	mi = std::abs(mi) - 1;

	if (leveltype == DTYPE_TOWN) {
		auto &towner = Towners[mi];
		const Point position = targetBufferPosition + towner.getRenderingOffset();
		const ClxSprite sprite = towner.currentSprite();
		if (mi == pcursmonst) {
			GetRenderer().DrawClxOutline(OutlineColorsTowner, position, sprite, true);
		}
		GetRenderer().DrawClx(position, sprite);
		return;
	}

	if (!IsTileLit(tilePosition) && !(MyPlayer->_pInfraFlag && IsFloor(tilePosition))) {
		return;
	}

	if (static_cast<size_t>(mi) >= MaxMonsters) {
		Log("Draw Monster: tried to draw illegal monster {}", mi);
		return;
	}

	const auto &monster = Monsters[mi];
	if ((monster.flags & MFLAG_HIDDEN) != 0) {
		return;
	}

	const ClxSprite sprite = monster.animInfo.currentSprite();
	const Displacement offset = monster.getRenderingOffset(sprite);

	const Point monsterRenderPosition = targetBufferPosition + offset;
	if (mi == pcursmonst) {
		GetRenderer().DrawClxOutline(OutlineColorsMonster, monsterRenderPosition, sprite, true);
	}
	DrawMonster(tilePosition, monsterRenderPosition, monster, lightTableIndex);
}

/**
 * @brief Render object sprites
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinates
 */
void DrawDungeon(Point tilePosition, Point targetBufferPosition)
{
	assert(InDungeonBounds(tilePosition));
	const int lightTableIndex = dLight[tilePosition.x][tilePosition.y];

	DrawCell(tilePosition, targetBufferPosition, lightTableIndex);

	const int8_t bDead = dCorpse[tilePosition.x][tilePosition.y];
	const int8_t bMap = dTransVal[tilePosition.x][tilePosition.y];

#ifdef _DEBUG
	if (DebugVision && IsTileLit(tilePosition)) {
		GetRenderer().DrawClx(targetBufferPosition, (*pSquareCel)[0]);
	}
#endif

	if (MissilePreFlag) {
		DrawMissile(tilePosition, targetBufferPosition, true, lightTableIndex);
	}

	if (lightTableIndex < LightsMax && bDead != 0) {
		const Corpse &corpse = Corpses[(bDead & 0x1F) - 1];
		const Point position { targetBufferPosition.x - CalculateSpriteTileCenterX(corpse.width), targetBufferPosition.y };
		const ClxSprite sprite = corpse.spritesForDirection(static_cast<Direction>((bDead >> 5) & 7))[corpse.frame];
		if (corpse.translationPaletteIndex != 0) {
			const uint8_t *trn = Monsters[corpse.translationPaletteIndex - 1].uniqueMonsterTRN.get();
			GetRenderer().DrawClxTRN(position, sprite, trn);
		} else {
			GetRenderer().DrawClxLit(position, sprite, lightTableIndex);
		}
	}

	const int8_t bItem = dItem[tilePosition.x][tilePosition.y];
	const Object *object = lightTableIndex < LightsMax
	    ? FindObjectAtPosition(tilePosition)
	    : nullptr;
	if (object != nullptr && object->_oPreFlag) {
		DrawObject(*object, tilePosition, targetBufferPosition, lightTableIndex);
	}
	if (bItem > 0 && !Items[bItem - 1]._iPostDraw) {
		DrawItem(static_cast<int8_t>(bItem - 1), targetBufferPosition, lightTableIndex);
	}

	if (TileContainsDeadPlayer(tilePosition)) {
		DrawDeadPlayer(tilePosition, targetBufferPosition, lightTableIndex);
	}
	Player *player = PlayerAtPosition(tilePosition);
	if (player != nullptr) {
		const uint8_t pid = player->getId();
		assert(pid < MAX_PLRS);
		int playerId = static_cast<int>(pid) + 1;
		// If sprite is moving southwards or east, we want to draw it offset from the tile it's moving to, so we need negative ID
		// This respests the order that tiles are drawn. By using the negative id, we ensure that the sprite is drawn with priority
		if (player->_pmode == PM_WALK_SOUTHWARDS || (player->_pmode == PM_WALK_SIDEWAYS && player->_pdir == Direction::East))
			playerId = -playerId;
		if (dPlayer[tilePosition.x][tilePosition.y] == playerId) {
			auto tempTilePosition = tilePosition;
			auto tempTargetBufferPosition = targetBufferPosition;

			// Offset the sprite to the tile it's moving from
			if (player->_pmode == PM_WALK_SOUTHWARDS) {
				switch (player->_pdir) {
				case Direction::SouthWest:
					tempTargetBufferPosition += { TILE_WIDTH / 2, -TILE_HEIGHT / 2 };
					break;
				case Direction::South:
					tempTargetBufferPosition += { 0, -TILE_HEIGHT };
					break;
				case Direction::SouthEast:
					tempTargetBufferPosition += { -TILE_WIDTH / 2, -TILE_HEIGHT / 2 };
					break;
				default:
					DVL_UNREACHABLE();
				}
				tempTilePosition += Opposite(player->_pdir);
			} else if (player->_pmode == PM_WALK_SIDEWAYS && player->_pdir == Direction::East) {
				tempTargetBufferPosition += { -TILE_WIDTH, 0 };
				tempTilePosition += Opposite(player->_pdir);
			}
			DrawPlayer(*player, tempTilePosition, tempTargetBufferPosition, lightTableIndex);
		}
	}

	Monster *monster = FindMonsterAtPosition(tilePosition);
	if (monster != nullptr) {
		auto mid = monster->getId();
		assert(mid < MaxMonsters);
		int monsterId = static_cast<int>(mid) + 1;
		// If sprite is moving southwards or east, we want to draw it offset from the tile it's moving to, so we need negative ID
		// This respests the order that tiles are drawn. By using the negative id, we ensure that the sprite is drawn with priority
		if (monster->mode == MonsterMode::MoveSouthwards || (monster->mode == MonsterMode::MoveSideways && monster->direction == Direction::East))
			monsterId = -monsterId;
		if (dMonster[tilePosition.x][tilePosition.y] == monsterId) {
			auto tempTilePosition = tilePosition;
			auto tempTargetBufferPosition = targetBufferPosition;

			// Offset the sprite to the tile it's moving from
			if (monster->mode == MonsterMode::MoveSouthwards) {
				switch (monster->direction) {
				case Direction::SouthWest:
					tempTargetBufferPosition += { TILE_WIDTH / 2, -TILE_HEIGHT / 2 };
					break;
				case Direction::South:
					tempTargetBufferPosition += { 0, -TILE_HEIGHT };
					break;
				case Direction::SouthEast:
					tempTargetBufferPosition += { -TILE_WIDTH / 2, -TILE_HEIGHT / 2 };
					break;
				default:
					DVL_UNREACHABLE();
				}
				tempTilePosition += Opposite(monster->direction);
			} else if (monster->mode == MonsterMode::MoveSideways && monster->direction == Direction::East) {
				tempTargetBufferPosition += { -TILE_WIDTH, 0 };
				tempTilePosition += Opposite(monster->direction);
			}
			DrawMonsterHelper(tempTilePosition, tempTargetBufferPosition, lightTableIndex);
		}
	}

	DrawMissile(tilePosition, targetBufferPosition, false, lightTableIndex);

	if (object != nullptr && !object->_oPreFlag) {
		DrawObject(*object, tilePosition, targetBufferPosition, lightTableIndex);
	}
	if (bItem > 0 && Items[bItem - 1]._iPostDraw) {
		DrawItem(static_cast<int8_t>(bItem - 1), targetBufferPosition, lightTableIndex);
	}

	if (leveltype != DTYPE_TOWN) {
		const int8_t bArch = dSpecial[tilePosition.x][tilePosition.y] - 1;
		if (bArch >= 0) {
			bool transparency = TransList[bMap];
#ifdef _DEBUG
			// Turn transparency off here for debugging
			transparency = transparency && (SDL_GetModState() & SDL_KMOD_ALT) == 0;
#endif
			if (transparency)
				GetRenderer().DrawClxLitBlended(targetBufferPosition, (*pSpecialCels)[bArch], lightTableIndex);
			else
				GetRenderer().DrawClxLit(targetBufferPosition, (*pSpecialCels)[bArch], lightTableIndex);
		}
	} else {
		// Tree leaves should always cover player when entering or leaving the tile,
		// So delay the rendering until after the next row is being drawn.
		// This could probably have been better solved by sprites in screen space.
		if (tilePosition.x > 0 && tilePosition.y > 0 && targetBufferPosition.y > TILE_HEIGHT) {
			const int8_t bArch = dSpecial[tilePosition.x - 1][tilePosition.y - 1] - 1;
			if (bArch >= 0)
				GetRenderer().DrawClx(targetBufferPosition + Displacement { 0, -TILE_HEIGHT }, (*pSpecialCels)[bArch]);
		}
	}
}

/**
 * @brief Render a row of tiles
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinates
 * @param rows Number of rows
 * @param columns Tile in a row
 */
void DrawFloor(Point tilePosition, Point targetBufferPosition, int rows, int columns)
{
	for (int i = 0; i < rows; i++) {
		for (int j = 0; j < columns; j++, tilePosition += Direction::East, targetBufferPosition.x += TILE_WIDTH) {
			if (!InDungeonBounds(tilePosition))
				continue;
			if (IsFloor(tilePosition)) {
				DrawFloorTile(tilePosition, targetBufferPosition);
			}
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
 * @brief Renders the tile content
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Buffer coordinates
 * @param rows Number of rows
 * @param columns Tile in a row
 */
void DrawTileContent(Point tilePosition, Point targetBufferPosition, int rows, int columns)
{
	// Keep evaluating until MicroTiles can't affect screen
	rows += MicroTileLen;

#ifdef _DEBUG
	DebugCoordsMap.reserve(rows * columns);
#endif

	for (int i = 0; i < rows; i++) {
		bool skip = false;
		for (int j = 0; j < columns; j++) {
			if (InDungeonBounds(tilePosition)) {
				bool skipNext = false;
#ifdef _DEBUG
				DebugCoordsMap[tilePosition.x + (tilePosition.y * MAXDUNX)] = targetBufferPosition;
#endif
				if (tilePosition.x + 1 < MAXDUNX && tilePosition.y - 1 >= 0 && targetBufferPosition.x + TILE_WIDTH <= gnScreenWidth) {
					// Render objects behind walls first to prevent sprites, that are moving
					// between tiles, from poking through the walls as they exceed the tile bounds.
					// A proper fix for this would probably be to layout the scene and render by
					// sprite screen position rather than tile position.
					if (IsWall(tilePosition) && (IsWall(tilePosition + Displacement { 1, 0 }) || (tilePosition.x > 0 && IsWall(tilePosition + Displacement { -1, 0 })))) { // Part of a wall aligned on the x-axis
						if (IsTileNotSolid(tilePosition + Displacement { 1, -1 }) && IsTileNotSolid(tilePosition + Displacement { 0, -1 })) {                              // Has walkable area behind it
							DrawDungeon(tilePosition + Direction::East, { targetBufferPosition.x + TILE_WIDTH, targetBufferPosition.y });
							skipNext = true;
						}
					}
				}
				if (!skip) {
					DrawDungeon(tilePosition, targetBufferPosition);
				}
				skip = skipNext;
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

void DrawDirtTile(Point tilePosition, Point targetBufferPosition)
{
	// This should be the *top-left* of the 2×2 dirt pattern in the actual dungeon.
	// You might need to tweak these to where your dirt patch actually lives.
	constexpr Point base { 0, 0 };

	// Decide which of the 4 tiles of the 2×2 block to use,
	// based on where this OOB tile is in the world grid.
	const int ox = (tilePosition.x & 1); // 0 or 1
	const int oy = (tilePosition.y & 1); // 0 or 1

	Point sample {
		base.x + ox,
		base.y + oy,
	};

	// Safety: clamp in case tilePosition is wildly outside and base+offset ever escapes
	sample.x = std::clamp(sample.x, 0, MAXDUNX - 1);
	sample.y = std::clamp(sample.y, 0, MAXDUNY - 1);

	if (!InDungeonBounds(sample) || dPiece[sample.x][sample.y] == 0) {
		// Failsafe: if our sample somehow isn't valid, fall back to black
		GetRenderer().DrawBlackTile(targetBufferPosition.x, targetBufferPosition.y);
		return;
	}

	const int lightTableIndex = dLight[sample.x][sample.y];

	// Let the normal dungeon tile renderer compose the full tile
	DrawCell(sample, targetBufferPosition, lightTableIndex);
}

/**
 * @brief Render out-of-bounds tiles around the viewport edges.
 * @param tilePosition dPiece coordinates
 * @param targetBufferPosition Target buffer coordinates
 * @param rows Number of rows
 * @param columns Tile in a row
 */
void DrawOOB(Point tilePosition, Point targetBufferPosition, int rows, int columns)
{
	for (int i = 0; i < rows + 5; i++) { // 5 extra rows needed to make sure everything gets rendered at the bottom half of the screen
		for (int j = 0; j < columns; j++, tilePosition += Direction::East, targetBufferPosition.x += TILE_WIDTH) {
			if (!InDungeonBounds(tilePosition)) {
				if (leveltype == DTYPE_TOWN) {
					GetRenderer().DrawBlackTile(targetBufferPosition.x, targetBufferPosition.y);
				} else {
					DrawDirtTile(tilePosition, targetBufferPosition);
				}
			}
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

Displacement tileOffset;
Displacement tileShift;
int tileColumns;
int tileRows;

void CalcFirstTilePosition(Point &position, Displacement &offset)
{
	// Adjust by player offset and tile grid alignment
	const Player &myPlayer = *MyPlayer;
	offset = tileOffset;
	if (myPlayer.isWalking())
		offset += GetOffsetForWalking(myPlayer.AnimInfo, myPlayer._pdir, true);

	position += tileShift;

	// Skip rendering parts covered by the panels
	if (CanPanelsCoverView() && (IsLeftPanelOpen() || IsRightPanelOpen())) {
		const int multiplier = (*GetOptions().Graphics.zoom) ? 1 : 2;
		position += Displacement(Direction::East) * multiplier;
		offset.deltaX += -TILE_WIDTH * multiplier / 2 / 2;

		if (IsLeftPanelOpen() && !*GetOptions().Graphics.zoom) {
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
void DrawGame(Point position, Displacement offset)
{

	int columns = tileColumns;
	int rows = tileRows;

	// Skip rendering parts covered by the panels
	if (CanPanelsCoverView() && (IsLeftPanelOpen() || IsRightPanelOpen())) {
		columns -= (*GetOptions().Graphics.zoom) ? 2 : 4;
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

	GetRenderer().BeginScene(position, Point {} + offset, rows, columns);
	GetRenderer().BeginViewportZoom();

	DrawFloor(position, Point {} + offset, rows, columns);
	DrawTileContent(position, Point {} + offset, rows, columns);
	DrawOOB(position, Point {} + offset, rows, columns);

	GetRenderer().EndViewportZoom(gnViewportHeight);
	GetRenderer().EndScene();

#ifdef DUN_RENDER_STATS
	std::vector<std::pair<DunRenderType, size_t>> sortedStats(DunRenderStats.begin(), DunRenderStats.end());
	c_sort(sortedStats, [](const std::pair<DunRenderType, size_t> &a, const std::pair<DunRenderType, size_t> &b) {
		return a.first.maskType == b.first.maskType
		    ? static_cast<uint8_t>(a.first.tileType) < static_cast<uint8_t>(b.first.tileType)
		    : static_cast<uint8_t>(a.first.maskType) < static_cast<uint8_t>(b.first.maskType);
	});
	Point pos { 100, 20 };
	for (size_t i = 0; i < sortedStats.size(); ++i) {
		const auto &stat = sortedStats[i];
		DrawString(StrCat(i, "."), Rectangle(pos, Size { 20, 16 }), { .flags = UiFlags::AlignRight });
		DrawString(MaskTypeToString(stat.first.maskType), Point { pos.x + 24, pos.y }, gnScreenWidth);
		DrawString(TileTypeToString(stat.first.tileType), Point { pos.x + 184, pos.y }, gnScreenWidth);
		DrawString(FormatInteger(stat.second), Rectangle({ pos.x + 354, pos.y }, Size(40, 16)), { .flags = UiFlags::AlignRight });
		pos.y += 16;
	}
#endif
}

/**
 * @brief Start rendering of screen, town variation
 * @param out Buffer to render to
 * @param startPosition Center of view in dPiece coordinates
 */
void DrawView(Point startPosition)
{
#ifdef _DEBUG
	DebugCoordsMap.clear();
#endif
	Displacement offset = {};
	CalcFirstTilePosition(startPosition, offset);
	DrawGame(startPosition, offset);
	if (AutomapActive) {
		DrawAutomap();
	}
#ifdef _DEBUG
	bool debugGridTextNeeded = IsDebugGridTextNeeded();
	if (debugGridTextNeeded || DebugGrid) {
		// force redrawing or debug stuff stays on panel on 640x480 resolution
		RedrawEverything();
		std::string debugGridText;
		bool megaTiles = IsDebugGridInMegatiles();

		for (auto [dunCoordVal, pixelCoords] : DebugCoordsMap) {
			Point dunCoords = { dunCoordVal % MAXDUNX, dunCoordVal / MAXDUNX };
			if (megaTiles && (dunCoords.x % 2 == 1 || dunCoords.y % 2 == 1))
				continue;
			if (megaTiles)
				pixelCoords += Displacement { 0, TILE_HEIGHT / 2 };
			if (*GetOptions().Graphics.zoom)
				pixelCoords *= 2;
			if (debugGridTextNeeded && GetDebugGridText(dunCoords, debugGridText)) {
				Size tileSize = { TILE_WIDTH, TILE_HEIGHT };
				if (*GetOptions().Graphics.zoom)
					tileSize *= 2;
				DrawString(debugGridText, { pixelCoords - Displacement { 0, tileSize.height }, tileSize },
				    { .flags = UiFlags::ColorRed | UiFlags::AlignCenter | UiFlags::VerticalCenter });
			}
			if (DebugGrid) {
				int halfTileWidth = TILE_WIDTH / 2;
				int halfTileHeight = TILE_HEIGHT / 2;
				if (*GetOptions().Graphics.zoom) {
					halfTileWidth *= 2;
					halfTileHeight *= 2;
				}
				const Point center { pixelCoords.x + halfTileWidth, pixelCoords.y - halfTileHeight };

				if (megaTiles) {
					halfTileWidth *= 2;
					halfTileHeight *= 2;
				}

				const uint8_t col = PAL16_BEIGE;
				for (const auto &[originX, dx] : { std::pair(center.x - halfTileWidth, 1), std::pair(center.x + halfTileWidth, -1) }) {
					// We only need to draw half of the grid cell boundaries (one triangle).
					// The other triangle will be drawn when drawing the adjacent grid cells.
					const Point lineEnd1 { originX + dx * halfTileHeight, center.y + halfTileHeight };
					const Point lineEnd2 { originX + dx + dx * halfTileHeight, center.y + halfTileHeight };
					GetRenderer().DrawLine({ originX, center.y }, lineEnd1, col);
					GetRenderer().DrawLine({ originX + dx, center.y }, lineEnd2, col);
				}
			}
		}
	}
#endif
	DrawItemNameLabels();
	DrawMonsterHealthBar();
	DrawFloatingNumbers(startPosition, offset);

	if (IsPlayerInStore() && !qtextflag)
		DrawSText();
	if (invflag) {
		DrawInv();
	} else if (SpellbookFlag) {
		DrawSpellBook();
	}

	DrawDurIcon();

	DrawLevelButton();

	if (CharFlag) {
		DrawChr();
	} else if (QuestLogIsOpen) {
		DrawQuestLog();
	} else if (IsStashOpen) {
		DrawStash();
	} else if (IsVisualStoreOpen) {
		DrawVisualStore();
	}

	if (ShowUniqueItemInfoBox) {
		DrawUniqueInfo();
	}
	if (qtextflag) {
		DrawQText();
	}
	if (SpellSelectFlag) {
		DrawSpellList();
	}
	if (DropGoldFlag) {
		DrawGoldSplit();
	}
	DrawGoldWithdraw();
	if (HelpFlag) {
		DrawHelp();
	}
	if (ChatLogFlag) {
		DrawChatLog();
	}
	if (MyPlayerIsDead) {
		RedBack();
		DrawDeathText();
	} else if (PauseMode != 0) {
		gmenu_draw_pause();
	}
	if (IsDiabloMsgAvailable()) {
		DrawDiabloMsg();
	}

	DrawControllerModifierHints();
	DrawPlrMsg();
	gmenu_draw();
	doom_draw();
	DrawInfoBox();
	UpdateLifeManaPercent(); // Update life/mana totals before rendering any portion of the flask.
	DrawLifeFlaskUpper();
	DrawManaFlaskUpper();
}

/**
 * @brief Display the current average FPS over 1 sec
 */
void DrawFPS()
{
	static int framesSinceLastUpdate = 0;
	static std::string_view formatted {};

	if (!frameflag || !gbActive) {
		return;
	}

	framesSinceLastUpdate++;
	const uint32_t runtimeInMs = SDL_GetTicks();
	const uint32_t msSinceLastUpdate = runtimeInMs - lastFpsUpdateInMs;
	if (msSinceLastUpdate >= 1000) {
		lastFpsUpdateInMs = runtimeInMs;
		constexpr int FpsPow10 = 10;
		const uint32_t fps = 1000 * FpsPow10 * framesSinceLastUpdate / msSinceLastUpdate;
		framesSinceLastUpdate = 0;

		static char buf[15] {};
		const char *end = fps >= 100 * FpsPow10
		    ? BufCopy(buf, fps / FpsPow10, " FPS")
		    : BufCopy(buf, fps / FpsPow10, ".", fps % FpsPow10, " FPS");
		formatted = { buf, static_cast<std::string_view::size_type>(end - buf) };
	};
	DrawString(formatted, Point { 8, 8 }, gnScreenWidth, { .flags = UiFlags::ColorRed });
}

/**
 * @brief Mark the screen regions that were updated this frame for partial blit optimization.
 */
void MarkDirtyRegions(const Rectangle &mainPanel, bool drawHealth, bool drawMana, bool drawBelt, bool drawControlButtons, bool drawChatInput)
{
	auto &renderer = GetRenderer();
	if (gnScreenWidth > mainPanel.size.width || IsRedrawEverything()) {
		renderer.MarkScreenDirty();
		return;
	}
	const Point mainPanelPosition = mainPanel.position;
	if (IsRedrawViewport()) {
		renderer.MarkDirtyRect({ { 0, 0 }, { gnScreenWidth, gnViewportHeight } });
		if (drawChatInput) {
			// When chat input is displayed, the belt is hidden and the chat moves up.
			renderer.MarkDirtyRect({ mainPanelPosition + Displacement { 171, 6 }, { 298, 116 } });
		} else {
			renderer.MarkDirtyRect({ mainPanelPosition + Displacement { InfoBoxRect.position.x, InfoBoxRect.position.y }, { InfoBoxRect.size } });
		}
	}
	if (drawBelt) {
		renderer.MarkDirtyRect({ mainPanelPosition + Displacement { 204, 5 }, { 232, 28 } });
	}
	if (drawMana) {
		renderer.MarkDirtyRect({ mainPanelPosition + Displacement { 460, 0 }, { 88, 72 } });
		renderer.MarkDirtyRect({ mainPanelPosition + Displacement { 564, 64 }, { 56, 56 } });
	}
	if (drawHealth) {
		renderer.MarkDirtyRect({ mainPanelPosition + Displacement { 96, 0 }, { 88, 72 } });
	}
	if (drawControlButtons) {
		renderer.MarkDirtyRect({ mainPanelPosition + Displacement { 8, 7 }, { 74, 114 } });
		renderer.MarkDirtyRect({ mainPanelPosition + Displacement { 559, 7 }, { 74, 48 } });
		if (gbIsMultiplayer) {
			renderer.MarkDirtyRect({ mainPanelPosition + Displacement { 86, 91 }, { 34, 32 } });
			renderer.MarkDirtyRect({ mainPanelPosition + Displacement { 526, 91 }, { 34, 32 } });
		}
	}
}

void OptionShowFPSChanged()
{
	if (*GetOptions().Graphics.showFPS)
		EnableFrameCount();
	else
		frameflag = false;
}
const auto OptionChangeHandlerShowFPS = (GetOptions().Graphics.showFPS.SetValueChangedCallback(OptionShowFPSChanged), true);

} // namespace

Displacement GetOffsetForWalking(const AnimationInfo &animationInfo, const Direction dir, bool cameraMode /*= false*/)
{
	// clang-format off
	//                                           South,        SouthWest,    West,         NorthWest,    North,        NorthEast,     East,         SouthEast,
	constexpr Displacement MovingOffset[8]   = { {   0,  32 }, { -32,  16 }, { -64,   0 }, { -32, -16 }, {   0, -32 }, {  32, -16 },  {  64,   0 }, {  32,  16 } };
	// clang-format on

	const uint8_t animationProgress = animationInfo.getAnimationProgress();
	Displacement offset = MovingOffset[static_cast<size_t>(dir)];
	offset *= animationProgress;
	offset /= AnimationInfo::baseValueFraction;

	if (cameraMode) {
		offset = -offset;
	}

	return offset;
}

void ShiftGrid(Point *offset, int horizontal, int vertical)
{
	offset->x += vertical + horizontal;
	offset->y += vertical - horizontal;
}

int RowsCoveredByPanel()
{
	const auto &mainPanelSize = GetMainPanel().size;
	if (GetScreenWidth() <= mainPanelSize.width) {
		return 0;
	}

	int rows = mainPanelSize.height / TILE_HEIGHT;
	if (*GetOptions().Graphics.zoom) {
		rows /= 2;
	}

	return rows;
}

void CalcTileOffset(int *offsetX, int *offsetY)
{
	const uint16_t screenWidth = GetScreenWidth();
	const uint16_t viewportHeight = GetViewportHeight();

	int x;
	int y;

	if (!*GetOptions().Graphics.zoom) {
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
	const uint16_t screenWidth = GetScreenWidth();
	const uint16_t viewportHeight = GetViewportHeight();

	int columns = screenWidth / TILE_WIDTH;
	if ((screenWidth % TILE_WIDTH) != 0) {
		columns++;
	}
	int rows = viewportHeight / TILE_HEIGHT;
	if ((viewportHeight % TILE_HEIGHT) != 0) {
		rows++;
	}

	if (*GetOptions().Graphics.zoom) {
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
	const int zoomFactor = *GetOptions().Graphics.zoom ? 2 : 1;
	const int screenWidth = GetScreenWidth() / zoomFactor;
	const int screenHeight = GetScreenHeight() / zoomFactor;
	const int panelHeight = GetMainPanel().size.height / zoomFactor;
	const int pixelsToPanel = screenHeight - panelHeight;
	Point playerPosition { screenWidth / 2, pixelsToPanel / 2 };

	if (*GetOptions().Graphics.zoom)
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
	tileOffset = { startPosition.x - (TILE_WIDTH / 2), startPosition.y + (TILE_HEIGHT / 2) - 1 };

	// Compute the number of rows to be rendered as well as
	// the number of columns to be rendered in the first row
	const int viewportHeight = GetViewportHeight() / zoomFactor;
	const Point renderStart = startPosition - Displacement { TILE_WIDTH / 2, TILE_HEIGHT / 2 };
	tileRows = (viewportHeight - renderStart.y + TILE_HEIGHT / 2 - 1) / (TILE_HEIGHT / 2);
	tileColumns = (screenWidth - renderStart.x + TILE_WIDTH - 1) / TILE_WIDTH;
}

Point GetScreenPosition(Point tile)
{
	Point firstTile = ViewPosition;
	Displacement offset = {};
	CalcFirstTilePosition(firstTile, offset);

	const Displacement delta = firstTile - tile;

	Point position {};
	position += delta.worldToScreen();
	position += offset;
	return position;
}

void ClearScreenBuffer()
{
	if (HeadlessMode)
		return;

	GetRenderer().ClearScreen();
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

void RedrawGameScene()
{
	if (HeadlessMode)
		return;

	RedrawEverything();

	const Rectangle &mainPanel = GetMainPanel();

	DrawView(ViewPosition);
	DrawMainPanel();
	DrawLifeFlaskLower(false);
	DrawManaFlaskLower(false);
	DrawSpell();
	DrawMainPanelButtons();
	DrawInvBelt();
	if (ChatFlag) {
		DrawChatBox();
	}
	DrawXPBar();
	if (*GetOptions().Gameplay.showHealthValues)
		DrawFlaskValues({ mainPanel.position.x + 134, mainPanel.position.y + 28 }, MyPlayer->_pHitPoints >> 6, MyPlayer->_pMaxHP >> 6);
	if (*GetOptions().Gameplay.showManaValues)
		DrawFlaskValues({ mainPanel.position.x + mainPanel.size.width - 138, mainPanel.position.y + 28 },
		    (HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::NoMana) || MyPlayer->hasNoMana()) ? 0 : MyPlayer->_pMana >> 6,
		    HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::NoMana) ? 0 : MyPlayer->_pMaxMana >> 6);
	if (*GetOptions().Gameplay.floatingInfoBox)
		DrawFloatingInfoBox();

	if (*GetOptions().Gameplay.showMultiplayerPartyInfo && PartySidePanelOpen)
		DrawPartyMemberInfoPanel();

	UpdateCursorVisibility();

	DrawFPS();

	GetRenderer().MarkScreenDirty();

	RedrawComplete();
}

void scrollrt_draw_game_screen()
{
	if (HeadlessMode)
		return;

	const bool fullRedraw = IsRedrawEverything();
	if (fullRedraw) {
		RedrawComplete();
	}

	UpdateCursorVisibility();

	auto &renderer = GetRenderer();
	if (fullRedraw) {
		renderer.MarkScreenDirty();
	}

	renderer.EndFrame();
}

void DrawAndBlit()
{
	if (!gbRunGame || HeadlessMode) {
		return;
	}

	bool drawHealth = IsRedrawComponent(PanelDrawComponent::Health);
	bool drawMana = IsRedrawComponent(PanelDrawComponent::Mana);
	bool drawControlButtons = IsRedrawComponent(PanelDrawComponent::ControlButtons);
	bool drawBelt = IsRedrawComponent(PanelDrawComponent::Belt);
	const bool drawChatInput = ChatFlag;
	bool drawCtrlPan = false;

	const Rectangle &mainPanel = GetMainPanel();

	if (gnScreenWidth > mainPanel.size.width || IsRedrawEverything()) {
		drawHealth = true;
		drawMana = true;
		drawControlButtons = true;
		drawBelt = true;
		drawCtrlPan = true;
	} else if (IsRedrawViewport()) {
		drawCtrlPan = false;
	}

	nthread_UpdateProgressToNextGameTick();

	DrawView(ViewPosition);
	if (drawCtrlPan) {
		DrawMainPanel();
	}
	if (drawHealth) {
		DrawLifeFlaskLower(!drawCtrlPan);
	}
	if (drawMana) {
		DrawManaFlaskLower(!drawCtrlPan);

		DrawSpell();
	}
	if (drawControlButtons) {
		DrawMainPanelButtons();
	}
	if (drawBelt) {
		DrawInvBelt();
	}
	if (drawChatInput) {
		DrawChatBox();
	}
	DrawXPBar();
	if (*GetOptions().Gameplay.showHealthValues)
		DrawFlaskValues({ mainPanel.position.x + 134, mainPanel.position.y + 28 }, MyPlayer->_pHitPoints >> 6, MyPlayer->_pMaxHP >> 6);
	if (*GetOptions().Gameplay.showManaValues)
		DrawFlaskValues({ mainPanel.position.x + mainPanel.size.width - 138, mainPanel.position.y + 28 },
		    (HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::NoMana) || MyPlayer->hasNoMana()) ? 0 : MyPlayer->_pMana >> 6,
		    HasAnyOf(InspectPlayer->_pIFlags, ItemSpecialEffect::NoMana) ? 0 : MyPlayer->_pMaxMana >> 6);
	if (*GetOptions().Gameplay.floatingInfoBox)
		DrawFloatingInfoBox();

	if (*GetOptions().Gameplay.showMultiplayerPartyInfo && PartySidePanelOpen)
		DrawPartyMemberInfoPanel();

	UpdateCursorVisibility();

	DrawFPS();

	lua::GameDrawComplete();

#ifdef _DEBUG
	DrawConsole();
#endif

	MarkDirtyRegions(mainPanel, drawHealth, drawMana, drawBelt, drawControlButtons, drawChatInput);

	RedrawComplete();
	for (const PanelDrawComponent component : enum_values<PanelDrawComponent>()) {
		if (IsRedrawComponent(component)) {
			RedrawComponentComplete(component);
		}
	}

	GetRenderer().EndFrame();
}

} // namespace devilution
