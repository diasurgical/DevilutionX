/**
 * @file player.cpp
 *
 * Implementation of player functionality, leveling, actions, creation, loading, etc.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>

#ifdef USE_SDL3
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "control/control.hpp"
#include "controls/control_mode.hpp"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "dead.h"
#ifdef _DEBUG
#include "debug.h"
#endif
#include "engine/backbuffer_state.hpp"
#include "engine/load_cl2.hpp"
#include "engine/load_file.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/trn.hpp"
#include "engine/world_tile.hpp"
#include "game_mode.hpp"
#include "gamemenu.h"
#include "headless_mode.hpp"
#include "help.h"
#include "inv_iterators.hpp"
#include "levels/tile_properties.hpp"
#include "levels/trigs.h"
#include "lighting.h"
#include "loadsave.h"
#include "lua/lua_event.hpp"
#include "minitext.h"
#include "missiles.h"
#include "monster.h"
#include "nthread.h"
#include "objects.h"
#include "options.h"
#include "player.h"
#include "qol/autopickup.h"
#include "qol/stash.h"
#include "spells.h"
#include "stores.h"
#include "towners.h"
#include "utils/format.hpp"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

uint8_t MyPlayerId;
Player *MyPlayer;
std::vector<Player> Players;
Player *InspectPlayer;
bool MyPlayerIsDead;

namespace {

struct DirectionSettings {
	Direction dir;
	PLR_MODE walkMode;
};

void UpdatePlayerLightOffset(Player &player)
{
	if (player.lightId == NO_LIGHT)
		return;

	const WorldTileDisplacement offset = player.position.CalculateWalkingOffset(player._pdir, player.AnimInfo);
	ChangeLightOffset(player.lightId, offset.screenToLight());
}

void WalkInDirection(Player &player, const DirectionSettings &walkParams)
{
	player.occupyTile(player.position.future, true);
	player.position.temp = player.position.tile + walkParams.dir;
}

constexpr std::array<const DirectionSettings, 8> WalkSettings { {
	// clang-format off
	{ Direction::South,     PM_WALK_SOUTHWARDS },
	{ Direction::SouthWest, PM_WALK_SOUTHWARDS },
	{ Direction::West,      PM_WALK_SIDEWAYS   },
	{ Direction::NorthWest, PM_WALK_NORTHWARDS },
	{ Direction::North,     PM_WALK_NORTHWARDS },
	{ Direction::NorthEast, PM_WALK_NORTHWARDS },
	{ Direction::East,      PM_WALK_SIDEWAYS   },
	{ Direction::SouthEast, PM_WALK_SOUTHWARDS }
	// clang-format on
} };

bool PlrDirOK(const Player &player, Direction dir)
{
	const Point position = player.position.tile;
	const Point futurePosition = position + dir;
	if (futurePosition.x < 0 || !PosOkPlayer(player, futurePosition)) {
		return false;
	}

	if (dir == Direction::East) {
		return !IsTileSolid(position + Direction::SouthEast);
	}

	if (dir == Direction::West) {
		return !IsTileSolid(position + Direction::SouthWest);
	}

	return true;
}

void HandleWalkMode(Player &player, Direction dir)
{
	const auto &dirModeParams = WalkSettings[static_cast<size_t>(dir)];
	SetPlayerOld(player);
	if (!PlrDirOK(player, dir)) {
		return;
	}

	player._pdir = dir;

	// The player's tile position after finishing this movement action
	player.position.future = player.position.tile + dirModeParams.dir;

	WalkInDirection(player, dirModeParams);

	player.tempDirection = dirModeParams.dir;
	player._pmode = dirModeParams.walkMode;
}

void StartWalkAnimation(Player &player, Direction dir, bool pmWillBeCalled)
{
	int8_t skippedFrames = -2;
	if (leveltype == DTYPE_TOWN && sgGameInitInfo.bRunInTown != 0)
		skippedFrames = 2;
	if (pmWillBeCalled)
		skippedFrames += 1;
	// Warrior: walking speed scales with Strength.
	// 1 extra skipped frame per 50 points above base 30, capped at 2 (max +25% speed).
	if (player._pClass == HeroClass::Warrior)
		skippedFrames += std::clamp<int>((player._pStrength - 30) / 50, 0, 2);
	NewPlrAnim(player, player_graphic::Walk, dir, AnimationDistributionFlags::ProcessAnimationPending, skippedFrames);
}

/**
 * @brief Start moving a player to a new tile
 */
void StartWalk(Player &player, Direction dir, bool pmWillBeCalled)
{
	if (player._pInvincible && player.hasNoLife() && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	StartWalkAnimation(player, dir, pmWillBeCalled);
	HandleWalkMode(player, dir);
}

void ClearStateVariables(Player &player)
{
	player.position.temp = { 0, 0 };
	player.tempDirection = Direction::South;
	player.queuedSpell.spellLevel = 0;
}

void StartAttack(Player &player, Direction d, bool includesFirstFrame)
{
	if (player._pInvincible && player.hasNoLife() && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	// Warrior Zeal: after a Zeal swing (or swing pair) resolves, a lockout
	// runs before the next Zeal swing can start: two quick hits, then the
	// full normal wait. Regular swings without a target (shift-attack into
	// the air) are never blocked and never lock out.
	if (player._pClass == HeroClass::Warrior && player._pZealLock > 0
	    && (player.destAction == ACTION_ATTACKMON || player.destAction == ACTION_ATTACKPLR)) {
		return;
	}

	int8_t skippedAnimationFrames = 0;
	const auto flags = player._pIFlags;

	// If the first frame is not included in vanilla, the skip logic for the first frame will not be executed.
	// This will result in a different and slower attack speed.
	if (HasAnyOf(flags, ItemSpecialEffect::FastestAttack)) {
		// If the fastest attack logic is trigger frames in vanilla two frames are skipped, so missing the first frame reduces the skip logic by two frames.
		skippedAnimationFrames = includesFirstFrame ? 4 : 2;
	} else if (HasAnyOf(flags, ItemSpecialEffect::FasterAttack)) {
		skippedAnimationFrames = includesFirstFrame ? 3 : 2;
	} else if (HasAnyOf(flags, ItemSpecialEffect::FastAttack)) {
		skippedAnimationFrames = includesFirstFrame ? 2 : 1;
	} else if (HasAnyOf(flags, ItemSpecialEffect::QuickAttack)) {
		skippedAnimationFrames = includesFirstFrame ? 1 : 0;
	}

	// Warrior Zeal: melee attacks that target a monster (or player) are
	// Zeal swings: faster, chainable into a pair, and followed by the
	// lockout. Swings at an empty tile (shift-attack into the air) stay
	// completely vanilla: no speedup, no chain, no lockout.
	// The skipped-frame count must stay at or below _pAFNum - 2, because
	// setNewAnimation starts the animation at currentFrame = numSkippedFrames
	// and one frame is consumed in the same tick; starting on the action
	// frame would skip the hit window (currentFrame == _pAFNum - 1).
	if (player._pClass == HeroClass::Warrior
	    && (player.destAction == ACTION_ATTACKMON || player.destAction == ACTION_ATTACKPLR)) {
		// 1 = first swing of a pair, 2 = chained second swing (set by the
		// queue in DoAttack; preserved here so the second swing stays fast).
		if (player._pZealSwing != 2)
			player._pZealSwing = 1;
		skippedAnimationFrames += player._pAFrames / 2;
		skippedAnimationFrames = std::min<int8_t>(skippedAnimationFrames, std::max<int8_t>(0, static_cast<int8_t>(player._pAFNum - 2)));
	} else if (player._pClass == HeroClass::Warrior) {
		player._pZealSwing = 0; // regular swing without a target: no Zeal
	}

	auto animationFlags = AnimationDistributionFlags::ProcessAnimationPending;
	if (player._pmode == PM_ATTACK)
		animationFlags = static_cast<AnimationDistributionFlags>(animationFlags | AnimationDistributionFlags::RepeatedAction);
	NewPlrAnim(player, player_graphic::Attack, d, animationFlags, skippedAnimationFrames, player._pAFNum);
	player._pmode = PM_ATTACK;
	FixPlayerLocation(player, d);
	SetPlayerOld(player);
}

void StartRangeAttack(Player &player, Direction d, WorldTileCoord cx, WorldTileCoord cy, bool includesFirstFrame)
{
	if (player._pInvincible && player.hasNoLife() && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	int8_t skippedAnimationFrames = 0;
	const auto flags = player._pIFlags;

	if (!gbIsHellfire) {
		if (includesFirstFrame && HasAnyOf(flags, ItemSpecialEffect::QuickAttack | ItemSpecialEffect::FastAttack)) {
			skippedAnimationFrames += 1;
		}
		if (HasAnyOf(flags, ItemSpecialEffect::FastAttack)) {
			skippedAnimationFrames += 1;
		}
	}

	auto animationFlags = AnimationDistributionFlags::ProcessAnimationPending;
	if (player._pmode == PM_RATTACK)
		animationFlags = static_cast<AnimationDistributionFlags>(animationFlags | AnimationDistributionFlags::RepeatedAction);
	NewPlrAnim(player, player_graphic::Attack, d, animationFlags, skippedAnimationFrames, player._pAFNum);

	player._pmode = PM_RATTACK;
	FixPlayerLocation(player, d);
	SetPlayerOld(player);
	player.position.temp = WorldTilePosition { cx, cy };
}

player_graphic GetPlayerGraphicForSpell(SpellID spellId)
{
	switch (GetSpellData(spellId).type()) {
	case MagicType::Fire:
		return player_graphic::Fire;
	case MagicType::Lightning:
		return player_graphic::Lightning;
	default:
		return player_graphic::Magic;
	}
}

void StartSpell(Player &player, Direction d, WorldTileCoord cx, WorldTileCoord cy)
{
	if (player._pInvincible && player.hasNoLife() && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	// Checks conditions for spell again, because initial check was done when spell was queued and the parameters could be changed meanwhile
	bool isValid = false;
	switch (player.queuedSpell.spellType) {
	case SpellType::Skill:
	case SpellType::Spell:
		isValid = CheckSpell(player, player.queuedSpell.spellId, player.queuedSpell.spellType, true) == SpellCheckResult::Success;
		break;
	case SpellType::Scroll:
		isValid = CanUseScroll(player, player.queuedSpell.spellId);
		break;
	case SpellType::Charges:
		isValid = CanUseStaff(player, player.queuedSpell.spellId);
		break;
	default:
		break;
	}
	if (!isValid)
		return;

	auto animationFlags = AnimationDistributionFlags::ProcessAnimationPending;
	if (player._pmode == PM_SPELL)
		animationFlags = static_cast<AnimationDistributionFlags>(animationFlags | AnimationDistributionFlags::RepeatedAction);
	NewPlrAnim(player, GetPlayerGraphicForSpell(player.queuedSpell.spellId), d, animationFlags, 0, player._pSFNum);

	PlaySfxLoc(GetSpellData(player.queuedSpell.spellId).sSFX, player.position.tile);

	player._pmode = PM_SPELL;

	FixPlayerLocation(player, d);
	SetPlayerOld(player);

	player.position.temp = WorldTilePosition { cx, cy };
	player.queuedSpell.spellLevel = player.GetSpellLevel(player.queuedSpell.spellId);
	player.executedSpell = player.queuedSpell;
}

void RespawnDeadItem(Item &&itm, Point target)
{
	if (ActiveItemCount >= MAXITEMS)
		return;

	const int ii = AllocateItem();
	Item &item = Items[ii];

	dItem[target.x][target.y] = ii + 1;

	item = itm;
	item.position = target;
	RespawnItem(item, true);
	NetSendCmdPItem(false, CMD_SPAWNITEM, target, item);
}

void DeadItem(Player &player, Item &&item, Displacement direction)
{
	if (item.isEmpty())
		return;

	const Point playerTile = player.position.tile;
	if (direction != Displacement { 0, 0 }) {
		const Point target = playerTile + direction;
		if (ItemSpaceOk(target)) {
			RespawnDeadItem(std::move(item), target);
			return;
		}
	}

	std::optional<Point> dropPoint = FindClosestValidPosition(ItemSpaceOk, playerTile, 1, 50);
	if (dropPoint) {
		RespawnDeadItem(std::move(item), *dropPoint);
	}
}

int DropGold(Player &player, int amount, bool skipFullStacks)
{
	for (int i = 0; i < player._pNumInv && amount > 0; i++) {
		Item &item = player.InvList[i];

		if (item._itype != ItemType::Gold || (skipFullStacks && item._ivalue == MaxGold))
			continue;

		if (amount < item._ivalue) {
			Item goldItem;
			MakeGoldStack(goldItem, amount);
			DeadItem(player, std::move(goldItem), { 0, 0 });

			item._ivalue -= amount;

			return 0;
		}

		amount -= item._ivalue;
		DeadItem(player, std::move(item), { 0, 0 });
		player.RemoveInvItem(i);
		i = -1;
	}

	return amount;
}

void DropHalfPlayersGold(Player &player)
{
	const int remainingGold = DropGold(player, player._pGold / 2, true);
	if (remainingGold > 0) {
		DropGold(player, remainingGold, false);
	}

	player._pGold /= 2;
}

void InitLevelChange(Player &player)
{
	const Player &myPlayer = *MyPlayer;

	RemoveEnemyReferences(player);
	RemovePlrMissiles(player);
	player.pManaShield = false;
	player.wReflections = 0;
	if (&player != MyPlayer) {
		// share info about your manashield when another player joins the level
		if (myPlayer.pManaShield)
			NetSendCmd(true, CMD_SETSHIELD);
		// share info about your reflect charges when another player joins the level
		NetSendCmdParam1(true, CMD_SETREFLECT, myPlayer.wReflections);
	} else if (qtextflag) {
		qtextflag = false;
		stream_stop();
	}

	FixPlrWalkTags(player);
	SetPlayerOld(player);
	if (&player == MyPlayer) {
		player.occupyTile(player.position.tile, false);
	} else {
		player._pLvlVisited[player.plrlevel] = true;
	}

	ClrPlrPath(player);
	player.destAction = ACTION_NONE;
	player._pLvlChanging = true;

	if (&player == MyPlayer) {
		player.pLvlLoad = 10;
	}
}

/**
 * @brief Continue movement towards new tile
 */
bool DoWalk(Player &player)
{
	// Play walking sound effect on certain animation frames
	if (*GetOptions().Audio.walkingSound && (leveltype != DTYPE_TOWN || sgGameInitInfo.bRunInTown == 0)) {
		if (player.AnimInfo.currentFrame == 0
		    || player.AnimInfo.currentFrame == 4) {
			PlaySfxLoc(SfxID::Walk, player.position.tile);
		}
	}

	if (!player.AnimInfo.isLastFrame()) {
		// We didn't reach new tile so update player's "sub-tile" position
		UpdatePlayerLightOffset(player);
		return false;
	}

	// We reached the new tile -> update the player's tile position
	dPlayer[player.position.tile.x][player.position.tile.y] = 0;
	MarkWalkabilityChanged();
	player.position.tile = player.position.temp;
	// dPlayer is set here for backwards compatibility; without it, the player would be invisible if loaded from a vanilla save.
	player.occupyTile(player.position.tile, false);

	// Update the coordinates for lighting and vision entries for the player
	if (leveltype != DTYPE_TOWN) {
		ChangeLightXY(player.lightId, player.position.tile);
		ChangeVisionXY(player.getId(), player.position.tile);
	}

	StartStand(player, player.tempDirection);

	ClearStateVariables(player);

	// Reset the "sub-tile" position of the player's light entry to 0
	if (leveltype != DTYPE_TOWN) {
		ChangeLightOffset(player.lightId, { 0, 0 });
	}

	AutoPickup(player);
	return true;
}

bool WeaponDecay(Player &player, int ii)
{
	if (!player.InvBody[ii].isEmpty() && player.InvBody[ii]._iClass == ICLASS_WEAPON && HasAnyOf(player.InvBody[ii]._iDamAcFlags, ItemSpecialEffectHf::Decay)) {
		player.InvBody[ii]._iPLDam -= 5;
		if (player.InvBody[ii]._iPLDam <= -100) {
			RemoveEquipment(player, static_cast<inv_body_loc>(ii), true);
			CalcPlrInv(player, true);
			return true;
		}
		CalcPlrInv(player, true);
	}
	return false;
}

bool DamageWeapon(Player &player, unsigned damageFrequency)
{
	if (&player != MyPlayer) {
		return false;
	}

	if (WeaponDecay(player, INVLOC_HAND_LEFT))
		return true;
	if (WeaponDecay(player, INVLOC_HAND_RIGHT))
		return true;

	if (!FlipCoin(damageFrequency)) {
		return false;
	}

	if (!player.InvBody[INVLOC_HAND_LEFT].isEmpty() && player.InvBody[INVLOC_HAND_LEFT]._iClass == ICLASS_WEAPON) {
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		player.InvBody[INVLOC_HAND_LEFT]._iDurability--;
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability <= 0) {
			RemoveEquipment(player, INVLOC_HAND_LEFT, true);
			CalcPlrInv(player, true);
			return true;
		}
	}

	if (!player.InvBody[INVLOC_HAND_RIGHT].isEmpty() && player.InvBody[INVLOC_HAND_RIGHT]._iClass == ICLASS_WEAPON) {
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		player.InvBody[INVLOC_HAND_RIGHT]._iDurability--;
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == 0) {
			RemoveEquipment(player, INVLOC_HAND_RIGHT, true);
			CalcPlrInv(player, true);
			return true;
		}
	}

	if (player.InvBody[INVLOC_HAND_LEFT].isEmpty() && player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield) {
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		player.InvBody[INVLOC_HAND_RIGHT]._iDurability--;
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == 0) {
			RemoveEquipment(player, INVLOC_HAND_RIGHT, true);
			CalcPlrInv(player, true);
			return true;
		}
	}

	if (player.InvBody[INVLOC_HAND_RIGHT].isEmpty() && player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield) {
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == DUR_INDESTRUCTIBLE) {
			return false;
		}

		player.InvBody[INVLOC_HAND_LEFT]._iDurability--;
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == 0) {
			RemoveEquipment(player, INVLOC_HAND_LEFT, true);
			CalcPlrInv(player, true);
			return true;
		}
	}

	return false;
}

bool PlrHitMonst(Player &player, Monster &monster, bool adjacentDamage = false)
{
	int hper = 0;

	if (!monster.isPossibleToHit())
		return false;

	if (adjacentDamage) {
		if (player.getCharacterLevel() > 20)
			hper -= 30;
		else
			hper -= (35 - player.getCharacterLevel()) * 2;
	}

	int hit = GenerateRnd(100);
	if (monster.mode == MonsterMode::Petrified) {
		hit = 0;
	}

	hper += player.GetMeleePiercingToHit() - player.CalculateArmorPierce(monster.armorClass, true);
	hper = std::clamp(hper, 5, 95);

	if (monster.tryLiftGargoyle())
		return true;

	if (hit >= hper) {
#ifdef _DEBUG
		if (!DebugGodMode)
#endif
			return false;
	}

	if (gbIsHellfire && HasAllOf(player._pIFlags, ItemSpecialEffect::FireDamage | ItemSpecialEffect::LightningDamage)) {
		// Fixed off by 1 error from Hellfire
		const int midam = RandomIntBetween(player._pIFMinDam, player._pIFMaxDam);
		AddMissile(player.position.tile, player.position.temp, player._pdir, MissileID::SpectralArrow, TARGET_MONSTERS, player, midam, 0);
	}
	const int mind = player._pIMinDam;
	const int maxd = player._pIMaxDam;
	int dam = RandomIntBetween(mind, maxd);
	dam += dam * player._pIBonusDam / 100;
	dam += player._pIBonusDamMod;
	int dam2 = dam << 6;
	dam += player._pDamageMod;

	const ClassAttributes &classAttributes = GetClassAttributes(player._pClass);
	if (HasAnyOf(classAttributes.classFlags, PlayerClassFlag::CriticalStrike)) {
		if (GenerateRnd(100) < player.getCharacterLevel()) {
			dam *= 2;
		}
	}

	ItemType phanditype = ItemType::None;
	if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Sword || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Sword) {
		phanditype = ItemType::Sword;
	}
	if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Mace || player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Mace) {
		phanditype = ItemType::Mace;
	}

	switch (monster.data().monsterClass) {
	case MonsterClass::Undead:
		if (phanditype == ItemType::Sword) {
			dam -= dam / 2;
		} else if (phanditype == ItemType::Mace) {
			dam += dam / 2;
		}
		break;
	case MonsterClass::Animal:
		if (phanditype == ItemType::Mace) {
			dam -= dam / 2;
		} else if (phanditype == ItemType::Sword) {
			dam += dam / 2;
		}
		break;
	case MonsterClass::Demon:
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::TripleDemonDamage)) {
			dam *= 3;
		}
		break;
	}

	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Devastation) && GenerateRnd(100) < 5) {
		dam *= 3;
	}

	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Doppelganger) && monster.type().type != MT_DIABLO && !monster.isUnique() && GenerateRnd(100) < 10) {
		AddDoppelganger(monster);
	}

	dam <<= 6;
	if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Jesters)) {
		int r = GenerateRnd(201);
		if (r >= 100)
			r = 100 + (r - 100) * 5;
		dam = dam * r / 100;
	}

	if (adjacentDamage)
		dam >>= 2;

	if (&player == MyPlayer) {
		if (HasAnyOf(player.pDamAcFlags, ItemSpecialEffectHf::Peril)) {
			dam2 += player._pIGetHit << 6;
			if (dam2 >= 0) {
				ApplyPlrDamage(DamageType::Physical, player, Fixed26_6::fromRaw(dam2), 1);
			}
			dam *= 2;
		}
#ifdef _DEBUG
		if (DebugGodMode) {
			dam = monster.hitPoints.raw(); /* ensure monster is killed with one hit */
		}
#endif
		ApplyMonsterDamage(DamageType::Physical, monster, Fixed26_6::fromRaw(dam));
	}

	int skdam = 0;
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::RandomStealLife)) {
		skdam = GenerateRnd(dam / 8);
		player._pHitPoints += Fixed26_6::fromRaw(skdam);
		if (player._pHitPoints > player._pMaxHP) {
			player._pHitPoints = player._pMaxHP;
		}
		player._pHPBase += Fixed26_6::fromRaw(skdam);
		if (player._pHPBase > player._pMaxHPBase) {
			player._pHPBase = player._pMaxHPBase;
		}
		RedrawComponent(PanelDrawComponent::Health);
	}
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealMana3 | ItemSpecialEffect::StealMana5) && HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealMana3)) {
			skdam = 3 * dam / 100;
		}
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealMana5)) {
			skdam = 5 * dam / 100;
		}
		player._pMana += Fixed26_6::fromRaw(skdam);
		if (player._pMana > player._pMaxMana) {
			player._pMana = player._pMaxMana;
		}
		player._pManaBase += Fixed26_6::fromRaw(skdam);
		if (player._pManaBase > player._pMaxManaBase) {
			player._pManaBase = player._pMaxManaBase;
		}
		RedrawComponent(PanelDrawComponent::Mana);
	}
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealLife3 | ItemSpecialEffect::StealLife5)) {
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealLife3)) {
			skdam = 3 * dam / 100;
		}
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::StealLife5)) {
			skdam = 5 * dam / 100;
		}
		player._pHitPoints += Fixed26_6::fromRaw(skdam);
		if (player._pHitPoints > player._pMaxHP) {
			player._pHitPoints = player._pMaxHP;
		}
		player._pHPBase += Fixed26_6::fromRaw(skdam);
		if (player._pHPBase > player._pMaxHPBase) {
			player._pHPBase = player._pMaxHPBase;
		}
		RedrawComponent(PanelDrawComponent::Health);
	}
	if (monster.hasNoLife()) {
		M_StartKill(monster, player);
	} else {
		if (monster.mode != MonsterMode::Petrified && HasAnyOf(player._pIFlags, ItemSpecialEffect::Knockback))
			M_GetKnockback(monster, player.position.tile);
		M_StartHit(monster, player, dam);
	}
	return true;
}

bool PlrHitPlr(Player &attacker, Player &target)
{
	if (target._pInvincible) {
		return false;
	}

	if (HasAnyOf(target._pSpellFlags, SpellFlag::Etherealize)) {
		return false;
	}

	const int hit = GenerateRnd(100);

	int hper = attacker.GetMeleeToHit() - target.GetArmor();
	hper = std::clamp(hper, 5, 95);

	int blk = 100;
	if ((target._pmode == PM_STAND || target._pmode == PM_ATTACK) && target._pBlockFlag) {
		blk = GenerateRnd(100);
	}

	int blkper = target.GetBlockChance() - (attacker.getCharacterLevel() * 2);
	blkper = std::clamp(blkper, 0, 100);

	if (hit >= hper) {
		return false;
	}

	if (blk < blkper) {
		const Direction dir = GetDirection(target.position.tile, attacker.position.tile);
		StartPlrBlock(target, dir);
		return true;
	}

	const int mind = attacker._pIMinDam;
	const int maxd = attacker._pIMaxDam;
	int dam = RandomIntBetween(mind, maxd);
	dam += (dam * attacker._pIBonusDam) / 100;
	dam += attacker._pIBonusDamMod + attacker._pDamageMod;

	const ClassAttributes &classAttributes = GetClassAttributes(attacker._pClass);
	if (HasAnyOf(classAttributes.classFlags, PlayerClassFlag::CriticalStrike)) {
		if (GenerateRnd(100) < attacker.getCharacterLevel()) {
			dam *= 2;
		}
	}
	const Fixed26_6 skdam = Fixed26_6::fromInt(dam);
	if (HasAnyOf(attacker._pIFlags, ItemSpecialEffect::RandomStealLife)) {
		const int tac = GenerateRnd((skdam / 8).raw());
		attacker._pHitPoints += Fixed26_6::fromRaw(tac);
		if (attacker._pHitPoints > attacker._pMaxHP) {
			attacker._pHitPoints = attacker._pMaxHP;
		}
		attacker._pHPBase += Fixed26_6::fromRaw(tac);
		if (attacker._pHPBase > attacker._pMaxHPBase) {
			attacker._pHPBase = attacker._pMaxHPBase;
		}
		RedrawComponent(PanelDrawComponent::Health);
	}
	if (&attacker == MyPlayer) {
		NetSendCmdDamage(true, target, skdam.raw(), DamageType::Physical);
	}
	StartPlrHit(target, skdam, false);

	return true;
}

bool PlrHitObj(const Player &player, Object &targetObject)
{
	if (targetObject.IsBreakable()) {
		BreakObject(player, targetObject);
		return true;
	}

	return false;
}

void StartZealLockout(Player &player)
{
	// One full vanilla swing period (frames * ticks per frame), counted from
	// the moment the last hit of the swing/pair resolved.
	// 1.5x the regular swing period: the zeal pair is faster than a normal
	// swing cycle, so the wait after it is slightly longer to compensate.
	player._pZealLock = (static_cast<int16_t>(player._pAFrames) * player.AnimInfo.ticksPerFrame * 3) / 2;
}

bool DoAttack(Player &player)
{
	if (player.AnimInfo.currentFrame == player._pAFNum - 2) {
		PlaySfxLoc(SfxID::Swing, player.position.tile);
	}

	bool didhit = false;

	if (player.AnimInfo.currentFrame == player._pAFNum - 1) {
		Point position = player.position.tile + player._pdir;
		Monster *monster = FindMonsterAtPosition(position);

		if (monster != nullptr) {
			if (CanTalkToMonst(*monster)) {
				player.position.temp.x = 0; /** @todo Looks to be irrelevant, probably just remove it */
				return false;
			}
		}

		if (monster != nullptr) {
			didhit = PlrHitMonst(player, *monster);
		} else if (PlayerAtPosition(position) != nullptr && !player.friendlyMode) {
			didhit = PlrHitPlr(player, *PlayerAtPosition(position));
		} else {
			Object *object = FindObjectAtPosition(position, false);
			if (object != nullptr) {
				didhit = PlrHitObj(player, *object);
			}
		}

		if (!gbIsHellfire || !HasAllOf(player._pIFlags, ItemSpecialEffect::FireDamage | ItemSpecialEffect::LightningDamage)) {
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FireDamage)) {
				int elementalDamage = didhit ? RandomIntBetween(player._pIFMinDam, player._pIFMaxDam) : 0;
				AddMissile(position, { 1, 0 }, Direction::South, MissileID::WeaponExplosion, TARGET_MONSTERS, player, elementalDamage, 0);
			}
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::LightningDamage)) {
				int elementalDamage = didhit ? RandomIntBetween(player._pILMinDam, player._pILMaxDam) : 0;
				AddMissile(position, { 2, 0 }, Direction::South, MissileID::WeaponExplosion, TARGET_MONSTERS, player, elementalDamage, 0);
			}
		}

		if (player.CanCleave()) {
			// playing as a class/weapon with cleave
			position = player.position.tile + Right(player._pdir);
			monster = FindMonsterAtPosition(position);
			if (monster != nullptr) {
				if (!CanTalkToMonst(*monster) && monster->position.old == position) {
					if (PlrHitMonst(player, *monster, true))
						didhit = true;
				}
			}
			position = player.position.tile + Left(player._pdir);
			monster = FindMonsterAtPosition(position);
			if (monster != nullptr) {
				if (!CanTalkToMonst(*monster) && monster->position.old == position) {
					if (PlrHitMonst(player, *monster, true))
						didhit = true;
				}
			}
		}

		if (didhit && DamageWeapon(player, 30)) {
			StartStand(player, player._pdir);
			ClearStateVariables(player);
			return true;
		}

		// Warrior Zeal: queue the second free swing at the action frame, right
		// after the first hit lands or whiffs. CheckNewPath executes the queued
		// attack in the same tick and replaces the rest of this animation, so
		// the pair flows like one motion and the normal inter-swing delay
		// happens after the pair, not between the two swings. This mirrors how
		// the vanilla hold-repeat starts its next swing mid-animation.
		if (player._pClass == HeroClass::Warrior && player._pZealSwing == 1) {
			Direction zealDir = player._pdir;
			Monster *zealTarget = FindMonsterAtPosition(player.position.tile + zealDir);
			if (zealTarget == nullptr || zealTarget->hasNoLife() || CanTalkToMonst(*zealTarget)) {
				zealTarget = nullptr;
				for (int i = 0; i < 8; i++) {
					const Direction candidate = static_cast<Direction>(i);
					Monster *monster = FindMonsterAtPosition(player.position.tile + candidate);
					if (monster != nullptr && !monster->hasNoLife() && !CanTalkToMonst(*monster)) {
						zealDir = candidate;
						zealTarget = monster;
						break;
					}
				}
			}
			if (zealTarget != nullptr) {
				player.destAction = ACTION_ATTACKMON;
				player.destParam1 = static_cast<int>(zealTarget->getId());
				player._pZealSwing = 2;
			} else {
				// No second target: the pair ends here, start the lockout.
				StartZealLockout(player);
			}
		}

		// Warrior Zeal: the second swing ends the pair. Start the lockout so
		// the next pair waits one full vanilla swing period (the same time
		// that passes between two normal swings) instead of chaining
		// non-stop. The lockout counts down while the second swing's
		// recovery animation plays and while standing afterwards.
		if (player._pClass == HeroClass::Warrior && player._pZealSwing == 2)
			StartZealLockout(player);
	}

	if (player.AnimInfo.isLastFrame()) {
		StartStand(player, player._pdir);
		ClearStateVariables(player);
		return true;
	}

	return false;
}

bool DoRangeAttack(Player &player)
{
	int arrows = 0;
	if (player.AnimInfo.currentFrame == player._pAFNum - 1) {
		arrows = 1;
	}

	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::MultipleArrows) && player.AnimInfo.currentFrame == player._pAFNum + 1) {
		arrows = 2;
	}

	for (int arrow = 0; arrow < arrows; arrow++) {
		int xoff = 0;
		int yoff = 0;
		if (arrows != 1) {
			const int angle = arrow == 0 ? -1 : 1;
			const int x = player.position.temp.x - player.position.tile.x;
			if (x != 0)
				yoff = x < 0 ? angle : -angle;
			const int y = player.position.temp.y - player.position.tile.y;
			if (y != 0)
				xoff = y < 0 ? -angle : angle;
		}

		int dmg = 4;
		MissileID mistype = MissileID::Arrow;
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FireArrows)) {
			mistype = MissileID::FireArrow;
		}
		if (HasAnyOf(player._pIFlags, ItemSpecialEffect::LightningArrows)) {
			mistype = MissileID::LightningArrow;
		}
		if (HasAllOf(player._pIFlags, ItemSpecialEffect::FireArrows | ItemSpecialEffect::LightningArrows)) {
			// Fixed off by 1 error from Hellfire
			dmg = RandomIntBetween(player._pIFMinDam, player._pIFMaxDam);
			mistype = MissileID::SpectralArrow;
		}

		AddMissile(
		    player.position.tile,
		    player.position.temp + Displacement { xoff, yoff },
		    player._pdir,
		    mistype,
		    TARGET_MONSTERS,
		    player,
		    dmg,
		    0);

		if (arrow == 0 && mistype != MissileID::SpectralArrow) {
			PlaySfxLoc(arrows != 1 ? SfxID::ShootBow2 : SfxID::ShootBow, player.position.tile);
		}

		if (DamageWeapon(player, 40)) {
			StartStand(player, player._pdir);
			ClearStateVariables(player);
			return true;
		}
	}

	if (player.AnimInfo.isLastFrame()) {
		StartStand(player, player._pdir);
		ClearStateVariables(player);
		return true;
	}
	return false;
}

void DamageParryItem(Player &player, unsigned damageFrequency, int numDurLost)
{
	if (&player != MyPlayer)
		return;

	if (!FlipCoin(damageFrequency))
		return;

	const int minHandSlot = INVLOC_HAND_LEFT;
	const int maxHandSlot = INVLOC_HAND_RIGHT;

	for (int slot = minHandSlot; slot <= maxHandSlot; slot++) {
		Item &item = player.InvBody[slot];

		if (item.isEmpty())
			continue;

		if (IsNoneOf(item._itype, ItemType::Shield, ItemType::Staff))
			continue;

		if (item._iMaxDur != DUR_INDESTRUCTIBLE) {
			item._iDurability -= numDurLost;

			if (item._iDurability <= 0) {
				RemoveEquipment(player, static_cast<inv_body_loc>(slot), true);
				CalcPlrInv(player, true);
			}
		}
	}
}

bool DoBlock(Player &player)
{
	if (player.AnimInfo.isLastFrame()) {
		StartStand(player, player._pdir);
		ClearStateVariables(player);
		DamageParryItem(player, 10, 1);
		return true;
	}

	return false;
}

void DamageArmor(Player &player, int numDurLost)
{
	if (&player != MyPlayer)
		return;

	Item &armor = player.InvBody[INVLOC_CHEST];
	Item &helmet = player.InvBody[INVLOC_HEAD];

	if (armor.isEmpty() && helmet.isEmpty())
		return;

	const bool coin = FlipCoin(3);
	const bool targetHelmet = armor.isEmpty() ? true : helmet.isEmpty() ? false
	                                                                    : coin;
	int slot = targetHelmet ? INVLOC_HEAD : INVLOC_CHEST;
	Item &item = player.InvBody[slot];

	if (item._iMaxDur == DUR_INDESTRUCTIBLE)
		return;

	item._iDurability -= numDurLost;

	if (item._iDurability <= 0) {
		RemoveEquipment(player, static_cast<inv_body_loc>(slot), true);
		CalcPlrInv(player, true);
	}
}

bool DoSpell(Player &player)
{
	if (player.AnimInfo.currentFrame == player._pSFNum) {
		CastSpell(
		    player,
		    player.executedSpell.spellId,
		    player.position.tile,
		    player.position.temp,
		    player.executedSpell.spellLevel);

		if (IsAnyOf(player.executedSpell.spellType, SpellType::Scroll, SpellType::Charges)) {
			EnsureValidReadiedSpell(player);
		}
	}

	if (player.AnimInfo.isLastFrame()) {
		StartStand(player, player._pdir);
		ClearStateVariables(player);
		return true;
	}

	return false;
}

bool DoGotHit(Player &player)
{
	if (player.AnimInfo.isLastFrame()) {
		StartStand(player, player._pdir);
		ClearStateVariables(player);
		if (!FlipCoin(4)) {
			DamageArmor(player, 1);
		}

		return true;
	}

	return false;
}

bool DoDeath(Player &player)
{
	if (player.AnimInfo.isLastFrame()) {
		if (player.AnimInfo.tickCounterOfCurrentFrame == 0) {
			player.AnimInfo.ticksPerFrame = 100;
			dFlags[player.position.tile.x][player.position.tile.y] |= DungeonFlag::DeadPlayer;
		} else if (&player == MyPlayer && player.AnimInfo.tickCounterOfCurrentFrame == 30) {
			MyPlayerIsDead = true;
		}
	}

	return false;
}

void TryDisarm(const Player &player, Object &object)
{
	if (&player == MyPlayer)
		NewCursor(CURSOR_HAND);
	if (!object._oTrapFlag) {
		return;
	}
	const int trapdisper = (2 * player._pDexterity) - (5 * currlevel);
	if (GenerateRnd(100) > trapdisper) {
		return;
	}
	for (int j = 0; j < ActiveObjectCount; j++) {
		Object &trap = Objects[ActiveObjects[j]];
		if (trap.IsTrap() && FindObjectAtPosition({ trap._oVar1, trap._oVar2 }) == &object) {
			trap._oVar4 = 1;
			object._oTrapFlag = false;
		}
	}
	if (object.IsTrappedChest()) {
		object._oTrapFlag = false;
	}
}

void CheckNewPath(Player &player, bool pmWillBeCalled)
{
	int x = 0;
	int y = 0;

	Monster *monster;
	Player *target;
	Object *object;
	Item *item;

	const int targetId = player.destParam1;

	switch (player.destAction) {
	case ACTION_ATTACKMON:
	case ACTION_RATTACKMON:
	case ACTION_SPELLMON:
		monster = &Monsters[targetId];
		if (monster->hasNoLife()) {
			player.Stop();
			return;
		}
		if (player.destAction == ACTION_ATTACKMON)
			MakePlrPath(player, monster->position.future, false);
		break;
	case ACTION_ATTACKPLR:
	case ACTION_RATTACKPLR:
	case ACTION_SPELLPLR:
		target = &Players[targetId];
		if (target->hasNoLife()) {
			player.Stop();
			return;
		}
		if (player.destAction == ACTION_ATTACKPLR)
			MakePlrPath(player, target->position.future, false);
		break;
	case ACTION_OPERATE:
	case ACTION_DISARM:
	case ACTION_OPERATETK:
		object = &Objects[targetId];
		break;
	case ACTION_PICKUPITEM:
	case ACTION_PICKUPAITEM:
		item = &Items[targetId];
		break;
	default:
		break;
	}

	Direction d;
	if (player.walkpath[0] != WALK_NONE) {
		if (player._pmode == PM_STAND) {
			if (&player == MyPlayer) {
				if (player.destAction == ACTION_ATTACKMON || player.destAction == ACTION_ATTACKPLR) {
					if (player.destAction == ACTION_ATTACKMON) {
						x = std::abs(player.position.future.x - monster->position.future.x);
						y = std::abs(player.position.future.y - monster->position.future.y);
						d = GetDirection(player.position.future, monster->position.future);
					} else {
						x = std::abs(player.position.future.x - target->position.future.x);
						y = std::abs(player.position.future.y - target->position.future.y);
						d = GetDirection(player.position.future, target->position.future);
					}

					if (x < 2 && y < 2) {
						ClrPlrPath(player);
						if (player.destAction == ACTION_ATTACKMON && monster->talkMsg != TEXT_NONE && monster->talkMsg != TEXT_VILE14) {
							TalktoMonster(player, *monster);
						} else {
							StartAttack(player, d, pmWillBeCalled);
						}
						player.destAction = ACTION_NONE;
					}
				}
			}

			switch (player.walkpath[0]) {
			case WALK_N:
				StartWalk(player, Direction::North, pmWillBeCalled);
				break;
			case WALK_NE:
				StartWalk(player, Direction::NorthEast, pmWillBeCalled);
				break;
			case WALK_E:
				StartWalk(player, Direction::East, pmWillBeCalled);
				break;
			case WALK_SE:
				StartWalk(player, Direction::SouthEast, pmWillBeCalled);
				break;
			case WALK_S:
				StartWalk(player, Direction::South, pmWillBeCalled);
				break;
			case WALK_SW:
				StartWalk(player, Direction::SouthWest, pmWillBeCalled);
				break;
			case WALK_W:
				StartWalk(player, Direction::West, pmWillBeCalled);
				break;
			case WALK_NW:
				StartWalk(player, Direction::NorthWest, pmWillBeCalled);
				break;
			}

			for (size_t j = 1; j < MaxPathLengthPlayer; j++) {
				player.walkpath[j - 1] = player.walkpath[j];
			}

			player.walkpath[MaxPathLengthPlayer - 1] = WALK_NONE;

			if (player._pmode == PM_STAND) {
				StartStand(player, player._pdir);
				player.destAction = ACTION_NONE;
			}
		}

		return;
	}
	if (player.destAction == ACTION_NONE) {
		return;
	}

	if (player._pmode == PM_STAND) {
		switch (player.destAction) {
		case ACTION_ATTACK:
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartAttack(player, d, pmWillBeCalled);
			break;
		case ACTION_ATTACKMON:
			x = std::abs(player.position.tile.x - monster->position.future.x);
			y = std::abs(player.position.tile.y - monster->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, monster->position.future);
				if (monster->talkMsg != TEXT_NONE && monster->talkMsg != TEXT_VILE14) {
					TalktoMonster(player, *monster);
				} else {
					StartAttack(player, d, pmWillBeCalled);
				}
			}
			break;
		case ACTION_ATTACKPLR:
			x = std::abs(player.position.tile.x - target->position.future.x);
			y = std::abs(player.position.tile.y - target->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, target->position.future);
				StartAttack(player, d, pmWillBeCalled);
			}
			break;
		case ACTION_RATTACK:
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartRangeAttack(player, d, player.destParam1, player.destParam2, pmWillBeCalled);
			break;
		case ACTION_RATTACKMON:
			d = GetDirection(player.position.future, monster->position.future);
			if (monster->talkMsg != TEXT_NONE && monster->talkMsg != TEXT_VILE14) {
				TalktoMonster(player, *monster);
			} else {
				StartRangeAttack(player, d, monster->position.future.x, monster->position.future.y, pmWillBeCalled);
			}
			break;
		case ACTION_RATTACKPLR:
			d = GetDirection(player.position.future, target->position.future);
			StartRangeAttack(player, d, target->position.future.x, target->position.future.y, pmWillBeCalled);
			break;
		case ACTION_SPELL:
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartSpell(player, d, player.destParam1, player.destParam2);
			break;
		case ACTION_SPELLWALL:
			StartSpell(player, static_cast<Direction>(player.destParam3), player.destParam1, player.destParam2);
			player.tempDirection = static_cast<Direction>(player.destParam3);
			break;
		case ACTION_SPELLMON:
			d = GetDirection(player.position.tile, monster->position.future);
			StartSpell(player, d, monster->position.future.x, monster->position.future.y);
			break;
		case ACTION_SPELLPLR:
			d = GetDirection(player.position.tile, target->position.future);
			StartSpell(player, d, target->position.future.x, target->position.future.y);
			break;
		case ACTION_OPERATE:
			if (IsPlayerAdjacentToObject(player, *object)) {
				if (object->_oBreak == 1) {
					d = GetDirection(player.position.tile, object->position);
					StartAttack(player, d, pmWillBeCalled);
				} else {
					OperateObject(player, *object);
				}
			}
			break;
		case ACTION_DISARM:
			if (IsPlayerAdjacentToObject(player, *object)) {
				if (object->_oBreak == 1) {
					d = GetDirection(player.position.tile, object->position);
					StartAttack(player, d, pmWillBeCalled);
				} else {
					TryDisarm(player, *object);
					OperateObject(player, *object);
				}
			}
			break;
		case ACTION_OPERATETK:
			if (object->_oBreak != 1) {
				OperateObject(player, *object);
			}
			break;
		case ACTION_PICKUPITEM:
			if (&player == MyPlayer) {
				x = std::abs(player.position.tile.x - item->position.x);
				y = std::abs(player.position.tile.y - item->position.y);
				if (x <= 1 && y <= 1 && pcurs == CURSOR_HAND && !item->_iRequest) {
					NetSendCmdGItem(true, CMD_REQUESTGITEM, player, targetId);
					item->_iRequest = true;
				}
			}
			break;
		case ACTION_PICKUPAITEM:
			if (&player == MyPlayer) {
				x = std::abs(player.position.tile.x - item->position.x);
				y = std::abs(player.position.tile.y - item->position.y);
				if (x <= 1 && y <= 1 && pcurs == CURSOR_HAND) {
					NetSendCmdGItem(true, CMD_REQUESTAGITEM, player, targetId);
				}
			}
			break;
		case ACTION_TALK:
			if (&player == MyPlayer) {
				HelpFlag = false;
				TalkToTowner(player, player.destParam1);
			}
			break;
		default:
			break;
		}

		FixPlayerLocation(player, player._pdir);
		player.destAction = ACTION_NONE;

		return;
	}

	if (player._pmode == PM_ATTACK && player.AnimInfo.currentFrame >= player._pAFNum) {
		if (player.destAction == ACTION_ATTACK) {
			d = GetDirection(player.position.future, { player.destParam1, player.destParam2 });
			StartAttack(player, d, pmWillBeCalled);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_ATTACKMON) {
			x = std::abs(player.position.tile.x - monster->position.future.x);
			y = std::abs(player.position.tile.y - monster->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, monster->position.future);
				StartAttack(player, d, pmWillBeCalled);
			}
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_ATTACKPLR) {
			x = std::abs(player.position.tile.x - target->position.future.x);
			y = std::abs(player.position.tile.y - target->position.future.y);
			if (x <= 1 && y <= 1) {
				d = GetDirection(player.position.future, target->position.future);
				StartAttack(player, d, pmWillBeCalled);
			}
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_OPERATE) {
			if (IsPlayerAdjacentToObject(player, *object)) {
				if (object->_oBreak == 1) {
					d = GetDirection(player.position.tile, object->position);
					StartAttack(player, d, pmWillBeCalled);
				}
			}
		}
	}

	if (player._pmode == PM_RATTACK && player.AnimInfo.currentFrame >= player._pAFNum) {
		if (player.destAction == ACTION_RATTACK) {
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartRangeAttack(player, d, player.destParam1, player.destParam2, pmWillBeCalled);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_RATTACKMON) {
			d = GetDirection(player.position.tile, monster->position.future);
			StartRangeAttack(player, d, monster->position.future.x, monster->position.future.y, pmWillBeCalled);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_RATTACKPLR) {
			d = GetDirection(player.position.tile, target->position.future);
			StartRangeAttack(player, d, target->position.future.x, target->position.future.y, pmWillBeCalled);
			player.destAction = ACTION_NONE;
		}
	}

	if (player._pmode == PM_SPELL && player.AnimInfo.currentFrame >= player._pSFNum) {
		if (player.destAction == ACTION_SPELL) {
			d = GetDirection(player.position.tile, { player.destParam1, player.destParam2 });
			StartSpell(player, d, player.destParam1, player.destParam2);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_SPELLMON) {
			d = GetDirection(player.position.tile, monster->position.future);
			StartSpell(player, d, monster->position.future.x, monster->position.future.y);
			player.destAction = ACTION_NONE;
		} else if (player.destAction == ACTION_SPELLPLR) {
			d = GetDirection(player.position.tile, target->position.future);
			StartSpell(player, d, target->position.future.x, target->position.future.y);
			player.destAction = ACTION_NONE;
		}
	}
}

bool PlrDeathModeOK(Player &player)
{
	if (&player != MyPlayer) {
		return true;
	}
	if (player._pmode == PM_DEATH) {
		return true;
	}
	if (player._pmode == PM_QUIT) {
		return true;
	}
	if (player._pmode == PM_NEWLVL) {
		return true;
	}

	return false;
}

void ValidatePlayer()
{
	assert(MyPlayer != nullptr);
	Player &myPlayer = *MyPlayer;

	// Player::setCharacterLevel ensures that the player level is within the expected range in case someone has edited their character level in memory
	myPlayer.setCharacterLevel(myPlayer.getCharacterLevel());
	// This lets us catch cases where someone is editing experience directly through memory modification and reset their experience back to the expected cap.
	if (myPlayer._pExperience > myPlayer.getNextExperienceThreshold()) {
		myPlayer._pExperience = myPlayer.getNextExperienceThreshold();
		if (*GetOptions().Gameplay.experienceBar) {
			RedrawEverything();
		}
	}

	int gt = 0;
	for (int i = 0; i < myPlayer._pNumInv; i++) {
		if (myPlayer.InvList[i]._itype == ItemType::Gold) {
			int maxGold = GOLD_MAX_LIMIT;
			if (gbIsHellfire) {
				maxGold *= 2;
			}
			if (myPlayer.InvList[i]._ivalue > maxGold) {
				myPlayer.InvList[i]._ivalue = maxGold;
			}
			gt += myPlayer.InvList[i]._ivalue;
		}
	}
	if (gt != myPlayer._pGold)
		myPlayer._pGold = gt;

	if (myPlayer._pBaseStr > myPlayer.GetMaximumAttributeValue(CharacterAttribute::Strength)) {
		myPlayer._pBaseStr = myPlayer.GetMaximumAttributeValue(CharacterAttribute::Strength);
	}
	if (myPlayer._pBaseMag > myPlayer.GetMaximumAttributeValue(CharacterAttribute::Magic)) {
		myPlayer._pBaseMag = myPlayer.GetMaximumAttributeValue(CharacterAttribute::Magic);
	}
	if (myPlayer._pBaseDex > myPlayer.GetMaximumAttributeValue(CharacterAttribute::Dexterity)) {
		myPlayer._pBaseDex = myPlayer.GetMaximumAttributeValue(CharacterAttribute::Dexterity);
	}
	if (myPlayer._pBaseVit > myPlayer.GetMaximumAttributeValue(CharacterAttribute::Vitality)) {
		myPlayer._pBaseVit = myPlayer.GetMaximumAttributeValue(CharacterAttribute::Vitality);
	}

	uint64_t msk = 0;
	for (auto b = static_cast<size_t>(SpellID::Firebolt); b < SpellsData.size(); b++) {
		if (GetSpellBookLevel((SpellID)b) != -1) {
			msk |= GetSpellBitmask(static_cast<SpellID>(b));
			if (myPlayer._pSplLvl[b] > MaxSpellLevel)
				myPlayer._pSplLvl[b] = MaxSpellLevel;
		}
	}

	myPlayer._pMemSpells &= msk;
	myPlayer._pInfraFlag = false;
}

HeroClass GetPlayerSpriteClass(HeroClass cls)
{
	if (cls == HeroClass::Bard && !HaveBardAssets())
		return HeroClass::Rogue;
	if (cls == HeroClass::Barbarian && !HaveBarbarianAssets())
		return HeroClass::Warrior;
	return cls;
}

PlayerWeaponGraphic GetPlayerWeaponGraphic(player_graphic graphic, PlayerWeaponGraphic weaponGraphic)
{
	if (leveltype == DTYPE_TOWN && IsAnyOf(graphic, player_graphic::Lightning, player_graphic::Fire, player_graphic::Magic)) {
		// If the hero doesn't hold the weapon in town then we should use the unarmed animation for casting
		switch (weaponGraphic) {
		case PlayerWeaponGraphic::Mace:
		case PlayerWeaponGraphic::Sword:
			return PlayerWeaponGraphic::Unarmed;
		case PlayerWeaponGraphic::SwordShield:
		case PlayerWeaponGraphic::MaceShield:
			return PlayerWeaponGraphic::UnarmedShield;
		default:
			break;
		}
	}
	return weaponGraphic;
}

uint16_t GetPlayerSpriteWidth(HeroClass cls, player_graphic graphic, PlayerWeaponGraphic weaponGraphic)
{
	const PlayerSpriteData spriteData = GetPlayerSpriteDataForClass(cls);

	switch (graphic) {
	case player_graphic::Stand:
		return spriteData.stand;
	case player_graphic::Walk:
		return spriteData.walk;
	case player_graphic::Attack:
		if (weaponGraphic == PlayerWeaponGraphic::Bow)
			return spriteData.bow;
		return spriteData.attack;
	case player_graphic::Hit:
		return spriteData.swHit;
	case player_graphic::Block:
		return spriteData.block;
	case player_graphic::Lightning:
		return spriteData.lightning;
	case player_graphic::Fire:
		return spriteData.fire;
	case player_graphic::Magic:
		return spriteData.magic;
	case player_graphic::Death:
		return spriteData.death;
	}
	app_fatal("Invalid player_graphic");
}

void GetPlayerGraphicsPath(std::string_view path, std::string_view prefix, std::string_view type, char out[256])
{
	*BufCopy(out, "plrgfx\\", path, "\\", prefix, "\\", prefix, type) = '\0';
}

} // namespace

void Player::CalcScrolls()
{
	_pScrlSpells = 0;
	for (const Item &item : InventoryAndBeltPlayerItemsRange { *this }) {
		if (item.isScroll() && item._iStatFlag) {
			_pScrlSpells |= GetSpellBitmask(item._iSpell);
		}
	}
	EnsureValidReadiedSpell(*this);
}

bool Player::CanUseItem(const Item &item) const
{
	if (!IsItemValid(*this, item))
		return false;

	return _pStrength >= item._iMinStr
	    && _pMagic >= item._iMinMag
	    && _pDexterity >= item._iMinDex;
}

void Player::RemoveInvItem(int iv, bool calcScrolls)
{
	if (this == MyPlayer) {
		// Locate the first grid index containing this item and notify remote clients
		for (size_t i = 0; i < InventoryGridCells; i++) {
			const int8_t itemIndex = InvGrid[i];
			if (std::abs(itemIndex) - 1 == iv) {
				NetSendCmdParam1(false, CMD_DELINVITEMS, static_cast<uint16_t>(i));
				break;
			}
		}
	}

	// Iterate through invGrid and remove every reference to item
	for (int8_t &itemIndex : InvGrid) {
		if (std::abs(itemIndex) - 1 == iv) {
			itemIndex = 0;
		}
	}

	InvList[iv].clear();

	_pNumInv--;

	// If the item at the end of inventory array isn't the one removed, shift all following items back one index to retain inventory order.
	if (_pNumInv > 0 && _pNumInv != iv) {
		for (int newIndex = iv; newIndex < _pNumInv; newIndex++) {
			InvList[newIndex] = InvList[newIndex + 1].pop();
		}

		for (int8_t &itemIndex : InvGrid) {
			if (itemIndex > iv + 1) { // if item was shifted, decrease the index so it's paired with the correct item.
				itemIndex--;
			}
			if (itemIndex < -(iv + 1)) {
				itemIndex++; // since occupied cells are negative, increment the index to keep it same as as top-left cell for item, only negative.
			}
		}
	}

	if (calcScrolls) {
		CalcScrolls();
	}
}

void Player::RemoveSpdBarItem(int iv)
{
	if (this == MyPlayer) {
		NetSendCmdParam1(false, CMD_DELBELTITEMS, iv);
	}

	SpdList[iv].clear();

	CalcScrolls();
	RedrawEverything();
}

[[nodiscard]] uint8_t Player::getId() const
{
	return static_cast<uint8_t>(std::distance<const Player *>(&Players[0], this));
}

int Player::GetBaseAttributeValue(CharacterAttribute attribute) const
{
	switch (attribute) {
	case CharacterAttribute::Dexterity:
		return this->_pBaseDex;
	case CharacterAttribute::Magic:
		return this->_pBaseMag;
	case CharacterAttribute::Strength:
		return this->_pBaseStr;
	case CharacterAttribute::Vitality:
		return this->_pBaseVit;
	default:
		app_fatal("Unsupported attribute");
	}
}

int Player::GetCurrentAttributeValue(CharacterAttribute attribute) const
{
	switch (attribute) {
	case CharacterAttribute::Dexterity:
		return this->_pDexterity;
	case CharacterAttribute::Magic:
		return this->_pMagic;
	case CharacterAttribute::Strength:
		return this->_pStrength;
	case CharacterAttribute::Vitality:
		return this->_pVitality;
	default:
		app_fatal("Unsupported attribute");
	}
}

int Player::GetMaximumAttributeValue(CharacterAttribute attribute) const
{
	const ClassAttributes &attr = getClassAttributes();
	switch (attribute) {
	case CharacterAttribute::Strength:
		return attr.maxStr;
	case CharacterAttribute::Magic:
		return attr.maxMag;
	case CharacterAttribute::Dexterity:
		return attr.maxDex;
	case CharacterAttribute::Vitality:
		return attr.maxVit;
	}
	app_fatal("Unsupported attribute");
}

Point Player::GetTargetPosition() const
{
	// clang-format off
	constexpr int DirectionOffsetX[8] = {  0,-1, 1, 0,-1, 1, 1,-1 };
	constexpr int DirectionOffsetY[8] = { -1, 0, 0, 1,-1,-1, 1, 1 };
	// clang-format on
	Point target = position.future;
	for (auto step : walkpath) {
		if (step == WALK_NONE)
			break;
		if (step > 0) {
			target.x += DirectionOffsetX[step - 1];
			target.y += DirectionOffsetY[step - 1];
		}
	}
	return target;
}

int Player::GetPositionPathIndex(Point pos)
{
	constexpr Displacement DirectionOffset[8] = { { 0, -1 }, { -1, 0 }, { 1, 0 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
	Point target = position.future;
	int i = 0;
	for (auto step : walkpath) {
		if (target == pos) return i;
		if (step == WALK_NONE)
			break;
		if (step > 0) {
			target += DirectionOffset[step - 1];
		}
		++i;
	}
	return -1;
}

void Player::Say(HeroSpeech speechId) const
{
	const SfxID soundEffect = GetHeroSound(_pClass, speechId);

	if (soundEffect == SfxID::None)
		return;

	PlaySfxLoc(soundEffect, position.tile);
}

void Player::SaySpecific(HeroSpeech speechId) const
{
	const SfxID soundEffect = GetHeroSound(_pClass, speechId);

	if (soundEffect == SfxID::None || effect_is_playing(soundEffect))
		return;

	PlaySfxLoc(soundEffect, position.tile, false);
}

void Player::Say(HeroSpeech speechId, int delay) const
{
	sfxdelay = delay;
	sfxdnum = GetHeroSound(_pClass, speechId);
}

void Player::Stop()
{
	ClrPlrPath(*this);
	destAction = ACTION_NONE;
}

bool Player::isWalking() const
{
	return IsAnyOf(_pmode, PM_WALK_NORTHWARDS, PM_WALK_SOUTHWARDS, PM_WALK_SIDEWAYS);
}

int Player::GetManaShieldDamageReduction()
{
	constexpr uint8_t Max = 7;
	return 24 - (std::min(_pSplLvl[static_cast<int8_t>(SpellID::ManaShield)], Max) * 3);
}

void Player::RestorePartialLife()
{
	const int wholeHitpoints = _pMaxHP.whole();
	Fixed26_6 l = Fixed26_6::fromInt((wholeHitpoints / 8) + GenerateRnd(wholeHitpoints / 4));
	const ClassAttributes &classAttributes = GetClassAttributes(_pClass);
	l = l * classAttributes.itmRestoreLife;
	_pHitPoints = std::min(_pHitPoints + l, _pMaxHP);
	_pHPBase = std::min(_pHPBase + l, _pMaxHPBase);
}

void Player::RestorePartialMana()
{
	const int wholeManaPoints = _pMaxMana.whole();
	Fixed26_6 l = Fixed26_6::fromInt((wholeManaPoints / 8) + GenerateRnd(wholeManaPoints / 4));
	const ClassAttributes &classAttributes = GetClassAttributes(_pClass);
	l = l * classAttributes.itmRestoreMana;
	if (HasNoneOf(_pIFlags, ItemSpecialEffect::NoMana)) {
		_pMana = std::min(_pMana + l, _pMaxMana);
		_pManaBase = std::min(_pManaBase + l, _pMaxManaBase);
	}
}

void Player::ReadySpellFromEquipment(inv_body_loc bodyLocation, bool forceSpell)
{
	const Item &item = InvBody[bodyLocation];
	if (item._itype == ItemType::Staff && IsValidSpell(item._iSpell) && item._iCharges > 0 && item._iStatFlag) {
		if (forceSpell || _pRSpell == SpellID::Invalid || _pRSplType == SpellType::Invalid) {
			_pRSpell = item._iSpell;
			_pRSplType = SpellType::Charges;
			RedrawEverything();
		}
	}
}

player_graphic Player::getGraphic() const
{
	switch (_pmode) {
	case PM_STAND:
	case PM_NEWLVL:
	case PM_QUIT:
		return player_graphic::Stand;
	case PM_WALK_NORTHWARDS:
	case PM_WALK_SOUTHWARDS:
	case PM_WALK_SIDEWAYS:
		return player_graphic::Walk;
	case PM_ATTACK:
	case PM_RATTACK:
		return player_graphic::Attack;
	case PM_BLOCK:
		return player_graphic::Block;
	case PM_SPELL:
		return GetPlayerGraphicForSpell(executedSpell.spellId);
	case PM_GOTHIT:
		return player_graphic::Hit;
	case PM_DEATH:
		return player_graphic::Death;
	default:
		app_fatal("SyncPlrAnim");
	}
}

uint16_t Player::getSpriteWidth() const
{
	if (!HeadlessMode)
		return (*AnimInfo.sprites)[0].width();
	const player_graphic graphic = getGraphic();
	const HeroClass cls = GetPlayerSpriteClass(_pClass);
	const PlayerWeaponGraphic weaponGraphic = GetPlayerWeaponGraphic(graphic, static_cast<PlayerWeaponGraphic>(_pgfxnum & 0xF));
	return GetPlayerSpriteWidth(cls, graphic, weaponGraphic);
}

void Player::getAnimationFramesAndTicksPerFrame(player_graphic graphics, int8_t &numberOfFrames, int8_t &ticksPerFrame) const
{
	ticksPerFrame = 1;
	switch (graphics) {
	case player_graphic::Stand:
		numberOfFrames = _pNFrames;
		ticksPerFrame = 4;
		break;
	case player_graphic::Walk:
		numberOfFrames = _pWFrames;
		break;
	case player_graphic::Attack:
		numberOfFrames = _pAFrames;
		break;
	case player_graphic::Hit:
		numberOfFrames = _pHFrames;
		break;
	case player_graphic::Lightning:
	case player_graphic::Fire:
	case player_graphic::Magic:
		numberOfFrames = _pSFrames;
		break;
	case player_graphic::Death:
		numberOfFrames = _pDFrames;
		ticksPerFrame = 2;
		break;
	case player_graphic::Block:
		numberOfFrames = _pBFrames;
		ticksPerFrame = 3;
		break;
	default:
		app_fatal("Unknown player graphics");
	}
}

void Player::UpdatePreviewCelSprite(_cmd_id cmdId, Point point, uint16_t wParam1, uint16_t wParam2)
{
	// if game is not running don't show a preview
	if (!gbRunGame || PauseMode != 0 || !gbProcessPlayers)
		return;

	// we can only show a preview if our command is executed in the next game tick
	if (_pmode != PM_STAND)
		return;

	std::optional<player_graphic> graphic;
	Direction dir = Direction::South;
	int minimalWalkDistance = -1;

	switch (cmdId) {
	case _cmd_id::CMD_RATTACKID: {
		const Monster &monster = Monsters[wParam1];
		dir = GetDirection(position.future, monster.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_SPELLID: {
		const Monster &monster = Monsters[wParam1];
		dir = GetDirection(position.future, monster.position.future);
		graphic = GetPlayerGraphicForSpell(static_cast<SpellID>(wParam2));
		break;
	}
	case _cmd_id::CMD_ATTACKID: {
		const Monster &monster = Monsters[wParam1];
		point = monster.position.future;
		minimalWalkDistance = 2;
		if (!CanTalkToMonst(monster)) {
			dir = GetDirection(position.future, monster.position.future);
			graphic = player_graphic::Attack;
		}
		break;
	}
	case _cmd_id::CMD_RATTACKPID: {
		const Player &targetPlayer = Players[wParam1];
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_SPELLPID: {
		const Player &targetPlayer = Players[wParam1];
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = GetPlayerGraphicForSpell(static_cast<SpellID>(wParam2));
		break;
	}
	case _cmd_id::CMD_ATTACKPID: {
		const Player &targetPlayer = Players[wParam1];
		point = targetPlayer.position.future;
		minimalWalkDistance = 2;
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_RATTACKXY:
	case _cmd_id::CMD_SATTACKXY:
		dir = GetDirection(position.tile, point);
		graphic = player_graphic::Attack;
		break;
	case _cmd_id::CMD_SPELLXY:
		dir = GetDirection(position.tile, point);
		graphic = GetPlayerGraphicForSpell(static_cast<SpellID>(wParam1));
		break;
	case _cmd_id::CMD_SPELLXYD:
		dir = static_cast<Direction>(wParam2);
		graphic = GetPlayerGraphicForSpell(static_cast<SpellID>(wParam1));
		break;
	case _cmd_id::CMD_WALKXY:
		minimalWalkDistance = 1;
		break;
	case _cmd_id::CMD_TALKXY:
	case _cmd_id::CMD_DISARMXY:
	case _cmd_id::CMD_OPOBJXY:
	case _cmd_id::CMD_GOTOGETITEM:
	case _cmd_id::CMD_GOTOAGETITEM:
		minimalWalkDistance = 2;
		break;
	default:
		return;
	}

	if (minimalWalkDistance >= 0 && position.future != point) {
		int8_t testWalkPath[MaxPathLengthPlayer];
		const int steps = FindPath(CanStep, [this](Point tile) { return PosOkPlayer(*this, tile); }, position.future, point, testWalkPath, MaxPathLengthPlayer);
		if (steps == 0) {
			// Can't walk to desired location => stand still
			return;
		}
		if (steps >= minimalWalkDistance) {
			graphic = player_graphic::Walk;
			switch (testWalkPath[0]) {
			case WALK_N:
				dir = Direction::North;
				break;
			case WALK_NE:
				dir = Direction::NorthEast;
				break;
			case WALK_E:
				dir = Direction::East;
				break;
			case WALK_SE:
				dir = Direction::SouthEast;
				break;
			case WALK_S:
				dir = Direction::South;
				break;
			case WALK_SW:
				dir = Direction::SouthWest;
				break;
			case WALK_W:
				dir = Direction::West;
				break;
			case WALK_NW:
				dir = Direction::NorthWest;
				break;
			}
			if (!PlrDirOK(*this, dir))
				return;
		}
	}

	if (!graphic || HeadlessMode)
		return;

	LoadPlrGFX(*this, *graphic);
	const ClxSpriteList sprites = AnimationData[static_cast<size_t>(*graphic)].spritesForDirection(dir);
	if (!previewCelSprite || *previewCelSprite != sprites[0]) {
		previewCelSprite = sprites[0];
		progressToNextGameTickWhenPreviewWasSet = ProgressToNextGameTick;
	}
}

void Player::setCharacterLevel(uint8_t level)
{
	this->_pLevel = std::clamp<uint8_t>(level, 1U, getMaxCharacterLevel());
}

uint8_t Player::getMaxCharacterLevel() const
{
	return GetMaximumCharacterLevel();
}

uint32_t Player::getNextExperienceThreshold() const
{
	return GetNextExperienceThresholdForLevel(this->getCharacterLevel());
}

Fixed26_6 Player::calculateBaseLife() const
{
	const ClassAttributes &attr = getClassAttributes();
	return attr.adjLife + attr.lvlLife * getCharacterLevel() + attr.chrLife * _pBaseVit;
}

Fixed26_6 Player::calculateBaseMana() const
{
	const ClassAttributes &attr = getClassAttributes();
	return attr.adjMana + attr.lvlMana * getCharacterLevel() + attr.chrMana * _pBaseMag;
}

void Player::occupyTile(Point tilePosition, bool isMoving) const
{
	int16_t id = this->getId();
	id += 1;
	dPlayer[tilePosition.x][tilePosition.y] = isMoving ? -id : id;
	MarkWalkabilityChanged();
}

bool Player::isLevelOwnedByLocalClient() const
{
	for (const Player &other : Players) {
		if (!other.plractive)
			continue;
		if (other._pLvlChanging)
			continue;
		if (other._pmode == PM_NEWLVL)
			continue;
		if (other.plrlevel != this->plrlevel)
