/**
 * @file player.cpp
 *
 * Implementation of player functionality, leveling, actions, creation, loading, etc.
 */
#include <algorithm>
#include <cmath>
#include <cstdint>

#include <fmt/core.h>

#include "control.h"
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
#include "minitext.h"
#include "missiles.h"
#include "nthread.h"
#include "objects.h"
#include "options.h"
#include "player.h"
#include "qol/autopickup.h"
#include "qol/floatingnumbers.h"
#include "qol/stash.h"
#include "spells.h"
#include "stores.h"
#include "towners.h"
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
	Point position = player.position.tile;
	Point futurePosition = position + dir;
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
	NewPlrAnim(player, player_graphic::Walk, dir, AnimationDistributionFlags::ProcessAnimationPending, skippedFrames);
}

/**
 * @brief Start moving a player to a new tile
 */
void StartWalk(Player &player, Direction dir, bool pmWillBeCalled)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
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
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
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
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
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
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
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

	int ii = AllocateItem();
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
	int remainingGold = DropGold(player, player._pGold / 2, true);
	if (remainingGold > 0) {
		DropGold(player, remainingGold, false);
	}

	player._pGold /= 2;
}

void InitLevelChange(Player &player)
{
	Player &myPlayer = *MyPlayer;

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
		int midam = RandomIntBetween(player._pIFMinDam, player._pIFMaxDam);
		AddMissile(player.position.tile, player.position.temp, player._pdir, MissileID::SpectralArrow, TARGET_MONSTERS, player, midam, 0);
	}
	int mind = player._pIMinDam;
	int maxd = player._pIMaxDam;
	int dam = RandomIntBetween(mind, maxd);
	dam += dam * player._pIBonusDam / 100;
	dam += player._pIBonusDamMod;
	int dam2 = dam << 6;
	dam += player._pDamageMod;
	if (player._pClass == HeroClass::Warrior || player._pClass == HeroClass::Barbarian) {
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
				ApplyPlrDamage(DamageType::Physical, player, 0, 1, dam2);
			}
			dam *= 2;
		}
#ifdef _DEBUG
		if (DebugGodMode) {
			dam = monster.hitPoints; /* ensure monster is killed with one hit */
		}
#endif
		ApplyMonsterDamage(DamageType::Physical, monster, dam);
	}

	int skdam = 0;
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::RandomStealLife)) {
		skdam = GenerateRnd(dam / 8);
		player._pHitPoints += skdam;
		if (player._pHitPoints > player._pMaxHP) {
			player._pHitPoints = player._pMaxHP;
		}
		player._pHPBase += skdam;
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
		player._pMana += skdam;
		if (player._pMana > player._pMaxMana) {
			player._pMana = player._pMaxMana;
		}
		player._pManaBase += skdam;
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
		player._pHitPoints += skdam;
		if (player._pHitPoints > player._pMaxHP) {
			player._pHitPoints = player._pMaxHP;
		}
		player._pHPBase += skdam;
		if (player._pHPBase > player._pMaxHPBase) {
			player._pHPBase = player._pMaxHPBase;
		}
		RedrawComponent(PanelDrawComponent::Health);
	}
	if ((monster.hitPoints >> 6) <= 0) {
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

	int hit = GenerateRnd(100);

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
		Direction dir = GetDirection(target.position.tile, attacker.position.tile);
		StartPlrBlock(target, dir);
		return true;
	}

	int mind = attacker._pIMinDam;
	int maxd = attacker._pIMaxDam;
	int dam = RandomIntBetween(mind, maxd);
	dam += (dam * attacker._pIBonusDam) / 100;
	dam += attacker._pIBonusDamMod + attacker._pDamageMod;

	if (attacker._pClass == HeroClass::Warrior || attacker._pClass == HeroClass::Barbarian) {
		if (GenerateRnd(100) < attacker.getCharacterLevel()) {
			dam *= 2;
		}
	}
	int skdam = dam << 6;
	if (HasAnyOf(attacker._pIFlags, ItemSpecialEffect::RandomStealLife)) {
		int tac = GenerateRnd(skdam / 8);
		attacker._pHitPoints += tac;
		if (attacker._pHitPoints > attacker._pMaxHP) {
			attacker._pHitPoints = attacker._pMaxHP;
		}
		attacker._pHPBase += tac;
		if (attacker._pHPBase > attacker._pMaxHPBase) {
			attacker._pHPBase = attacker._pMaxHPBase;
		}
		RedrawComponent(PanelDrawComponent::Health);
	}
	if (&attacker == MyPlayer) {
		NetSendCmdDamage(true, target, skdam, DamageType::Physical);
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

		if (!gbIsHellfire || !HasAllOf(player._pIFlags, ItemSpecialEffect::FireDamage | ItemSpecialEffect::LightningDamage)) {
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FireDamage)) {
				AddMissile(position, { 1, 0 }, Direction::South, MissileID::WeaponExplosion, TARGET_MONSTERS, player, 0, 0);
			}
			if (HasAnyOf(player._pIFlags, ItemSpecialEffect::LightningDamage)) {
				AddMissile(position, { 2, 0 }, Direction::South, MissileID::WeaponExplosion, TARGET_MONSTERS, player, 0, 0);
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
			int angle = arrow == 0 ? -1 : 1;
			int x = player.position.temp.x - player.position.tile.x;
			if (x != 0)
				yoff = x < 0 ? angle : -angle;
			int y = player.position.temp.y - player.position.tile.y;
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

void DamageParryItem(Player &player)
{
	if (&player != MyPlayer) {
		return;
	}

	if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield || player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Staff) {
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == DUR_INDESTRUCTIBLE) {
			return;
		}

		player.InvBody[INVLOC_HAND_LEFT]._iDurability--;
		if (player.InvBody[INVLOC_HAND_LEFT]._iDurability == 0) {
			RemoveEquipment(player, INVLOC_HAND_LEFT, true);
			CalcPlrInv(player, true);
		}
	}

	if (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield) {
		if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability != DUR_INDESTRUCTIBLE) {
			player.InvBody[INVLOC_HAND_RIGHT]._iDurability--;
			if (player.InvBody[INVLOC_HAND_RIGHT]._iDurability == 0) {
				RemoveEquipment(player, INVLOC_HAND_RIGHT, true);
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

		if (FlipCoin(10)) {
			DamageParryItem(player);
		}
		return true;
	}

	return false;
}

void DamageArmor(Player &player)
{
	if (&player != MyPlayer) {
		return;
	}

	if (player.InvBody[INVLOC_CHEST].isEmpty() && player.InvBody[INVLOC_HEAD].isEmpty()) {
		return;
	}

	bool targetHead = FlipCoin(3);
	if (!player.InvBody[INVLOC_CHEST].isEmpty() && player.InvBody[INVLOC_HEAD].isEmpty()) {
		targetHead = false;
	}
	if (player.InvBody[INVLOC_CHEST].isEmpty() && !player.InvBody[INVLOC_HEAD].isEmpty()) {
		targetHead = true;
	}

	Item *pi;
	if (targetHead) {
		pi = &player.InvBody[INVLOC_HEAD];
	} else {
		pi = &player.InvBody[INVLOC_CHEST];
	}
	if (pi->_iDurability == DUR_INDESTRUCTIBLE) {
		return;
	}

	pi->_iDurability--;
	if (pi->_iDurability != 0) {
		return;
	}

	if (targetHead) {
		RemoveEquipment(player, INVLOC_HEAD, true);
	} else {
		RemoveEquipment(player, INVLOC_CHEST, true);
	}
	CalcPlrInv(player, true);
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
			DamageArmor(player);
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

bool IsPlayerAdjacentToObject(Player &player, Object &object)
{
	int x = std::abs(player.position.tile.x - object.position.x);
	int y = std::abs(player.position.tile.y - object.position.y);
	if (y > 1 && object.position.y >= 1 && FindObjectAtPosition(object.position + Direction::NorthEast) == &object) {
		// special case for activating a large object from the north-east side
		y = std::abs(player.position.tile.y - object.position.y + 1);
	}
	return x <= 1 && y <= 1;
}

void TryDisarm(const Player &player, Object &object)
{
	if (&player == MyPlayer)
		NewCursor(CURSOR_HAND);
	if (!object._oTrapFlag) {
		return;
	}
	int trapdisper = 2 * player._pDexterity - 5 * currlevel;
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

	int targetId = player.destParam1;

	switch (player.destAction) {
	case ACTION_ATTACKMON:
	case ACTION_RATTACKMON:
	case ACTION_SPELLMON:
		monster = &Monsters[targetId];
		if ((monster->hitPoints >> 6) <= 0) {
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
		if ((target->_pHitPoints >> 6) <= 0) {
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
	for (int b = static_cast<int8_t>(SpellID::Firebolt); b < MAX_SPELLS; b++) {
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
	PlayerSpriteData spriteData = PlayersSpriteData[static_cast<size_t>(cls)];

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

} // namespace

void Player::CalcScrolls()
{
	_pScrlSpells = 0;
	for (Item &item : InventoryAndBeltPlayerItemsRange { *this }) {
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
			int8_t itemIndex = InvGrid[i];
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
	SfxID soundEffect = herosounds[static_cast<size_t>(_pClass)][static_cast<size_t>(speechId)];

	if (soundEffect == SfxID::None)
		return;

	PlaySfxLoc(soundEffect, position.tile);
}

void Player::SaySpecific(HeroSpeech speechId) const
{
	SfxID soundEffect = herosounds[static_cast<size_t>(_pClass)][static_cast<size_t>(speechId)];

	if (soundEffect == SfxID::None || effect_is_playing(soundEffect))
		return;

	PlaySfxLoc(soundEffect, position.tile, false);
}

void Player::Say(HeroSpeech speechId, int delay) const
{
	sfxdelay = delay;
	sfxdnum = herosounds[static_cast<size_t>(_pClass)][static_cast<size_t>(speechId)];
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
	return 24 - std::min(_pSplLvl[static_cast<int8_t>(SpellID::ManaShield)], Max) * 3;
}

void Player::RestorePartialLife()
{
	int wholeHitpoints = _pMaxHP >> 6;
	int l = ((wholeHitpoints / 8) + GenerateRnd(wholeHitpoints / 4)) << 6;
	if (IsAnyOf(_pClass, HeroClass::Warrior, HeroClass::Barbarian))
		l *= 2;
	if (IsAnyOf(_pClass, HeroClass::Rogue, HeroClass::Monk, HeroClass::Bard))
		l += l / 2;
	_pHitPoints = std::min(_pHitPoints + l, _pMaxHP);
	_pHPBase = std::min(_pHPBase + l, _pMaxHPBase);
}

void Player::RestorePartialMana()
{
	int wholeManaPoints = _pMaxMana >> 6;
	int l = ((wholeManaPoints / 8) + GenerateRnd(wholeManaPoints / 4)) << 6;
	if (_pClass == HeroClass::Sorcerer)
		l *= 2;
	if (IsAnyOf(_pClass, HeroClass::Rogue, HeroClass::Monk, HeroClass::Bard))
		l += l / 2;
	if (HasNoneOf(_pIFlags, ItemSpecialEffect::NoMana)) {
		_pMana = std::min(_pMana + l, _pMaxMana);
		_pManaBase = std::min(_pManaBase + l, _pMaxManaBase);
	}
}

void Player::ReadySpellFromEquipment(inv_body_loc bodyLocation, bool forceSpell)
{
	Item &item = InvBody[bodyLocation];
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
		Monster &monster = Monsters[wParam1];
		dir = GetDirection(position.future, monster.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_SPELLID: {
		Monster &monster = Monsters[wParam1];
		dir = GetDirection(position.future, monster.position.future);
		graphic = GetPlayerGraphicForSpell(static_cast<SpellID>(wParam2));
		break;
	}
	case _cmd_id::CMD_ATTACKID: {
		Monster &monster = Monsters[wParam1];
		point = monster.position.future;
		minimalWalkDistance = 2;
		if (!CanTalkToMonst(monster)) {
			dir = GetDirection(position.future, monster.position.future);
			graphic = player_graphic::Attack;
		}
		break;
	}
	case _cmd_id::CMD_RATTACKPID: {
		Player &targetPlayer = Players[wParam1];
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_SPELLPID: {
		Player &targetPlayer = Players[wParam1];
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = GetPlayerGraphicForSpell(static_cast<SpellID>(wParam2));
		break;
	}
	case _cmd_id::CMD_ATTACKPID: {
		Player &targetPlayer = Players[wParam1];
		point = targetPlayer.position.future;
		minimalWalkDistance = 2;
		dir = GetDirection(position.future, targetPlayer.position.future);
		graphic = player_graphic::Attack;
		break;
	}
	case _cmd_id::CMD_ATTACKXY:
		dir = GetDirection(position.tile, point);
		graphic = player_graphic::Attack;
		minimalWalkDistance = 2;
		break;
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
		int steps = FindPath(CanStep, [this](Point position) { return PosOkPlayer(*this, position); }, position.future, point, testWalkPath, MaxPathLengthPlayer);
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
	ClxSpriteList sprites = AnimationData[static_cast<size_t>(*graphic)].spritesForDirection(dir);
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

int32_t Player::calculateBaseLife() const
{
	const ClassAttributes &attr = getClassAttributes();
	return attr.adjLife + (attr.lvlLife * getCharacterLevel()) + (attr.chrLife * _pBaseVit);
}

int32_t Player::calculateBaseMana() const
{
	const ClassAttributes &attr = getClassAttributes();
	return attr.adjMana + (attr.lvlMana * getCharacterLevel()) + (attr.chrMana * _pBaseMag);
}

void Player::occupyTile(Point position, bool isMoving) const
{
	int16_t id = this->getId();
	id += 1;
	dPlayer[position.x][position.y] = isMoving ? -id : id;
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
			continue;
		if (other.plrIsOnSetLevel != this->plrIsOnSetLevel)
			continue;
		if (&other == MyPlayer && gbBufferMsgs != 0)
			continue;
		return &other == MyPlayer;
	}

	return false;
}

Player *PlayerAtPosition(Point position, bool ignoreMovingPlayers /*= false*/)
{
	if (!InDungeonBounds(position))
		return nullptr;

	auto playerIndex = dPlayer[position.x][position.y];
	if (playerIndex == 0 || (ignoreMovingPlayers && playerIndex < 0))
		return nullptr;

	return &Players[std::abs(playerIndex) - 1];
}

void LoadPlrGFX(Player &player, player_graphic graphic)
{
	if (HeadlessMode)
		return;

	auto &animationData = player.AnimationData[static_cast<size_t>(graphic)];
	if (animationData.sprites)
		return;

	const HeroClass cls = GetPlayerSpriteClass(player._pClass);
	const PlayerWeaponGraphic animWeaponId = GetPlayerWeaponGraphic(graphic, static_cast<PlayerWeaponGraphic>(player._pgfxnum & 0xF));

	const char *path = PlayersSpriteData[static_cast<std::size_t>(cls)].classPath;

	const char *szCel;
	switch (graphic) {
	case player_graphic::Stand:
		szCel = "as";
		if (leveltype == DTYPE_TOWN)
			szCel = "st";
		break;
	case player_graphic::Walk:
		szCel = "aw";
		if (leveltype == DTYPE_TOWN)
			szCel = "wl";
		break;
	case player_graphic::Attack:
		if (leveltype == DTYPE_TOWN)
			return;
		szCel = "at";
		break;
	case player_graphic::Hit:
		if (leveltype == DTYPE_TOWN)
			return;
		szCel = "ht";
		break;
	case player_graphic::Lightning:
		szCel = "lm";
		break;
	case player_graphic::Fire:
		szCel = "fm";
		break;
	case player_graphic::Magic:
		szCel = "qm";
		break;
	case player_graphic::Death:
		if (animWeaponId != PlayerWeaponGraphic::Unarmed)
			return;
		szCel = "dt";
		break;
	case player_graphic::Block:
		if (leveltype == DTYPE_TOWN)
			return;
		if (!player._pBlockFlag)
			return;
		szCel = "bl";
		break;
	default:
		app_fatal("PLR:2");
	}

	char prefix[3] = { CharChar[static_cast<std::size_t>(cls)], ArmourChar[player._pgfxnum >> 4], WepChar[static_cast<std::size_t>(animWeaponId)] };
	char pszName[256];
	*fmt::format_to(pszName, R"(plrgfx\{0}\{1}\{1}{2})", path, std::string_view(prefix, 3), szCel) = 0;
	const uint16_t animationWidth = GetPlayerSpriteWidth(cls, graphic, animWeaponId);
	animationData.sprites = LoadCl2Sheet(pszName, animationWidth);
	std::optional<std::array<uint8_t, 256>> graphicTRN = GetPlayerGraphicTRN(pszName);
	if (graphicTRN) {
		ClxApplyTrans(*animationData.sprites, graphicTRN->data());
	}
	std::optional<std::array<uint8_t, 256>> classTRN = GetClassTRN(player);
	if (classTRN) {
		ClxApplyTrans(*animationData.sprites, classTRN->data());
	}
}

void InitPlayerGFX(Player &player)
{
	if (HeadlessMode)
		return;

	ResetPlayerGFX(player);

	if (player._pHitPoints >> 6 == 0) {
		player._pgfxnum &= ~0xFU;
		LoadPlrGFX(player, player_graphic::Death);
		return;
	}

	for (size_t i = 0; i < enum_size<player_graphic>::value; i++) {
		auto graphic = static_cast<player_graphic>(i);
		if (graphic == player_graphic::Death)
			continue;
		LoadPlrGFX(player, graphic);
	}
}

void ResetPlayerGFX(Player &player)
{
	player.AnimInfo.sprites = std::nullopt;
	for (PlayerAnimationData &animData : player.AnimationData) {
		animData.sprites = std::nullopt;
	}
}

void NewPlrAnim(Player &player, player_graphic graphic, Direction dir, AnimationDistributionFlags flags /*= AnimationDistributionFlags::None*/, int8_t numSkippedFrames /*= 0*/, int8_t distributeFramesBeforeFrame /*= 0*/)
{
	LoadPlrGFX(player, graphic);

	OptionalClxSpriteList sprites;
	int previewShownGameTickFragments = 0;
	if (!HeadlessMode) {
		sprites = player.AnimationData[static_cast<size_t>(graphic)].spritesForDirection(dir);
		if (player.previewCelSprite && (*sprites)[0] == *player.previewCelSprite && !player.isWalking()) {
			previewShownGameTickFragments = std::clamp<int>(AnimationInfo::baseValueFraction - player.progressToNextGameTickWhenPreviewWasSet, 0, AnimationInfo::baseValueFraction);
		}
	}

	int8_t numberOfFrames;
	int8_t ticksPerFrame;
	player.getAnimationFramesAndTicksPerFrame(graphic, numberOfFrames, ticksPerFrame);
	player.AnimInfo.setNewAnimation(sprites, numberOfFrames, ticksPerFrame, flags, numSkippedFrames, distributeFramesBeforeFrame, static_cast<uint8_t>(previewShownGameTickFragments));
}

void SetPlrAnims(Player &player)
{
	HeroClass pc = player._pClass;
	PlayerAnimData plrAtkAnimData = PlayersAnimData[static_cast<uint8_t>(pc)];
	auto gn = static_cast<PlayerWeaponGraphic>(player._pgfxnum & 0xFU);

	if (leveltype == DTYPE_TOWN) {
		player._pNFrames = plrAtkAnimData.townIdleFrames;
		player._pWFrames = plrAtkAnimData.townWalkingFrames;
	} else {
		player._pNFrames = plrAtkAnimData.idleFrames;
		player._pWFrames = plrAtkAnimData.walkingFrames;
		player._pHFrames = plrAtkAnimData.recoveryFrames;
		player._pBFrames = plrAtkAnimData.blockingFrames;
		switch (gn) {
		case PlayerWeaponGraphic::Unarmed:
			player._pAFrames = plrAtkAnimData.unarmedFrames;
			player._pAFNum = plrAtkAnimData.unarmedActionFrame;
			break;
		case PlayerWeaponGraphic::UnarmedShield:
			player._pAFrames = plrAtkAnimData.unarmedShieldFrames;
			player._pAFNum = plrAtkAnimData.unarmedShieldActionFrame;
			break;
		case PlayerWeaponGraphic::Sword:
			player._pAFrames = plrAtkAnimData.swordFrames;
			player._pAFNum = plrAtkAnimData.swordActionFrame;
			break;
		case PlayerWeaponGraphic::SwordShield:
			player._pAFrames = plrAtkAnimData.swordShieldFrames;
			player._pAFNum = plrAtkAnimData.swordShieldActionFrame;
			break;
		case PlayerWeaponGraphic::Bow:
			player._pAFrames = plrAtkAnimData.bowFrames;
			player._pAFNum = plrAtkAnimData.bowActionFrame;
			break;
		case PlayerWeaponGraphic::Axe:
			player._pAFrames = plrAtkAnimData.axeFrames;
			player._pAFNum = plrAtkAnimData.axeActionFrame;
			break;
		case PlayerWeaponGraphic::Mace:
			player._pAFrames = plrAtkAnimData.maceFrames;
			player._pAFNum = plrAtkAnimData.maceActionFrame;
			break;
		case PlayerWeaponGraphic::MaceShield:
			player._pAFrames = plrAtkAnimData.maceShieldFrames;
			player._pAFNum = plrAtkAnimData.maceShieldActionFrame;
			break;
		case PlayerWeaponGraphic::Staff:
			player._pAFrames = plrAtkAnimData.staffFrames;
			player._pAFNum = plrAtkAnimData.staffActionFrame;
			break;
		}
	}

	player._pDFrames = plrAtkAnimData.deathFrames;
	player._pSFrames = plrAtkAnimData.castingFrames;
	player._pSFNum = plrAtkAnimData.castingActionFrame;
	int armorGraphicIndex = player._pgfxnum & ~0xFU;
	if (IsAnyOf(pc, HeroClass::Warrior, HeroClass::Barbarian)) {
		if (gn == PlayerWeaponGraphic::Bow && leveltype != DTYPE_TOWN)
			player._pNFrames = 8;
		if (armorGraphicIndex > 0)
			player._pDFrames = 15;
	}
}

/**
 * @param player The player reference.
 * @param c The hero class.
 */
void CreatePlayer(Player &player, HeroClass c)
{
	player = {};
	SetRndSeed(SDL_GetTicks());

	player.setCharacterLevel(1);
	player._pClass = c;

	const ClassAttributes &attr = player.getClassAttributes();

	player._pBaseStr = attr.baseStr;
	player._pStrength = player._pBaseStr;

	player._pBaseMag = attr.baseMag;
	player._pMagic = player._pBaseMag;

	player._pBaseDex = attr.baseDex;
	player._pDexterity = player._pBaseDex;

	player._pBaseVit = attr.baseVit;
	player._pVitality = player._pBaseVit;

	player._pHitPoints = player.calculateBaseLife();
	player._pMaxHP = player._pHitPoints;
	player._pHPBase = player._pHitPoints;
	player._pMaxHPBase = player._pHitPoints;

	player._pMana = player.calculateBaseMana();
	player._pMaxMana = player._pMana;
	player._pManaBase = player._pMana;
	player._pMaxManaBase = player._pMana;

	player._pExperience = 0;
	player._pArmorClass = 0;
	player._pLightRad = 10;
	player._pInfraFlag = false;

	for (uint8_t &spellLevel : player._pSplLvl) {
		spellLevel = 0;
	}

	player._pSpellFlags = SpellFlag::None;
	player._pRSplType = SpellType::Invalid;

	// Initializing the hotkey bindings to no selection
	std::fill(player._pSplHotKey, player._pSplHotKey + NumHotkeys, SpellID::Invalid);

	// CreatePlrItems calls AutoEquip which will overwrite the player graphic if required
	player._pgfxnum = static_cast<uint8_t>(PlayerWeaponGraphic::Unarmed);

	for (bool &levelVisited : player._pLvlVisited) {
		levelVisited = false;
	}

	for (int i = 0; i < 10; i++) {
		player._pSLvlVisited[i] = false;
	}

	player._pLvlChanging = false;
	player.pTownWarps = 0;
	player.pLvlLoad = 0;
	player.pManaShield = false;
	player.pDamAcFlags = ItemSpecialEffectHf::None;
	player.wReflections = 0;

	InitDungMsgs(player);
	CreatePlrItems(player);
	SetRndSeed(0);
}

int CalcStatDiff(Player &player)
{
	int diff = 0;
	for (auto attribute : enum_values<CharacterAttribute>()) {
		diff += player.GetMaximumAttributeValue(attribute);
		diff -= player.GetBaseAttributeValue(attribute);
	}
	return diff;
}

void NextPlrLevel(Player &player)
{
	player.setCharacterLevel(player.getCharacterLevel() + 1);

	CalcPlrInv(player, true);

	if (CalcStatDiff(player) < 5) {
		player._pStatPts = CalcStatDiff(player);
	} else {
		player._pStatPts += 5;
	}
	int hp = player.getClassAttributes().lvlLife;

	player._pMaxHP += hp;
	player._pHitPoints = player._pMaxHP;
	player._pMaxHPBase += hp;
	player._pHPBase = player._pMaxHPBase;

	if (&player == MyPlayer) {
		RedrawComponent(PanelDrawComponent::Health);
	}

	int mana = player.getClassAttributes().lvlMana;

	player._pMaxMana += mana;
	player._pMaxManaBase += mana;

	if (HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		player._pMana = player._pMaxMana;
		player._pManaBase = player._pMaxManaBase;
	}

	if (&player == MyPlayer) {
		RedrawComponent(PanelDrawComponent::Mana);
	}

	if (ControlMode != ControlTypes::KeyboardAndMouse)
		FocusOnCharInfo();

	CalcPlrInv(player, true);
	PlaySFX(SfxID::ItemArmor);
	PlaySFX(SfxID::ItemSign);
}

void Player::_addExperience(uint32_t experience, int levelDelta)
{
	if (this != MyPlayer || _pHitPoints <= 0)
		return;

	if (isMaxCharacterLevel()) {
		return;
	}

	// Adjust xp based on difference between the players current level and the target level (usually a monster level)
	uint32_t clampedExp = static_cast<uint32_t>(std::clamp<int64_t>(static_cast<int64_t>(experience * (1 + levelDelta / 10.0)), 0, std::numeric_limits<uint32_t>::max()));

	// Prevent power leveling
	if (gbIsMultiplayer) {
		// for low level characters experience gain is capped to 1/20 of current levels xp
		// for high level characters experience gain is capped to 200 * current level - this is a smaller value than 1/20 of the exp needed for the next level after level 5.
		clampedExp = std::min<uint32_t>({ clampedExp, /* level 1-5: */ getNextExperienceThreshold() / 20U, /* level 6-50: */ 200U * getCharacterLevel() });
	}

	const uint32_t maxExperience = GetNextExperienceThresholdForLevel(getMaxCharacterLevel());

	// ensure we only add enough experience to reach the max experience cap so we don't overflow
	_pExperience += std::min(clampedExp, maxExperience - _pExperience);

	if (*GetOptions().Gameplay.experienceBar) {
		RedrawEverything();
	}

	// Increase player level if applicable
	while (!isMaxCharacterLevel() && _pExperience >= getNextExperienceThreshold()) {
		// NextPlrLevel increments character level which changes the next experience threshold
		NextPlrLevel(*this);
	}

	NetSendCmdParam1(false, CMD_PLRLEVEL, getCharacterLevel());
}

void AddPlrMonstExper(int lvl, unsigned exp, char pmask)
{
	unsigned totplrs = 0;
	for (size_t i = 0; i < Players.size(); i++) {
		if (((1 << i) & pmask) != 0) {
			totplrs++;
		}
	}

	if (totplrs != 0) {
		unsigned e = exp / totplrs;
		if ((pmask & (1 << MyPlayerId)) != 0)
			MyPlayer->addExperience(e, lvl);
	}
}

void InitPlayer(Player &player, bool firstTime)
{
	if (firstTime) {
		player._pRSplType = SpellType::Invalid;
		player._pRSpell = SpellID::Invalid;
		if (&player == MyPlayer)
			LoadHotkeys();
		player._pSBkSpell = SpellID::Invalid;
		player.queuedSpell.spellId = player._pRSpell;
		player.queuedSpell.spellType = player._pRSplType;
		player.pManaShield = false;
		player.wReflections = 0;
	}

	if (player.isOnActiveLevel()) {

		SetPlrAnims(player);

		ClearStateVariables(player);

		if (player._pHitPoints >> 6 > 0) {
			player._pmode = PM_STAND;
			NewPlrAnim(player, player_graphic::Stand, Direction::South);
			player.AnimInfo.currentFrame = GenerateRnd(player._pNFrames - 1);
			player.AnimInfo.tickCounterOfCurrentFrame = GenerateRnd(3);
		} else {
			player._pgfxnum &= ~0xFU;
			player._pmode = PM_DEATH;
			NewPlrAnim(player, player_graphic::Death, Direction::South);
			player.AnimInfo.currentFrame = player.AnimInfo.numberOfFrames - 2;
		}

		player._pdir = Direction::South;

		if (&player == MyPlayer && (!firstTime || leveltype != DTYPE_TOWN)) {
			player.position.tile = ViewPosition;
		}

		SetPlayerOld(player);
		player.walkpath[0] = WALK_NONE;
		player.destAction = ACTION_NONE;

		if (&player == MyPlayer) {
			player.lightId = AddLight(player.position.tile, player._pLightRad);
			ChangeLightXY(player.lightId, player.position.tile); // fix for a bug where old light is still visible at the entrance after reentering level
		} else {
			player.lightId = NO_LIGHT;
		}
		ActivateVision(player.position.tile, player._pLightRad, player.getId());
	}

	player._pAblSpells = GetSpellBitmask(GetPlayerStartingLoadoutForClass(player._pClass).skill);

	player._pInvincible = false;

	if (&player == MyPlayer) {
		MyPlayerIsDead = false;
	}
}

void InitMultiView()
{
	assert(MyPlayer != nullptr);
	ViewPosition = MyPlayer->position.tile;
}

void PlrClrTrans(Point position)
{
	for (int i = position.y - 1; i <= position.y + 1; i++) {
		for (int j = position.x - 1; j <= position.x + 1; j++) {
			TransList[dTransVal[j][i]] = false;
		}
	}
}

void PlrDoTrans(Point position)
{
	if (IsNoneOf(leveltype, DTYPE_CATHEDRAL, DTYPE_CATACOMBS, DTYPE_CRYPT)) {
		TransList[1] = true;
		return;
	}

	for (int i = position.y - 1; i <= position.y + 1; i++) {
		for (int j = position.x - 1; j <= position.x + 1; j++) {
			if (IsTileNotSolid({ j, i }) && dTransVal[j][i] != 0) {
				TransList[dTransVal[j][i]] = true;
			}
		}
	}
}

void SetPlayerOld(Player &player)
{
	player.position.old = player.position.tile;
}

void FixPlayerLocation(Player &player, Direction bDir)
{
	player.position.future = player.position.tile;
	player._pdir = bDir;
	if (&player == MyPlayer) {
		ViewPosition = player.position.tile;
	}
	ChangeLightXY(player.lightId, player.position.tile);
	ChangeVisionXY(player.getId(), player.position.tile);
}

void StartStand(Player &player, Direction dir)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	NewPlrAnim(player, player_graphic::Stand, dir);
	player._pmode = PM_STAND;
	FixPlayerLocation(player, dir);
	FixPlrWalkTags(player);
	player.occupyTile(player.position.tile, false);
	SetPlayerOld(player);
}

void StartPlrBlock(Player &player, Direction dir)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	PlaySfxLoc(SfxID::ItemSword, player.position.tile);

	int8_t skippedAnimationFrames = 0;
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastBlock)) {
		skippedAnimationFrames = (player._pBFrames - 2); // ISPL_FASTBLOCK means we cancel the animation if frame 2 was shown
	}

	NewPlrAnim(player, player_graphic::Block, dir, AnimationDistributionFlags::SkipsDelayOfLastFrame, skippedAnimationFrames);

	player._pmode = PM_BLOCK;
	FixPlayerLocation(player, dir);
	SetPlayerOld(player);
}

/**
 * @todo Figure out why clearing player.position.old sometimes fails
 */
void FixPlrWalkTags(const Player &player)
{
	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			if (PlayerAtPosition({ x, y }) == &player)
				dPlayer[x][y] = 0;
		}
	}
}

void StartPlrHit(Player &player, int dam, bool forcehit)
{
	if (player._pInvincible && player._pHitPoints == 0 && &player == MyPlayer) {
		SyncPlrKill(player, DeathReason::Unknown);
		return;
	}

	player.Say(HeroSpeech::ArghClang);

	RedrawComponent(PanelDrawComponent::Health);
	if (player._pClass == HeroClass::Barbarian) {
		if (dam >> 6 < player.getCharacterLevel() + player.getCharacterLevel() / 4 && !forcehit) {
			return;
		}
	} else if (dam >> 6 < player.getCharacterLevel() && !forcehit) {
		return;
	}

	Direction pd = player._pdir;

	int8_t skippedAnimationFrames = 0;
	if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastestHitRecovery)) {
		skippedAnimationFrames = 3;
	} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FasterHitRecovery)) {
		skippedAnimationFrames = 2;
	} else if (HasAnyOf(player._pIFlags, ItemSpecialEffect::FastHitRecovery)) {
		skippedAnimationFrames = 1;
	} else {
		skippedAnimationFrames = 0;
	}

	NewPlrAnim(player, player_graphic::Hit, pd, AnimationDistributionFlags::None, skippedAnimationFrames);

	player._pmode = PM_GOTHIT;
	FixPlayerLocation(player, pd);
	FixPlrWalkTags(player);
	player.occupyTile(player.position.tile, false);
	SetPlayerOld(player);
}

#if defined(__clang__) || defined(__GNUC__)
__attribute__((no_sanitize("shift-base")))
#endif
void
StartPlayerKill(Player &player, DeathReason deathReason)
{
	if (player._pHitPoints <= 0 && player._pmode == PM_DEATH) {
		return;
	}

	if (&player == MyPlayer) {
		NetSendCmdParam1(true, CMD_PLRDEAD, static_cast<uint16_t>(deathReason));
		gamemenu_off();
	}

	const bool dropGold = !gbIsMultiplayer || !(player.isOnLevel(16) || player.isOnArenaLevel());
	const bool dropItems = dropGold && deathReason == DeathReason::MonsterOrTrap;
	const bool dropEar = dropGold && deathReason == DeathReason::Player;

	player.Say(HeroSpeech::AuughUh);

	// Are the current animations item dependent?
	if (player._pgfxnum != 0) {
		if (dropItems) {
			// Ensure death animation show the player without weapon and armor, because they drop on death
			player._pgfxnum = 0;
		} else {
			// Death animation aren't weapon specific, so always use the unarmed animations
			player._pgfxnum &= ~0xFU;
		}
		ResetPlayerGFX(player);
		SetPlrAnims(player);
	}

	NewPlrAnim(player, player_graphic::Death, player._pdir);

	player._pBlockFlag = false;
	player._pmode = PM_DEATH;
	player._pInvincible = true;
	SetPlayerHitPoints(player, 0);

	if (&player != MyPlayer && dropItems) {
		// Ensure that items are removed for remote players
		// The dropped items will be synced separately (by the remote client)
		for (Item &item : player.InvBody) {
			item.clear();
		}
		CalcPlrInv(player, false);
	}

	if (player.isOnActiveLevel()) {
		FixPlayerLocation(player, player._pdir);
		FixPlrWalkTags(player);
		dFlags[player.position.tile.x][player.position.tile.y] |= DungeonFlag::DeadPlayer;
		SetPlayerOld(player);

		// Only generate drops once (for the local player)
		// For remote players we get separated sync messages (by the remote client)
		if (&player == MyPlayer) {
			RedrawComponent(PanelDrawComponent::Health);

			if (!player.HoldItem.isEmpty()) {
				DeadItem(player, std::move(player.HoldItem), { 0, 0 });
				NewCursor(CURSOR_HAND);
			}
			if (dropGold) {
				DropHalfPlayersGold(player);
			}
			if (dropEar) {
				Item ear;
				InitializeItem(ear, IDI_EAR);
				CopyUtf8(ear._iName, fmt::format(fmt::runtime("Ear of {:s}"), player._pName), sizeof(ear._iName));
				CopyUtf8(ear._iIName, player._pName, ItemNameLength);
				switch (player._pClass) {
				case HeroClass::Sorcerer:
					ear._iCurs = ICURS_EAR_SORCERER;
					break;
				case HeroClass::Warrior:
					ear._iCurs = ICURS_EAR_WARRIOR;
					break;
				case HeroClass::Rogue:
				case HeroClass::Monk:
				case HeroClass::Bard:
				case HeroClass::Barbarian:
					ear._iCurs = ICURS_EAR_ROGUE;
					break;
				}

				ear._iCreateInfo = player._pName[0] << 8 | player._pName[1];
				ear._iSeed = player._pName[2] << 24 | player._pName[3] << 16 | player._pName[4] << 8 | player._pName[5];
				ear._ivalue = player.getCharacterLevel();

				if (FindGetItem(ear._iSeed, IDI_EAR, ear._iCreateInfo) == -1) {
					DeadItem(player, std::move(ear), { 0, 0 });
				}
			}
			if (dropItems) {
				Direction pdd = player._pdir;
				for (Item &item : player.InvBody) {
					pdd = Left(pdd);
					DeadItem(player, item.pop(), Displacement(pdd));
				}

				CalcPlrInv(player, false);
			}
		}
	}
	SetPlayerHitPoints(player, 0);
}

void StripTopGold(Player &player)
{
	for (Item &item : InventoryPlayerItemsRange { player }) {
		if (item._itype != ItemType::Gold)
			continue;
		if (item._ivalue <= MaxGold)
			continue;
		Item excessGold;
		MakeGoldStack(excessGold, item._ivalue - MaxGold);
		item._ivalue = MaxGold;

		if (GoldAutoPlace(player, excessGold))
			continue;
		if (!player.HoldItem.isEmpty() && ActiveItemCount + 1 >= MAXITEMS)
			continue;
		DeadItem(player, std::move(excessGold), { 0, 0 });
	}
	player._pGold = CalculateGold(player);

	if (player.HoldItem.isEmpty())
		return;
	if (AutoEquip(player, player.HoldItem, false))
		return;
	if (CanFitItemInInventory(player, player.HoldItem))
		return;
	if (AutoPlaceItemInBelt(player, player.HoldItem))
		return;
	std::optional<Point> itemTile = FindAdjacentPositionForItem(player.position.tile, player._pdir);
	if (itemTile)
		return;
	DeadItem(player, std::move(player.HoldItem), { 0, 0 });
	NewCursor(CURSOR_HAND);
}

void ApplyPlrDamage(DamageType damageType, Player &player, int dam, int minHP /*= 0*/, int frac /*= 0*/, DeathReason deathReason /*= DeathReason::MonsterOrTrap*/)
{
	int totalDamage = (dam << 6) + frac;
	if (&player == MyPlayer && player._pHitPoints > 0) {
		AddFloatingNumber(damageType, player, totalDamage);
	}
	if (totalDamage > 0 && player.pManaShield && HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		uint8_t manaShieldLevel = player._pSplLvl[static_cast<int8_t>(SpellID::ManaShield)];
		if (manaShieldLevel > 0) {
			totalDamage += totalDamage / -player.GetManaShieldDamageReduction();
		}
		if (&player == MyPlayer)
			RedrawComponent(PanelDrawComponent::Mana);
		if (player._pMana >= totalDamage) {
			player._pMana -= totalDamage;
			player._pManaBase -= totalDamage;
			totalDamage = 0;
		} else {
			totalDamage -= player._pMana;
			if (manaShieldLevel > 0) {
				totalDamage += totalDamage / (player.GetManaShieldDamageReduction() - 1);
			}
			player._pMana = 0;
			player._pManaBase = player._pMaxManaBase - player._pMaxMana;
			if (&player == MyPlayer)
				NetSendCmd(true, CMD_REMSHIELD);
		}
	}

	if (totalDamage == 0)
		return;

	RedrawComponent(PanelDrawComponent::Health);
	player._pHitPoints -= totalDamage;
	player._pHPBase -= totalDamage;
	if (player._pHitPoints > player._pMaxHP) {
		player._pHitPoints = player._pMaxHP;
		player._pHPBase = player._pMaxHPBase;
	}
	int minHitPoints = minHP << 6;
	if (player._pHitPoints < minHitPoints) {
		SetPlayerHitPoints(player, minHitPoints);
	}
	if (player._pHitPoints >> 6 <= 0) {
		SyncPlrKill(player, deathReason);
	}
}

void SyncPlrKill(Player &player, DeathReason deathReason)
{
	if (player._pHitPoints <= 0 && leveltype == DTYPE_TOWN) {
		SetPlayerHitPoints(player, 64);
		return;
	}

	SetPlayerHitPoints(player, 0);
	StartPlayerKill(player, deathReason);
}

void RemovePlrMissiles(const Player &player)
{
	if (leveltype != DTYPE_TOWN && &player == MyPlayer) {
		Monster &golem = Monsters[MyPlayerId];
		if (golem.position.tile.x != 1 || golem.position.tile.y != 0) {
			KillMyGolem();
			AddCorpse(golem.position.tile, golem.type().corpseId, golem.direction);
			int mx = golem.position.tile.x;
			int my = golem.position.tile.y;
			dMonster[mx][my] = 0;
			golem.isInvalid = true;
			DeleteMonsterList();
		}
	}

	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::StoneCurse && &Players[missile._misource] == &player) {
			Monsters[missile.var2].mode = static_cast<MonsterMode>(missile.var1);
		}
	}
}

#if defined(__clang__) || defined(__GNUC__)
__attribute__((no_sanitize("shift-base")))
#endif
void
StartNewLvl(Player &player, interface_mode fom, int lvl)
{
	InitLevelChange(player);

	switch (fom) {
	case WM_DIABNEXTLVL:
	case WM_DIABPREVLVL:
	case WM_DIABRTNLVL:
	case WM_DIABTOWNWARP:
		player.setLevel(lvl);
		break;
	case WM_DIABSETLVL:
		if (&player == MyPlayer)
			setlvlnum = (_setlevels)lvl;
		player.setLevel(setlvlnum);
		break;
	case WM_DIABTWARPUP:
		MyPlayer->pTownWarps |= 1 << (leveltype - 2);
		player.setLevel(lvl);
		break;
	case WM_DIABRETOWN:
		break;
	default:
		app_fatal("StartNewLvl");
	}

	if (&player == MyPlayer) {
		player._pmode = PM_NEWLVL;
		player._pInvincible = true;
		SDL_Event event;
		event.type = CustomEventToSdlEvent(fom);
		SDL_PushEvent(&event);
		if (gbIsMultiplayer) {
			NetSendCmdParam2(true, CMD_NEWLVL, fom, lvl);
		}
	}
}

void RestartTownLvl(Player &player)
{
	InitLevelChange(player);

	player.setLevel(0);
	player._pInvincible = false;

	SetPlayerHitPoints(player, 64);

	player._pMana = 0;
	player._pManaBase = player._pMana - (player._pMaxMana - player._pMaxManaBase);

	CalcPlrInv(player, false);
	player._pmode = PM_NEWLVL;

	if (&player == MyPlayer) {
		player._pInvincible = true;
		SDL_Event event;
		event.type = CustomEventToSdlEvent(WM_DIABRETOWN);
		SDL_PushEvent(&event);
	}
}

void StartWarpLvl(Player &player, size_t pidx)
{
	InitLevelChange(player);

	if (gbIsMultiplayer) {
		if (!player.isOnLevel(0)) {
			player.setLevel(0);
		} else {
			if (Portals[pidx].setlvl)
				player.setLevel(static_cast<_setlevels>(Portals[pidx].level));
			else
				player.setLevel(Portals[pidx].level);
		}
	}

	if (&player == MyPlayer) {
		SetCurrentPortal(pidx);
		player._pmode = PM_NEWLVL;
		player._pInvincible = true;
		SDL_Event event;
		event.type = CustomEventToSdlEvent(WM_DIABWARPLVL);
		SDL_PushEvent(&event);
	}
}

void ProcessPlayers()
{
	assert(MyPlayer != nullptr);
	Player &myPlayer = *MyPlayer;

	if (myPlayer.pLvlLoad > 0) {
		myPlayer.pLvlLoad--;
	}

	if (sfxdelay > 0) {
		sfxdelay--;
		if (sfxdelay == 0) {
			switch (sfxdnum) {
			case SfxID::Defiler1:
				InitQTextMsg(TEXT_DEFILER1);
				break;
			case SfxID::Defiler2:
				InitQTextMsg(TEXT_DEFILER2);
				break;
			case SfxID::Defiler3:
				InitQTextMsg(TEXT_DEFILER3);
				break;
			case SfxID::Defiler4:
				InitQTextMsg(TEXT_DEFILER4);
				break;
			default:
				PlaySFX(sfxdnum);
			}
		}
	}

	ValidatePlayer();

	for (size_t pnum = 0; pnum < Players.size(); pnum++) {
		Player &player = Players[pnum];
		if (player.plractive && player.isOnActiveLevel() && (&player == MyPlayer || !player._pLvlChanging)) {
			if (!PlrDeathModeOK(player) && (player._pHitPoints >> 6) <= 0) {
				SyncPlrKill(player, DeathReason::Unknown);
			}

			if (&player == MyPlayer) {
				if (HasAnyOf(player._pIFlags, ItemSpecialEffect::DrainLife) && leveltype != DTYPE_TOWN) {
					ApplyPlrDamage(DamageType::Physical, player, 0, 0, 4);
				}
				if (player.pManaShield && HasAnyOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
					NetSendCmd(true, CMD_REMSHIELD);
				}
			}

			bool tplayer = false;
			do {
				switch (player._pmode) {
				case PM_STAND:
				case PM_NEWLVL:
				case PM_QUIT:
					tplayer = false;
					break;
				case PM_WALK_NORTHWARDS:
				case PM_WALK_SOUTHWARDS:
				case PM_WALK_SIDEWAYS:
					tplayer = DoWalk(player);
					break;
				case PM_ATTACK:
					tplayer = DoAttack(player);
					break;
				case PM_RATTACK:
					tplayer = DoRangeAttack(player);
					break;
				case PM_BLOCK:
					tplayer = DoBlock(player);
					break;
				case PM_SPELL:
					tplayer = DoSpell(player);
					break;
				case PM_GOTHIT:
					tplayer = DoGotHit(player);
					break;
				case PM_DEATH:
					tplayer = DoDeath(player);
					break;
				}
				CheckNewPath(player, tplayer);
			} while (tplayer);

			player.previewCelSprite = std::nullopt;
			if (player._pmode != PM_DEATH || player.AnimInfo.tickCounterOfCurrentFrame != 40)
				player.AnimInfo.processAnimation();
		}
	}
}

void ClrPlrPath(Player &player)
{
	memset(player.walkpath, WALK_NONE, sizeof(player.walkpath));
}

/**
 * @brief Determines if the target position is clear for the given player to stand on.
 *
 * This requires an ID instead of a Player& to compare with the dPlayer lookup table values.
 *
 * @param player The player to check.
 * @param position Dungeon tile coordinates.
 * @return False if something (other than the player themselves) is blocking the tile.
 */
bool PosOkPlayer(const Player &player, Point position)
{
	if (!InDungeonBounds(position))
		return false;
	if (!IsTileWalkable(position))
		return false;
	Player *otherPlayer = PlayerAtPosition(position);
	if (otherPlayer != nullptr && otherPlayer != &player && otherPlayer->_pHitPoints != 0)
		return false;

	if (dMonster[position.x][position.y] != 0) {
		if (leveltype == DTYPE_TOWN) {
			return false;
		}
		if (dMonster[position.x][position.y] <= 0) {
			return false;
		}
		if ((Monsters[dMonster[position.x][position.y] - 1].hitPoints >> 6) > 0) {
			return false;
		}
	}

	return true;
}

void MakePlrPath(Player &player, Point targetPosition, bool endspace)
{
	if (player.position.future == targetPosition) {
		return;
	}

	int path = FindPath(CanStep, [&player](Point position) { return PosOkPlayer(player, position); }, player.position.future, targetPosition, player.walkpath, MaxPathLengthPlayer);
	if (path == 0) {
		return;
	}

	if (!endspace) {
		path--;
	}

	player.walkpath[path] = WALK_NONE;
}

void CheckPlrSpell(bool isShiftHeld, SpellID spellID, SpellType spellType)
{
	bool addflag = false;

	assert(MyPlayer != nullptr);
	Player &myPlayer = *MyPlayer;

	if (!IsValidSpell(spellID)) {
		myPlayer.Say(HeroSpeech::IDontHaveASpellReady);
		return;
	}

	if (ControlMode == ControlTypes::KeyboardAndMouse) {
		if (pcurs != CURSOR_HAND)
			return;

		if (GetMainPanel().contains(MousePosition)) // inside main panel
			return;

		if (
		    (IsLeftPanelOpen() && GetLeftPanel().contains(MousePosition))      // inside left panel
		    || (IsRightPanelOpen() && GetRightPanel().contains(MousePosition)) // inside right panel
		) {
			if (spellID != SpellID::Healing
			    && spellID != SpellID::Identify
			    && spellID != SpellID::ItemRepair
			    && spellID != SpellID::Infravision
			    && spellID != SpellID::StaffRecharge)
				return;
		}
	}

	if (leveltype == DTYPE_TOWN && !GetSpellData(spellID).isAllowedInTown()) {
		myPlayer.Say(HeroSpeech::ICantCastThatHere);
		return;
	}

	SpellCheckResult spellcheck = SpellCheckResult::Success;
	switch (spellType) {
	case SpellType::Skill:
	case SpellType::Spell:
		spellcheck = CheckSpell(*MyPlayer, spellID, spellType, false);
		addflag = spellcheck == SpellCheckResult::Success;
		break;
	case SpellType::Scroll:
		addflag = pcurs == CURSOR_HAND && CanUseScroll(myPlayer, spellID);
		break;
	case SpellType::Charges:
		addflag = pcurs == CURSOR_HAND && CanUseStaff(myPlayer, spellID);
		break;
	case SpellType::Invalid:
		return;
	}

	if (!addflag) {
		if (spellType == SpellType::Spell) {
			switch (spellcheck) {
			case SpellCheckResult::Fail_NoMana:
				myPlayer.Say(HeroSpeech::NotEnoughMana);
				break;
			case SpellCheckResult::Fail_Level0:
				myPlayer.Say(HeroSpeech::ICantCastThatYet);
				break;
			default:
				myPlayer.Say(HeroSpeech::ICantDoThat);
				break;
			}
			LastMouseButtonAction = MouseActionType::None;
		}
		return;
	}

	const int spellFrom = 0;
	if (IsWallSpell(spellID)) {
		LastMouseButtonAction = MouseActionType::Spell;
		Direction sd = GetDirection(myPlayer.position.tile, cursPosition);
		NetSendCmdLocParam4(true, CMD_SPELLXYD, cursPosition, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), static_cast<uint16_t>(sd), spellFrom);
	} else if (pcursmonst != -1 && !isShiftHeld) {
		LastMouseButtonAction = MouseActionType::SpellMonsterTarget;
		NetSendCmdParam4(true, CMD_SPELLID, pcursmonst, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
	} else if (PlayerUnderCursor != nullptr && !isShiftHeld && !myPlayer.friendlyMode) {
		LastMouseButtonAction = MouseActionType::SpellPlayerTarget;
		NetSendCmdParam4(true, CMD_SPELLPID, PlayerUnderCursor->getId(), static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
	} else {
		Point targetedTile = cursPosition;
		if (spellID == SpellID::Teleport && myPlayer.executedSpell.spellId == SpellID::Teleport) {
			// Check if the player is attempting to queue Teleport onto a tile that is currently being targeted with Teleport, or a nearby tile
			if (cursPosition.WalkingDistance(myPlayer.position.temp) <= 1) {
				// Get the relative displacement from the player's current position to the cursor position
				WorldTileDisplacement relativeMove = cursPosition - static_cast<Point>(myPlayer.position.tile);
				// Target the tile the relative distance away from the player's targeted Teleport tile
				targetedTile = myPlayer.position.temp + relativeMove;
			}
		}
		LastMouseButtonAction = MouseActionType::Spell;
		NetSendCmdLocParam3(true, CMD_SPELLXY, targetedTile, static_cast<int8_t>(spellID), static_cast<uint8_t>(spellType), spellFrom);
	}
}

void SyncPlrAnim(Player &player)
{
	const player_graphic graphic = player.getGraphic();
	if (!HeadlessMode)
		player.AnimInfo.sprites = player.AnimationData[static_cast<size_t>(graphic)].spritesForDirection(player._pdir);
}

void SyncInitPlrPos(Player &player)
{
	if (!player.isOnActiveLevel())
		return;

	const WorldTileDisplacement offset[9] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 }, { 2, 0 }, { 0, 2 }, { 1, 2 }, { 2, 1 }, { 2, 2 } };

	Point position = [&]() {
		for (int i = 0; i < 8; i++) {
			Point position = player.position.tile + offset[i];
			if (PosOkPlayer(player, position))
				return position;
		}

		std::optional<Point> nearPosition = FindClosestValidPosition(
		    [&player](Point testPosition) {
			    for (int i = 0; i < numtrigs; i++) {
				    if (trigs[i].position == testPosition)
					    return false;
			    }
			    return PosOkPlayer(player, testPosition) && !PosOkPortal(currlevel, testPosition);
		    },
		    player.position.tile,
		    1, // skip the starting tile since that was checked in the previous loop
		    50);

		return nearPosition.value_or(Point { 0, 0 });
	}();

	player.position.tile = position;
	player.occupyTile(position, false);
	player.position.future = position;

	if (&player == MyPlayer) {
		ViewPosition = position;
	}
}

void SyncInitPlr(Player &player)
{
	SetPlrAnims(player);
	SyncInitPlrPos(player);
	if (&player != MyPlayer)
		player.lightId = NO_LIGHT;
}

void CheckStats(Player &player)
{
	for (auto attribute : enum_values<CharacterAttribute>()) {
		int maxStatPoint = player.GetMaximumAttributeValue(attribute);
		switch (attribute) {
		case CharacterAttribute::Strength:
			player._pBaseStr = std::clamp(player._pBaseStr, 0, maxStatPoint);
			break;
		case CharacterAttribute::Magic:
			player._pBaseMag = std::clamp(player._pBaseMag, 0, maxStatPoint);
			break;
		case CharacterAttribute::Dexterity:
			player._pBaseDex = std::clamp(player._pBaseDex, 0, maxStatPoint);
			break;
		case CharacterAttribute::Vitality:
			player._pBaseVit = std::clamp(player._pBaseVit, 0, maxStatPoint);
			break;
		}
	}
}

void ModifyPlrStr(Player &player, int l)
{
	l = std::clamp(l, 0 - player._pBaseStr, player.GetMaximumAttributeValue(CharacterAttribute::Strength) - player._pBaseStr);

	player._pStrength += l;
	player._pBaseStr += l;

	CalcPlrInv(player, true);

	if (&player == MyPlayer) {
		NetSendCmdParam1(false, CMD_SETSTR, player._pBaseStr);
	}
}

void ModifyPlrMag(Player &player, int l)
{
	l = std::clamp(l, 0 - player._pBaseMag, player.GetMaximumAttributeValue(CharacterAttribute::Magic) - player._pBaseMag);

	player._pMagic += l;
	player._pBaseMag += l;

	int ms = l;
	ms *= player.getClassAttributes().chrMana;

	player._pMaxManaBase += ms;
	player._pMaxMana += ms;
	if (HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		player._pManaBase += ms;
		player._pMana += ms;
	}

	CalcPlrInv(player, true);

	if (&player == MyPlayer) {
		NetSendCmdParam1(false, CMD_SETMAG, player._pBaseMag);
	}
}

void ModifyPlrDex(Player &player, int l)
{
	l = std::clamp(l, 0 - player._pBaseDex, player.GetMaximumAttributeValue(CharacterAttribute::Dexterity) - player._pBaseDex);

	player._pDexterity += l;
	player._pBaseDex += l;
	CalcPlrInv(player, true);

	if (&player == MyPlayer) {
		NetSendCmdParam1(false, CMD_SETDEX, player._pBaseDex);
	}
}

void ModifyPlrVit(Player &player, int l)
{
	l = std::clamp(l, 0 - player._pBaseVit, player.GetMaximumAttributeValue(CharacterAttribute::Vitality) - player._pBaseVit);

	player._pVitality += l;
	player._pBaseVit += l;

	int ms = l;
	ms *= player.getClassAttributes().chrLife;

	player._pHPBase += ms;
	player._pMaxHPBase += ms;
	player._pHitPoints += ms;
	player._pMaxHP += ms;

	CalcPlrInv(player, true);

	if (&player == MyPlayer) {
		NetSendCmdParam1(false, CMD_SETVIT, player._pBaseVit);
	}
}

void SetPlayerHitPoints(Player &player, int val)
{
	player._pHitPoints = val;
	player._pHPBase = val + player._pMaxHPBase - player._pMaxHP;

	if (&player == MyPlayer) {
		RedrawComponent(PanelDrawComponent::Health);
	}
}

void SetPlrStr(Player &player, int v)
{
	player._pBaseStr = v;
	CalcPlrInv(player, true);
}

void SetPlrMag(Player &player, int v)
{
	player._pBaseMag = v;

	int m = v;
	m *= player.getClassAttributes().chrMana;

	player._pMaxManaBase = m;
	player._pMaxMana = m;
	CalcPlrInv(player, true);
}

void SetPlrDex(Player &player, int v)
{
	player._pBaseDex = v;
	CalcPlrInv(player, true);
}

void SetPlrVit(Player &player, int v)
{
	player._pBaseVit = v;

	int hp = v;
	hp *= player.getClassAttributes().chrLife;

	player._pHPBase = hp;
	player._pMaxHPBase = hp;
	CalcPlrInv(player, true);
}

void InitDungMsgs(Player &player)
{
	player.pDungMsgs = 0;
	player.pDungMsgs2 = 0;
}

enum {
	// clang-format off
	DungMsgCathedral = 1 << 0,
	DungMsgCatacombs = 1 << 1,
	DungMsgCaves     = 1 << 2,
	DungMsgHell      = 1 << 3,
	DungMsgDiablo    = 1 << 4,
	// clang-format on
};

void PlayDungMsgs()
{
	assert(MyPlayer != nullptr);
	Player &myPlayer = *MyPlayer;

	if (!setlevel && currlevel == 1 && !myPlayer._pLvlVisited[1] && (myPlayer.pDungMsgs & DungMsgCathedral) == 0) {
		myPlayer.Say(HeroSpeech::TheSanctityOfThisPlaceHasBeenFouled, 40);
		myPlayer.pDungMsgs = myPlayer.pDungMsgs | DungMsgCathedral;
	} else if (!setlevel && currlevel == 5 && !myPlayer._pLvlVisited[5] && (myPlayer.pDungMsgs & DungMsgCatacombs) == 0) {
		myPlayer.Say(HeroSpeech::TheSmellOfDeathSurroundsMe, 40);
		myPlayer.pDungMsgs |= DungMsgCatacombs;
	} else if (!setlevel && currlevel == 9 && !myPlayer._pLvlVisited[9] && (myPlayer.pDungMsgs & DungMsgCaves) == 0) {
		myPlayer.Say(HeroSpeech::ItsHotDownHere, 40);
		myPlayer.pDungMsgs |= DungMsgCaves;
	} else if (!setlevel && currlevel == 13 && !myPlayer._pLvlVisited[13] && (myPlayer.pDungMsgs & DungMsgHell) == 0) {
		myPlayer.Say(HeroSpeech::IMustBeGettingClose, 40);
		myPlayer.pDungMsgs |= DungMsgHell;
	} else if (!setlevel && currlevel == 16 && !myPlayer._pLvlVisited[16] && (myPlayer.pDungMsgs & DungMsgDiablo) == 0) {
		for (auto &monster : Monsters) {
			if (monster.type().type != MT_DIABLO) continue;
			if (monster.hitPoints > 0) {
				sfxdelay = 40;
				sfxdnum = SfxID::DiabloGreeting;
				myPlayer.pDungMsgs |= DungMsgDiablo;
			}
			break;
		}
	} else if (!setlevel && currlevel == 17 && !myPlayer._pLvlVisited[17] && (myPlayer.pDungMsgs2 & 1) == 0) {
		sfxdelay = 10;
		sfxdnum = SfxID::Defiler1;
		Quests[Q_DEFILER]._qactive = QUEST_ACTIVE;
		Quests[Q_DEFILER]._qlog = true;
		Quests[Q_DEFILER]._qmsg = TEXT_DEFILER1;
		NetSendCmdQuest(true, Quests[Q_DEFILER]);
		myPlayer.pDungMsgs2 |= 1;
	} else if (!setlevel && currlevel == 19 && !myPlayer._pLvlVisited[19] && (myPlayer.pDungMsgs2 & 4) == 0) {
		sfxdelay = 10;
		sfxdnum = SfxID::Defiler3;
		myPlayer.pDungMsgs2 |= 4;
	} else if (!setlevel && currlevel == 21 && !myPlayer._pLvlVisited[21] && (myPlayer.pDungMsgs & 32) == 0) {
		myPlayer.Say(HeroSpeech::ThisIsAPlaceOfGreatPower, 30);
		myPlayer.pDungMsgs |= 32;
	} else if (setlevel && setlvlnum == SL_SKELKING && !gbIsSpawn && !myPlayer._pSLvlVisited[SL_SKELKING] && Quests[Q_SKELKING]._qactive == QUEST_ACTIVE) {
		sfxdelay = 10;
		sfxdnum = SfxID::LeoricGreeting;
	} else {
		sfxdelay = 0;
	}
}

#ifdef BUILD_TESTING
bool TestPlayerDoGotHit(Player &player)
{
	return DoGotHit(player);
}
#endif

} // namespace devilution
