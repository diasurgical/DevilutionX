#include "controls/plrctrls.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <list>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#endif

#include "automap.h"
#include "control.h"
#include "controls/controller_motion.h"
#ifndef USE_SDL1
#include "controls/devices/game_controller.h"
#endif
#include "controls/control_mode.hpp"
#include "controls/game_controls.h"
#include "controls/touch/gamepad.h"
#include "cursor.h"
#include "doom.h"
#include "engine/point.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "gmenu.h"
#include "help.h"
#include "hwcursor.hpp"
#include "inv.h"
#include "items.h"
#include "levels/tile_properties.hpp"
#include "levels/town.h"
#include "levels/trigs.h"
#include "minitext.h"
#include "missiles.h"
#include "panels/spell_icons.hpp"
#include "panels/spell_list.hpp"
#include "panels/ui_panels.hpp"
#include "qol/chatlog.h"
#include "qol/stash.h"
#include "stores.h"
#include "towners.h"
#include "track.h"
#include "utils/is_of.hpp"
#include "utils/log.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

GameActionType ControllerActionHeld = GameActionType_NONE;

bool StandToggle = false;

int pcurstrig = -1;
Missile *pcursmissile = nullptr;
quest_id pcursquest = Q_INVALID;

/**
 * Native game menu, controlled by simulating a keyboard.
 */
bool InGameMenu()
{
	return IsPlayerInStore()
	    || HelpFlag
	    || ChatLogFlag
	    || ChatFlag
	    || qtextflag
	    || gmenu_is_active()
	    || PauseMode == 2
	    || (MyPlayer != nullptr && MyPlayer->_pInvincible && MyPlayer->_pHitPoints == 0);
}

namespace {

int Slot = SLOTXY_INV_FIRST;
Point ActiveStashSlot = InvalidStashPoint;
int PreviousInventoryColumn = -1;
bool BeltReturnsToStash = false;

const Direction FaceDir[3][3] = {
	// NONE             UP                DOWN
	{ Direction::South, Direction::North, Direction::South },        // NONE
	{ Direction::West, Direction::NorthWest, Direction::SouthWest }, // LEFT
	{ Direction::East, Direction::NorthEast, Direction::SouthEast }, // RIGHT
};

/**
 * Number of angles to turn to face the coordinate
 * @param destination Tile coordinates
 * @return -1 == down
 */
int GetRotaryDistance(Point destination)
{
	Player &myPlayer = *MyPlayer;

	if (myPlayer.position.future == destination)
		return -1;

	int d1 = static_cast<int>(myPlayer._pdir);
	int d2 = static_cast<int>(GetDirection(myPlayer.position.future, destination));

	int d = std::abs(d1 - d2);
	if (d > 4)
		return 4 - (d % 4);

	return d;
}

/**
 * @brief Get the best case walking steps to coordinates
 * @param position Tile coordinates
 */
int GetMinDistance(Point position)
{
	return MyPlayer->position.future.WalkingDistance(position);
}

/**
 * @brief Get walking steps to coordinate
 * @param destination Tile coordinates
 * @param maxDistance the max number of steps to search
 * @return number of steps, or 0 if not reachable
 */
int GetDistance(Point destination, int maxDistance)
{
	if (GetMinDistance(destination) > maxDistance) {
		return 0;
	}

	int8_t walkpath[MaxPathLengthPlayer];
	Player &myPlayer = *MyPlayer;
	int steps = FindPath(CanStep, [&myPlayer](Point position) { return PosOkPlayer(myPlayer, position); }, myPlayer.position.future, destination, walkpath, std::min<size_t>(maxDistance, MaxPathLengthPlayer));
	if (steps > maxDistance)
		return 0;

	return steps;
}

/**
 * @brief Get distance to coordinate
 * @param destination Tile coordinates
 */
int GetDistanceRanged(Point destination)
{
	return MyPlayer->position.future.ExactDistance(destination);
}

void FindItemOrObject()
{
	WorldTilePosition futurePosition = MyPlayer->position.future;
	int rotations = 5;

	auto searchArea = PointsInRectangleColMajor(WorldTileRectangle { futurePosition, 1 });

	for (WorldTilePosition targetPosition : searchArea) {
		// As the player can not stand on the edge of the map this is safe from OOB
		int8_t itemId = dItem[targetPosition.x][targetPosition.y] - 1;
		if (itemId < 0) {
			// there shouldn't be any items that occupy multiple ground tiles, but just in case only considering positive indexes here

			continue;
		}
		Item &item = Items[itemId];
		if (item.isEmpty() || item.selectionRegion == SelectionRegion::None) {
			continue;
		}

		int newRotations = GetRotaryDistance(targetPosition);
		if (rotations < newRotations) {
			continue;
		}
		if (targetPosition != futurePosition && GetDistance(targetPosition, 1) == 0) {
			// Don't check the tile we're leaving if the player is walking
			continue;
		}
		rotations = newRotations;
		pcursitem = itemId;
		cursPosition = targetPosition;
	}

	if (leveltype == DTYPE_TOWN || pcursitem != -1) {
		return; // Don't look for objects in town
	}

	for (WorldTilePosition targetPosition : searchArea) {
		Object *object = FindObjectAtPosition(targetPosition);
		if (object == nullptr || !object->canInteractWith()) {
			// No object or non-interactive object
			continue;
		}
		if (targetPosition == futurePosition && object->_oDoorFlag) {
			continue; // Ignore doorway so we don't get stuck behind barrels
		}

		int newRotations = GetRotaryDistance(targetPosition);
		if (rotations < newRotations) {
			continue;
		}
		if (targetPosition != futurePosition && GetDistance(targetPosition, 1) == 0) {
			// Don't check the tile we're leaving if the player is walking
			continue;
		}
		if (object->IsDisabled()) {
			continue;
		}

		rotations = newRotations;
		ObjectUnderCursor = object;
		cursPosition = targetPosition;
	}
}

void CheckTownersNearby()
{
	for (int i = 0; i < 16; i++) {
		int distance = GetDistance(Towners[i].position, 2);
		if (distance == 0)
			continue;
		if (!IsTownerPresent(Towners[i]._ttype))
			continue;
		pcursmonst = i;
	}
}

bool HasRangedSpell()
{
	SpellID spl = MyPlayer->_pRSpell;

	return spl != SpellID::Invalid
	    && spl != SpellID::TownPortal
	    && spl != SpellID::Teleport
	    && GetSpellData(spl).isTargeted()
	    && !GetSpellData(spl).isAllowedInTown();
}

bool CanTargetMonster(const Monster &monster)
{
	if ((monster.flags & MFLAG_HIDDEN) != 0)
		return false;
	if (monster.isPlayerMinion())
		return false;
	if (monster.hitPoints >> 6 <= 0) // dead
		return false;

	if (!IsTileLit(monster.position.tile)) // not visible
		return false;

	const int mx = monster.position.tile.x;
	const int my = monster.position.tile.y;
	if (dMonster[mx][my] == 0)
		return false;

	return true;
}

void FindRangedTarget()
{
	int rotations = 0;
	int distance = 0;
	bool canTalk = false;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		int mi = ActiveMonsters[i];
		const Monster &monster = Monsters[mi];

		if (!CanTargetMonster(monster))
			continue;

		const bool newCanTalk = CanTalkToMonst(monster);
		if (pcursmonst != -1 && !canTalk && newCanTalk)
			continue;
		const int newDdistance = GetDistanceRanged(monster.position.future);
		const int newRotations = GetRotaryDistance(monster.position.future);
		if (pcursmonst != -1 && canTalk == newCanTalk) {
			if (distance < newDdistance)
				continue;
			if (distance == newDdistance && rotations < newRotations)
				continue;
		}
		distance = newDdistance;
		rotations = newRotations;
		canTalk = newCanTalk;
		pcursmonst = mi;
	}
}

void FindMeleeTarget()
{
	bool visited[MAXDUNX][MAXDUNY] = { {} };
	int maxSteps = 25; // Max steps for FindPath is 25
	int rotations = 0;
	bool canTalk = false;

	struct SearchNode {
		int x, y;
		int steps;
	};
	std::list<SearchNode> queue;

	Player &myPlayer = *MyPlayer;

	{
		const int startX = myPlayer.position.future.x;
		const int startY = myPlayer.position.future.y;
		visited[startX][startY] = true;
		queue.push_back({ startX, startY, 0 });
	}

	while (!queue.empty()) {
		SearchNode node = queue.front();
		queue.pop_front();

		for (auto pathDir : PathDirs) {
			const int dx = node.x + pathDir.deltaX;
			const int dy = node.y + pathDir.deltaY;

			if (visited[dx][dy])
				continue; // already visisted

			if (node.steps > maxSteps) {
				visited[dx][dy] = true;
				continue;
			}

			if (!PosOkPlayer(myPlayer, { dx, dy })) {
				visited[dx][dy] = true;

				if (dMonster[dx][dy] != 0) {
					const int mi = std::abs(dMonster[dx][dy]) - 1;
					const Monster &monster = Monsters[mi];
					if (CanTargetMonster(monster)) {
						const bool newCanTalk = CanTalkToMonst(monster);
						if (pcursmonst != -1 && !canTalk && newCanTalk)
							continue;
						const int newRotations = GetRotaryDistance({ dx, dy });
						if (pcursmonst != -1 && canTalk == newCanTalk && rotations < newRotations)
							continue;
						rotations = newRotations;
						canTalk = newCanTalk;
						pcursmonst = mi;
						if (!canTalk)
							maxSteps = node.steps; // Monsters found, cap search to current steps
					}
				}

				continue;
			}

			if (CanStep({ node.x, node.y }, { dx, dy })) {
				queue.push_back({ dx, dy, node.steps + 1 });
				visited[dx][dy] = true;
			}
		}
	}
}

void CheckMonstersNearby()
{
	if (MyPlayer->UsesRangedWeapon() || HasRangedSpell()) {
		FindRangedTarget();
		return;
	}

	FindMeleeTarget();
}

void CheckPlayerNearby()
{
	int newDdistance;
	int rotations = 0;
	int distance = 0;

	if (pcursmonst != -1)
		return;

	Player &myPlayer = *MyPlayer;

	SpellID spl = myPlayer._pRSpell;
	if (myPlayer.friendlyMode && spl != SpellID::Resurrect && spl != SpellID::HealOther)
		return;

	for (const Player &player : Players) {
		if (&player == MyPlayer)
			continue;
		const int mx = player.position.future.x;
		const int my = player.position.future.y;
		if (dPlayer[mx][my] == 0
		    || !IsTileLit(player.position.future)
		    || (player._pHitPoints == 0 && spl != SpellID::Resurrect))
			continue;

		if (myPlayer.UsesRangedWeapon() || HasRangedSpell() || spl == SpellID::HealOther) {
			newDdistance = GetDistanceRanged(player.position.future);
		} else {
			newDdistance = GetDistance(player.position.future, distance);
			if (newDdistance == 0)
				continue;
		}

		if (PlayerUnderCursor != nullptr && distance < newDdistance)
			continue;
		const int newRotations = GetRotaryDistance(player.position.future);
		if (PlayerUnderCursor != nullptr && distance == newDdistance && rotations < newRotations)
			continue;

		distance = newDdistance;
		rotations = newRotations;
		PlayerUnderCursor = &player;
	}
}

void FindActor()
{
	if (leveltype != DTYPE_TOWN)
		CheckMonstersNearby();
	else
		CheckTownersNearby();

	if (gbIsMultiplayer)
		CheckPlayerNearby();
}

void FindTrigger()
{
	int rotations = 0;
	int distance = 0;

	if (pcursitem != -1 || ObjectUnderCursor != nullptr)
		return; // Prefer showing items/objects over triggers (use of cursm* conflicts)

	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::TownPortal || missile._mitype == MissileID::RedPortal) {
			const int newDistance = GetDistance(missile.position.tile, 2);
			if (newDistance == 0)
				continue;
			if (pcursmissile != nullptr && distance < newDistance)
				continue;
			const int newRotations = GetRotaryDistance(missile.position.tile);
			if (pcursmissile != nullptr && distance == newDistance && rotations < newRotations)
				continue;
			cursPosition = missile.position.tile;
			pcursmissile = &missile;
			distance = newDistance;
			rotations = newRotations;
		}
	}

	if (pcursmissile == nullptr) {
		for (int i = 0; i < numtrigs; i++) {
			int tx = trigs[i].position.x;
			int ty = trigs[i].position.y;
			if (trigs[i]._tlvl == 13)
				ty -= 1;
			const int newDistance = GetDistance({ tx, ty }, 2);
			if (newDistance == 0)
				continue;
			cursPosition = { tx, ty };
			pcurstrig = i;
		}

		if (pcurstrig == -1) {
			for (auto &quest : Quests) {
				if (quest._qidx == Q_BETRAYER || currlevel != quest._qlevel || quest._qslvl == 0)
					continue;
				const int newDistance = GetDistance(quest.position, 2);
				if (newDistance == 0)
					continue;
				cursPosition = quest.position;
				pcursquest = quest._qidx;
			}
		}
	}

	if (pcursmonst != -1 || PlayerUnderCursor != nullptr || cursPosition.x == -1 || cursPosition.y == -1)
		return; // Prefer monster/player info text

	CheckTrigForce();
	CheckTown();
	CheckRportal();
}

bool IsStandingGround()
{
	if (ControlMode == ControlTypes::Gamepad) {
		ControllerButtonCombo standGroundCombo = GetOptions().Padmapper.ButtonComboForAction("StandGround");
		return StandToggle || IsControllerButtonComboPressed(standGroundCombo);
	}
#ifndef USE_SDL1
	if (ControlMode == ControlTypes::VirtualGamepad) {
		return VirtualGamepadState.standButton.isHeld;
	}
#endif
	return false;
}

void Interact()
{
	if (leveltype == DTYPE_TOWN && pcursmonst != -1) {
		NetSendCmdLocParam1(true, CMD_TALKXY, Towners[pcursmonst].position, pcursmonst);
		return;
	}

	Player &myPlayer = *MyPlayer;

	if (leveltype != DTYPE_TOWN && IsStandingGround()) {
		Direction pdir = myPlayer._pdir;
		AxisDirection moveDir = GetMoveDirection();
		bool motion = moveDir.x != AxisDirectionX_NONE || moveDir.y != AxisDirectionY_NONE;
		if (motion) {
			pdir = FaceDir[static_cast<std::size_t>(moveDir.x)][static_cast<std::size_t>(moveDir.y)];
		}

		Point position = myPlayer.position.tile + pdir;
		if (pcursmonst != -1 && !motion) {
			position = Monsters[pcursmonst].position.tile;
		}

		NetSendCmdLoc(MyPlayerId, true, myPlayer.UsesRangedWeapon() ? CMD_RATTACKXY : CMD_SATTACKXY, position);
		LastMouseButtonAction = MouseActionType::Attack;
		return;
	}

	if (pcursmonst != -1) {
		if (!myPlayer.UsesRangedWeapon() || CanTalkToMonst(Monsters[pcursmonst])) {
			NetSendCmdParam1(true, CMD_ATTACKID, pcursmonst);
		} else {
			NetSendCmdParam1(true, CMD_RATTACKID, pcursmonst);
		}
		LastMouseButtonAction = MouseActionType::AttackMonsterTarget;
		return;
	}

	if (leveltype != DTYPE_TOWN && PlayerUnderCursor != nullptr && !myPlayer.friendlyMode) {
		NetSendCmdParam1(true, myPlayer.UsesRangedWeapon() ? CMD_RATTACKPID : CMD_ATTACKPID, PlayerUnderCursor->getId());
		LastMouseButtonAction = MouseActionType::AttackPlayerTarget;
		return;
	}

	if (ObjectUnderCursor != nullptr) {
		NetSendCmdLoc(MyPlayerId, true, CMD_OPOBJXY, cursPosition);
		LastMouseButtonAction = MouseActionType::OperateObject;
		return;
	}
}

void AttrIncBtnSnap(AxisDirection dir)
{
	static AxisDirectionRepeater repeater;
	dir = repeater.Get(dir);
	if (dir.y == AxisDirectionY_NONE)
		return;

	if (CharPanelButtonActive && MyPlayer->_pStatPts <= 0)
		return;

	// first, find our cursor location
	int slot = 0;
	Rectangle button;
	for (int i = 0; i < 4; i++) {
		button = CharPanelButtonRect[i];
		button.position = GetPanelPosition(UiPanels::Character, button.position);
		if (button.contains(MousePosition)) {
			slot = i;
			break;
		}
	}

	if (dir.y == AxisDirectionY_UP) {
		if (slot > 0)
			--slot;
	} else if (dir.y == AxisDirectionY_DOWN) {
		if (slot < 3)
			++slot;
	}

	// move cursor to our new location
	button = CharPanelButtonRect[slot];
	button.position = GetPanelPosition(UiPanels::Character, button.position);
	SetCursorPos(button.Center());
}

Point InvGetEquipSlotCoord(const inv_body_loc invSlot)
{
	Point result = GetPanelPosition(UiPanels::Inventory);
	switch (invSlot) {
	case INVLOC_HEAD:
		result.x += InvRect[SLOTXY_HEAD].Center().x;
		result.y += InvRect[SLOTXY_HEAD].Center().y;
		break;
	case INVLOC_RING_LEFT:
		result.x += InvRect[SLOTXY_RING_LEFT].Center().x;
		result.y += InvRect[SLOTXY_RING_LEFT].Center().y;
		break;
	case INVLOC_RING_RIGHT:
		result.x += InvRect[SLOTXY_RING_RIGHT].Center().x;
		result.y += InvRect[SLOTXY_RING_RIGHT].Center().y;
		break;
	case INVLOC_AMULET:
		result.x += InvRect[SLOTXY_AMULET].Center().x;
		result.y += InvRect[SLOTXY_AMULET].Center().y;
		break;
	case INVLOC_HAND_LEFT:
		result.x += InvRect[SLOTXY_HAND_LEFT].Center().x;
		result.y += InvRect[SLOTXY_HAND_LEFT].Center().y;
		break;
	case INVLOC_HAND_RIGHT:
		result.x += InvRect[SLOTXY_HAND_RIGHT].Center().x;
		result.y += InvRect[SLOTXY_HAND_RIGHT].Center().y;
		break;
	case INVLOC_CHEST:
		result.x += InvRect[SLOTXY_CHEST].Center().x;
		result.y += InvRect[SLOTXY_CHEST].Center().y;
		break;
	default:
		break;
	}

	return result;
}

Point InvGetEquipSlotCoordFromInvSlot(const inv_xy_slot slot)
{
	if (slot == SLOTXY_HEAD) {
		return InvGetEquipSlotCoord(INVLOC_HEAD);
	}
	if (slot == SLOTXY_RING_LEFT) {
		return InvGetEquipSlotCoord(INVLOC_RING_LEFT);
	}
	if (slot == SLOTXY_RING_RIGHT) {
		return InvGetEquipSlotCoord(INVLOC_RING_RIGHT);
	}
	if (slot == SLOTXY_AMULET) {
		return InvGetEquipSlotCoord(INVLOC_AMULET);
	}
	if (slot == SLOTXY_HAND_LEFT) {
		return InvGetEquipSlotCoord(INVLOC_HAND_LEFT);
	}
	if (slot == SLOTXY_HAND_RIGHT) {
		return InvGetEquipSlotCoord(INVLOC_HAND_RIGHT);
	}
	if (slot == SLOTXY_CHEST) {
		return InvGetEquipSlotCoord(INVLOC_CHEST);
	}

	return {};
}

/**
 * Get coordinates for a given slot
 */
Point GetSlotCoord(int slot)
{
	if (slot >= SLOTXY_BELT_FIRST && slot <= SLOTXY_BELT_LAST) {
		return GetPanelPosition(UiPanels::Main, InvRect[slot].Center());
	}

	return GetPanelPosition(UiPanels::Inventory, InvRect[slot].Center());
}

/**
 * Return the item id of the current slot
 */
int GetItemIdOnSlot(int slot)
{
	if (slot >= SLOTXY_INV_FIRST && slot <= SLOTXY_INV_LAST) {
		return std::abs(MyPlayer->InvGrid[slot - SLOTXY_INV_FIRST]);
	}

	return 0;
}

/**
 * Get item size (grid size) on the slot specified. Returns 1x1 if none exists.
 */
Size GetItemSizeOnSlot(int slot)
{
	if (slot >= SLOTXY_INV_FIRST && slot <= SLOTXY_INV_LAST) {
		int8_t ii = GetItemIdOnSlot(slot);
		if (ii != 0) {
			Item &item = MyPlayer->InvList[ii - 1];
			if (!item.isEmpty()) {
				return GetInventorySize(item);
			}
		}
	}

	return { 1, 1 };
}

/**
 * Get item size (grid size) on the slot specified. Returns 1x1 if none exists.
 */
Size GetItemSizeOnSlot(Point slot)
{
	if (Rectangle { { 0, 0 }, { 10, 10 } }.contains(slot)) {
		StashStruct::StashCell ii = Stash.GetItemIdAtPosition(slot);
		if (ii != StashStruct::EmptyCell) {
			Item &item = Stash.stashList[ii];
			if (!item.isEmpty()) {
				return GetInventorySize(item);
			}
		}
	}

	return { 1, 1 };
}

/**
 * Search for the first slot occupied by an item in the inventory.
 */
int FindFirstSlotOnItem(int8_t itemInvId)
{
	if (itemInvId == 0)
		return -1;
	for (int s = SLOTXY_INV_FIRST; s < SLOTXY_INV_LAST; s++) {
		if (GetItemIdOnSlot(s) == itemInvId)
			return s;
	}
	return -1;
}

Point FindFirstStashSlotOnItem(StashStruct::StashCell itemInvId)
{
	if (itemInvId == StashStruct::EmptyCell)
		return InvalidStashPoint;

	for (WorldTilePosition point : PointsInRectangle(WorldTileRectangle { { 0, 0 }, { 10, 10 } })) {
		if (Stash.GetItemIdAtPosition(point) == itemInvId)
			return point;
	}

	return InvalidStashPoint;
}

/**
 * Reset cursor position based on the current slot.
 */
void ResetInvCursorPosition()
{
	Point mousePos {};
	if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_INV_LAST) {
		auto slot = Slot;
		Size itemSize = { 1, 1 };

		if (MyPlayer->HoldItem.isEmpty()) {
			int8_t itemInvId = GetItemIdOnSlot(Slot);
			if (itemInvId != 0) {
				slot = FindFirstSlotOnItem(itemInvId);
				itemSize = GetItemSizeOnSlot(Slot);
			}
		} else {
			itemSize = GetInventorySize(MyPlayer->HoldItem);
		}

		mousePos = GetSlotCoord(slot);
		mousePos.x += ((itemSize.width - 1) * InventorySlotSizeInPixels.width) / 2;
		mousePos.y += ((itemSize.height - 1) * InventorySlotSizeInPixels.height) / 2;
	} else if (Slot >= SLOTXY_BELT_FIRST && Slot <= SLOTXY_BELT_LAST) {
		mousePos = GetSlotCoord(Slot);
	} else {
		mousePos = InvGetEquipSlotCoordFromInvSlot((inv_xy_slot)Slot);
	}

	SetCursorPos(mousePos);
}

int FindClosestInventorySlot(
    Point mousePos, const Item &heldItem,
    tl::function_ref<int(Point, int)> distanceFunction = [](Point mousePos, int slot) { return mousePos.ManhattanDistance(GetSlotCoord(slot)); })
{
	int shortestDistance = std::numeric_limits<int>::max();
	int bestSlot = 0;

	auto checkCandidateSlot = [&](int slot) {
		int distance = distanceFunction(mousePos, slot);
		if (distance < shortestDistance) {
			shortestDistance = distance;
			bestSlot = slot;
		}
	};

	if (heldItem.isEmpty()) {
		for (int i = SLOTXY_HEAD; i <= SLOTXY_CHEST; i++) {
			checkCandidateSlot(i);
		}
	} else {
		if (heldItem._itype == ItemType::Ring) {
			for (int i : { SLOTXY_RING_LEFT, SLOTXY_RING_RIGHT }) {
				checkCandidateSlot(i);
			}
		} else if (heldItem.isWeapon()) {
			checkCandidateSlot(SLOTXY_HAND_LEFT);
		} else if (heldItem.isShield()) {
			checkCandidateSlot(SLOTXY_HAND_RIGHT);
		} else if (heldItem.isHelm()) {
			checkCandidateSlot(SLOTXY_HEAD);
		} else if (heldItem.isArmor()) {
			checkCandidateSlot(SLOTXY_CHEST);
		} else if (heldItem._itype == ItemType::Amulet) {
			checkCandidateSlot(SLOTXY_AMULET);
		}
	}
	for (int i = SLOTXY_INV_FIRST; i <= SLOTXY_INV_LAST; i++) {
		checkCandidateSlot(i);
	}

	return bestSlot;
}

Point FindClosestStashSlot(Point mousePos)
{
	int shortestDistance = std::numeric_limits<int>::max();
	Point bestSlot = {};

	for (Point point : PointsInRectangle(Rectangle { { 0, 0 }, Size { 10, 10 } })) {
		int distance = mousePos.ManhattanDistance(GetStashSlotCoord(point));
		if (distance < shortestDistance) {
			shortestDistance = distance;
			bestSlot = point;
		}
	}

	return bestSlot;
}

void LiftInventoryItem()
{
	int inventorySlot = (Slot >= 0) ? Slot : FindClosestInventorySlot(MousePosition, MyPlayer->HoldItem);

	int jumpSlot = inventorySlot; // If the cursor is over an inventory slot we may need to adjust it due to pasting items of different sizes over each other
	if (inventorySlot >= SLOTXY_INV_FIRST && inventorySlot <= SLOTXY_INV_LAST) {
		const Size cursorSizeInCells = MyPlayer->HoldItem.isEmpty() ? Size { 1, 1 } : GetInventorySize(MyPlayer->HoldItem);

		// Find any item occupying a slot that is currently under the cursor
		int8_t itemUnderCursor = [](int inventorySlot, Size cursorSizeInCells) {
			if (inventorySlot < SLOTXY_INV_FIRST || inventorySlot > SLOTXY_INV_LAST)
				return 0;
			for (int x = 0; x < cursorSizeInCells.width; x++) {
				for (int y = 0; y < cursorSizeInCells.height; y++) {
					int slotUnderCursor = inventorySlot + x + y * INV_ROW_SLOT_SIZE;
					if (slotUnderCursor > SLOTXY_INV_LAST)
						continue;
					int itemId = GetItemIdOnSlot(slotUnderCursor);
					if (itemId != 0)
						return itemId;
				}
			}
			return 0;
		}(inventorySlot, cursorSizeInCells);

		// Capture the first slot of the first item (if any) under the cursor
		if (itemUnderCursor > 0)
			jumpSlot = FindFirstSlotOnItem(itemUnderCursor);
	}
	CheckInvItem();

	if (inventorySlot >= SLOTXY_INV_FIRST && inventorySlot <= SLOTXY_INV_LAST) {
		Point mousePos = GetSlotCoord(jumpSlot);
		Slot = jumpSlot;
		const Size newCursorSizeInCells = MyPlayer->HoldItem.isEmpty() ? GetItemSizeOnSlot(jumpSlot) : GetInventorySize(MyPlayer->HoldItem);
		mousePos.x += ((newCursorSizeInCells.width - 1) * InventorySlotSizeInPixels.width) / 2;
		mousePos.y += ((newCursorSizeInCells.height - 1) * InventorySlotSizeInPixels.height) / 2;
		SetCursorPos(mousePos);
	}
}

void LiftStashItem()
{
	Point stashSlot = (ActiveStashSlot != InvalidStashPoint) ? ActiveStashSlot : FindClosestStashSlot(MousePosition);

	Size cursorSizeInCells = MyPlayer->HoldItem.isEmpty() ? Size { 1, 1 } : GetInventorySize(MyPlayer->HoldItem);

	// Find any item occupying a slot that is currently under the cursor
	StashStruct::StashCell itemUnderCursor = [](Point stashSlot, Size cursorSizeInCells) -> StashStruct::StashCell {
		if (stashSlot == InvalidStashPoint)
			return StashStruct::EmptyCell;
		for (Point slotUnderCursor : PointsInRectangle(Rectangle { stashSlot, cursorSizeInCells })) {
			if (slotUnderCursor.x >= 10 || slotUnderCursor.y >= 10)
				continue;
			StashStruct::StashCell itemId = Stash.GetItemIdAtPosition(slotUnderCursor);
			if (itemId != StashStruct::EmptyCell)
				return itemId;
		}
		return StashStruct::EmptyCell;
	}(stashSlot, cursorSizeInCells);

	Point jumpSlot = itemUnderCursor == StashStruct::EmptyCell ? stashSlot : FindFirstStashSlotOnItem(itemUnderCursor);
	CheckStashItem(MousePosition);

	Point mousePos = GetStashSlotCoord(jumpSlot);
	ActiveStashSlot = jumpSlot;
	// Center the Cursor based on the item we just put down or we're holding.
	cursorSizeInCells = MyPlayer->HoldItem.isEmpty() ? GetItemSizeOnSlot(jumpSlot) : GetInventorySize(MyPlayer->HoldItem);
	mousePos.x += ((cursorSizeInCells.width) * InventorySlotSizeInPixels.width) / 2;
	mousePos.y += ((cursorSizeInCells.height) * InventorySlotSizeInPixels.height) / 2;

	SetCursorPos(mousePos);
}

/**
 * @brief Figures out where on the body to move when on the first row
 */
inv_xy_slot InventoryMoveToBody(int slot)
{
	PreviousInventoryColumn = slot - SLOTXY_INV_ROW1_FIRST;
	if (slot <= SLOTXY_INV_ROW1_FIRST + 2) { // first 3 general slots
		return SLOTXY_RING_LEFT;
	}
	if (slot <= SLOTXY_INV_ROW1_FIRST + 6) { // middle 4 general slots
		return SLOTXY_CHEST;
	}
	// last 3 general slots
	return SLOTXY_RING_RIGHT;
}

void InventoryMove(AxisDirection dir)
{
	Point mousePos = MousePosition;

	const Item &heldItem = MyPlayer->HoldItem;
	// normalize slots
	if (Slot < 0)
		Slot = FindClosestInventorySlot(mousePos, heldItem);
	else if (Slot > SLOTXY_BELT_LAST)
		Slot = SLOTXY_BELT_LAST;

	const int initialSlot = Slot;

	const bool isHoldingItem = !heldItem.isEmpty();
	Size itemSize = isHoldingItem ? GetInventorySize(heldItem) : Size { 1 };

	// when item is on cursor (pcurs > 1), this is the real cursor XY
	if (dir.x == AxisDirectionX_LEFT) {
		if (isHoldingItem) {
			if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_BELT_LAST) {
				if (IsNoneOf(Slot, SLOTXY_INV_ROW1_FIRST, SLOTXY_INV_ROW2_FIRST, SLOTXY_INV_ROW3_FIRST, SLOTXY_INV_ROW4_FIRST, SLOTXY_BELT_FIRST)) {
					Slot -= 1;
				}
			} else if (heldItem._itype == ItemType::Ring) {
				Slot = SLOTXY_RING_LEFT;
			} else if (heldItem.isWeapon() || heldItem.isShield()) {
				Slot = SLOTXY_HAND_LEFT;
			}
		} else {
			if (Slot == SLOTXY_HAND_RIGHT) {
				Slot = SLOTXY_CHEST;
			} else if (Slot == SLOTXY_CHEST) {
				Slot = SLOTXY_HAND_LEFT;
			} else if (Slot == SLOTXY_AMULET) {
				Slot = SLOTXY_HEAD;
			} else if (Slot == SLOTXY_RING_RIGHT) {
				Slot = SLOTXY_RING_LEFT;
			} else if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_BELT_LAST) {
				int8_t itemId = GetItemIdOnSlot(Slot);
				if (itemId != 0) {
					for (int i = 1; i < INV_ROW_SLOT_SIZE && !IsAnyOf(Slot - i + 1, SLOTXY_INV_ROW1_FIRST, SLOTXY_INV_ROW2_FIRST, SLOTXY_INV_ROW3_FIRST, SLOTXY_INV_ROW4_FIRST, SLOTXY_BELT_FIRST); i++) {
						if (itemId != GetItemIdOnSlot(Slot - i)) {
							Slot -= i;
							break;
						}
					}
				} else if (IsNoneOf(Slot, SLOTXY_INV_ROW1_FIRST, SLOTXY_INV_ROW2_FIRST, SLOTXY_INV_ROW3_FIRST, SLOTXY_INV_ROW4_FIRST, SLOTXY_BELT_FIRST)) {
					Slot -= 1;
				}
			}
		}
	} else if (dir.x == AxisDirectionX_RIGHT) {
		if (isHoldingItem) {
			if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_BELT_LAST) {
				if (IsNoneOf(Slot + itemSize.width - 1, SLOTXY_INV_ROW1_LAST, SLOTXY_INV_ROW2_LAST, SLOTXY_INV_ROW3_LAST, SLOTXY_INV_ROW4_LAST, SLOTXY_BELT_LAST)) {
					Slot += 1;
				}
			} else if (heldItem._itype == ItemType::Ring) {
				Slot = SLOTXY_RING_RIGHT;
			} else if (heldItem.isWeapon() || heldItem.isShield()) {
				Slot = SLOTXY_HAND_RIGHT;
			}
		} else {
			if (Slot == SLOTXY_RING_LEFT) {
				Slot = SLOTXY_RING_RIGHT;
			} else if (Slot == SLOTXY_HAND_LEFT) {
				Slot = SLOTXY_CHEST;
			} else if (Slot == SLOTXY_CHEST) {
				Slot = SLOTXY_HAND_RIGHT;
			} else if (Slot == SLOTXY_HEAD) {
				Slot = SLOTXY_AMULET;
			} else if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_BELT_LAST) {
				int8_t itemId = GetItemIdOnSlot(Slot);
				if (itemId != 0) {
					for (int i = 1; i < INV_ROW_SLOT_SIZE && !IsAnyOf(Slot + i - 1, SLOTXY_INV_ROW1_LAST, SLOTXY_INV_ROW2_LAST, SLOTXY_INV_ROW3_LAST, SLOTXY_INV_ROW4_LAST, SLOTXY_BELT_LAST); i++) {
						if (itemId != GetItemIdOnSlot(Slot + i)) {
							Slot += i;
							break;
						}
					}
				} else if (IsNoneOf(Slot, SLOTXY_INV_ROW1_LAST, SLOTXY_INV_ROW2_LAST, SLOTXY_INV_ROW3_LAST, SLOTXY_INV_ROW4_LAST, SLOTXY_BELT_LAST)) {
					Slot += 1;
				}
			}
		}
	}
	if (dir.y == AxisDirectionY_UP) {
		if (isHoldingItem) {
			if (Slot >= SLOTXY_INV_ROW2_FIRST) { // general inventory
				Slot -= INV_ROW_SLOT_SIZE;
			} else if (Slot >= SLOTXY_INV_FIRST) {
				if (heldItem._itype == ItemType::Ring) {
					if (Slot >= SLOTXY_INV_ROW1_FIRST && Slot <= SLOTXY_INV_ROW1_FIRST + (INV_ROW_SLOT_SIZE / 2) - 1) {
						Slot = SLOTXY_RING_LEFT;
					} else {
						Slot = SLOTXY_RING_RIGHT;
					}
				} else if (heldItem.isWeapon()) {
					Slot = SLOTXY_HAND_LEFT;
				} else if (heldItem.isShield()) {
					Slot = SLOTXY_HAND_RIGHT;
				} else if (heldItem.isHelm()) {
					Slot = SLOTXY_HEAD;
				} else if (heldItem.isArmor()) {
					Slot = SLOTXY_CHEST;
				} else if (heldItem._itype == ItemType::Amulet) {
					Slot = SLOTXY_AMULET;
				}
			}
		} else {
			if (Slot >= SLOTXY_INV_ROW1_FIRST && Slot <= SLOTXY_INV_ROW1_LAST) {
				Slot = InventoryMoveToBody(Slot);
			} else if (Slot == SLOTXY_CHEST || Slot == SLOTXY_HAND_LEFT) {
				Slot = SLOTXY_HEAD;
			} else if (Slot == SLOTXY_RING_LEFT) {
				Slot = SLOTXY_HAND_LEFT;
			} else if (Slot == SLOTXY_RING_RIGHT) {
				Slot = SLOTXY_HAND_RIGHT;
			} else if (Slot == SLOTXY_HAND_RIGHT) {
				Slot = SLOTXY_AMULET;
			} else if (Slot >= SLOTXY_INV_ROW2_FIRST) {
				int8_t itemId = GetItemIdOnSlot(Slot);
				if (itemId != 0) {
					for (int i = 1; i < 5; i++) {
						if (Slot - i * INV_ROW_SLOT_SIZE < SLOTXY_INV_ROW1_FIRST) {
							Slot = InventoryMoveToBody(Slot - (i - 1) * INV_ROW_SLOT_SIZE);
							break;
						}
						if (itemId != GetItemIdOnSlot(Slot - i * INV_ROW_SLOT_SIZE)) {
							Slot -= i * INV_ROW_SLOT_SIZE;
							break;
						}
					}
				} else {
					Slot -= INV_ROW_SLOT_SIZE;
				}
			}
		}
	} else if (dir.y == AxisDirectionY_DOWN) {
		if (isHoldingItem) {
			if (Slot == SLOTXY_HEAD || Slot == SLOTXY_CHEST) {
				Slot = SLOTXY_INV_ROW1_FIRST + 4;
			} else if (Slot == SLOTXY_RING_LEFT || Slot == SLOTXY_HAND_LEFT) {
				Slot = SLOTXY_INV_ROW1_FIRST + (itemSize.width > 1 ? 0 : 1);
			} else if (Slot == SLOTXY_RING_RIGHT || Slot == SLOTXY_HAND_RIGHT || Slot == SLOTXY_AMULET) {
				Slot = SLOTXY_INV_ROW1_LAST - 1;
			} else if (Slot <= (SLOTXY_INV_ROW4_LAST - (itemSize.height * INV_ROW_SLOT_SIZE))) {
				Slot += INV_ROW_SLOT_SIZE;
			} else if (Slot <= SLOTXY_INV_LAST && heldItem._itype == ItemType::Misc && itemSize == Size { 1, 1 }) { // forcing only 1x1 misc items
				if (Slot + INV_ROW_SLOT_SIZE <= SLOTXY_BELT_LAST)
					Slot += INV_ROW_SLOT_SIZE;
			}
		} else {
			if (Slot == SLOTXY_HEAD) {
				Slot = SLOTXY_CHEST;
			} else if (Slot == SLOTXY_CHEST) {
				if (PreviousInventoryColumn >= 3 && PreviousInventoryColumn <= 6)
					Slot = SLOTXY_INV_ROW1_FIRST + PreviousInventoryColumn;
				else
					Slot = SLOTXY_INV_ROW1_FIRST + (INV_ROW_SLOT_SIZE / 2);
			} else if (Slot == SLOTXY_HAND_LEFT) {
				Slot = SLOTXY_RING_LEFT;
			} else if (Slot == SLOTXY_RING_LEFT) {
				if (PreviousInventoryColumn >= 0 && PreviousInventoryColumn <= 2)
					Slot = SLOTXY_INV_ROW1_FIRST + PreviousInventoryColumn;
				else
					Slot = SLOTXY_INV_ROW1_FIRST + 1;
			} else if (Slot == SLOTXY_RING_RIGHT) {
				if (PreviousInventoryColumn >= 7 && PreviousInventoryColumn <= 9)
					Slot = SLOTXY_INV_ROW1_FIRST + PreviousInventoryColumn;
				else
					Slot = SLOTXY_INV_ROW1_LAST - 1;
			} else if (Slot == SLOTXY_AMULET) {
				Slot = SLOTXY_HAND_RIGHT;
			} else if (Slot == SLOTXY_HAND_RIGHT) {
				Slot = SLOTXY_RING_RIGHT;
			} else if (Slot <= SLOTXY_INV_LAST) {
				int8_t itemId = GetItemIdOnSlot(Slot);
				if (itemId != 0) {
					for (int i = 1; i < 5 && Slot + i * INV_ROW_SLOT_SIZE <= SLOTXY_BELT_LAST; i++) {
						if (itemId != GetItemIdOnSlot(Slot + i * INV_ROW_SLOT_SIZE)) {
							Slot += i * INV_ROW_SLOT_SIZE;
							break;
						}
					}
				} else if (Slot + INV_ROW_SLOT_SIZE <= SLOTXY_BELT_LAST) {
					Slot += INV_ROW_SLOT_SIZE;
				}
			}
		}
	}

	// no movement was made
	if (Slot == initialSlot)
		return;

	if (Slot < SLOTXY_INV_FIRST) {
		mousePos = InvGetEquipSlotCoordFromInvSlot(static_cast<inv_xy_slot>(Slot));
	} else {
		mousePos = GetSlotCoord(Slot);
	}
	// If we're in the inventory we may need to move the cursor to an area that doesn't line up with the center of a cell
	if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_INV_LAST) {
		if (!isHoldingItem) {
			// If we're not holding an item
			int8_t itemInvId = GetItemIdOnSlot(Slot);
			if (itemInvId != 0) {
				// but the cursor moved over an item
				int itemSlot = FindFirstSlotOnItem(itemInvId);
				if (itemSlot < 0)
					itemSlot = Slot;

				// then we need to offset the cursor so it shows over the center of the item
				mousePos = GetSlotCoord(itemSlot);
				itemSize = GetItemSizeOnSlot(itemSlot);
			}
		}
		// At this point itemSize is either the size of the cell/item the hand cursor is over, or the size of the item we're currently holding.
		// mousePos is the center of the top left cell of the item under the hand cursor, or the top left cell of the region that could fit the item we're holding.
		// either way we need to offset the mouse position to account for items (we're holding or hovering over) with a dimension larger than a single cell.
		mousePos.x += ((itemSize.width - 1) * InventorySlotSizeInPixels.width) / 2;
		mousePos.y += ((itemSize.height - 1) * InventorySlotSizeInPixels.height) / 2;
	}

	if (mousePos == MousePosition) {
		return; // Avoid wobbling when scaled
	}

	SetCursorPos(mousePos);
}

/**
 * Move the cursor around in the inventory
 * If mouse coords are at SLOTXY_CHEST_LAST, consider this center of equipment
 * small inventory squares are 29x29 (roughly)
 */
void CheckInventoryMove(AxisDirection dir)
{
	static AxisDirectionRepeater repeater(/*min_interval_ms=*/150);
	dir = repeater.Get(dir);
	if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE)
		return;

	InventoryMove(dir);
}

/**
 * @brief Try to clean the inventory related cursor states.
 * @return True if it is safe to close the inventory
 */
bool BlurInventory()
{
	if (!MyPlayer->HoldItem.isEmpty()) {
		if (!TryDropItem()) {
			MyPlayer->Say(HeroSpeech::WhereWouldIPutThis);
			return false;
		}
	}

	CloseInventory();
	if (pcurs > CURSOR_HAND)
		NewCursor(CURSOR_HAND);
	if (CharFlag)
		FocusOnCharInfo();

	return true;
}

void StashMove(AxisDirection dir)
{
	static AxisDirectionRepeater repeater(/*min_interval_ms=*/150);
	dir = repeater.Get(dir);
	if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE)
		return;

	Item &holdItem = MyPlayer->HoldItem;
	if (Slot < 0 && ActiveStashSlot == InvalidStashPoint) {
		int invSlot = FindClosestInventorySlot(MousePosition, holdItem);
		Point invSlotCoord = GetSlotCoord(invSlot);
		int invDistance = MousePosition.ManhattanDistance(invSlotCoord);

		Point stashSlot = FindClosestStashSlot(MousePosition);
		Point stashSlotCoord = GetStashSlotCoord(stashSlot);
		int stashDistance = MousePosition.ManhattanDistance(stashSlotCoord);

		if (invDistance < stashDistance) {
			BeltReturnsToStash = false;
			InventoryMove(dir);
			return;
		}

		ActiveStashSlot = stashSlot;
	}

	Size itemSize = holdItem.isEmpty() ? Size { 1, 1 } : GetInventorySize(holdItem);

	if (dir.y == AxisDirectionY_UP) {
		// Check if we need to jump from belt to stash
		if (BeltReturnsToStash && Slot >= SLOTXY_BELT_FIRST && Slot <= SLOTXY_BELT_LAST) {
			int beltSlot = Slot - SLOTXY_BELT_FIRST;
			InvalidateInventorySlot();
			ActiveStashSlot = { 2 + beltSlot, 10 - itemSize.height };
			dir.y = AxisDirectionY_NONE;
		}
	}

	if (dir.x == AxisDirectionX_LEFT) {
		// Check if we need to jump from general inventory to stash
		int firstSlot = Slot;
		if (Slot >= SLOTXY_INV_FIRST && Slot <= SLOTXY_INV_LAST) {
			if (MyPlayer->HoldItem.isEmpty()) {
				int8_t itemId = GetItemIdOnSlot(Slot);
				if (itemId != 0) {
					firstSlot = FindFirstSlotOnItem(itemId);
				}
			}
		}

		// If we're in the leftmost column (or hovering over an item on the left side of the inventory) or
		//  left side of the body and we're moving left we need to move into the closest stash column
		if (IsAnyOf(firstSlot, SLOTXY_HEAD, SLOTXY_HAND_LEFT, SLOTXY_RING_LEFT, SLOTXY_AMULET, SLOTXY_CHEST, SLOTXY_INV_ROW1_FIRST, SLOTXY_INV_ROW2_FIRST, SLOTXY_INV_ROW3_FIRST, SLOTXY_INV_ROW4_FIRST)) {
			Point slotCoord = GetSlotCoord(Slot);
			InvalidateInventorySlot();
			ActiveStashSlot = FindClosestStashSlot(slotCoord) - Displacement { itemSize.width - 1, 0 };
			dir.x = AxisDirectionX_NONE;
		}
	}

	if (Slot >= 0) {
		InventoryMove(dir);
		return;
	}

	if (dir.x == AxisDirectionX_LEFT) {
		if (ActiveStashSlot.x > 0) {
			const StashStruct::StashCell itemIdAtActiveStashSlot = Stash.GetItemIdAtPosition(ActiveStashSlot);
			ActiveStashSlot.x--;
			if (holdItem.isEmpty() && itemIdAtActiveStashSlot != StashStruct::EmptyCell) {
				while (ActiveStashSlot.x > 0 && itemIdAtActiveStashSlot == Stash.GetItemIdAtPosition(ActiveStashSlot)) {
					ActiveStashSlot.x--;
				}
			}
		}
	} else if (dir.x == AxisDirectionX_RIGHT) {
		// If we're empty-handed and trying to move right while hovering over an item we may not
		//  have a free stash column to move to. If the item we're hovering over occupies the last
		//  column then we want to jump to the inventory instead of just moving one column over.
		Size itemUnderCursorSize = holdItem.isEmpty() ? GetItemSizeOnSlot(ActiveStashSlot) : itemSize;
		if (ActiveStashSlot.x < 10 - itemUnderCursorSize.width) {
			const StashStruct::StashCell itemIdAtActiveStashSlot = Stash.GetItemIdAtPosition(ActiveStashSlot);
			ActiveStashSlot.x++;
			if (holdItem.isEmpty() && itemIdAtActiveStashSlot != StashStruct::EmptyCell) {
				while (ActiveStashSlot.x < 10 - itemSize.width && itemIdAtActiveStashSlot == Stash.GetItemIdAtPosition(ActiveStashSlot)) {
					ActiveStashSlot.x++;
				}
			}
		} else {
			Point stashSlotCoord = GetStashSlotCoord(ActiveStashSlot);
			Point rightPanelCoord = { GetRightPanel().position.x, stashSlotCoord.y };
			Slot = FindClosestInventorySlot(rightPanelCoord, holdItem, [](Point mousePos, int slot) {
				Point slotPos = GetSlotCoord(slot);
				// Exaggerate the vertical difference so that moving from the top 6 rows of the
				//  stash is more likely to land on a body slot. The value 3 was found by trial and
				//  error, this allows moving from the top row of the stash to the head while
				//  empty-handed while 4 causes the amulet to be preferenced (due to less vertical
				//  distance) and 2 causes the left hand to be preferenced (due to less horizontal
				//  distance).
				return std::abs(mousePos.y - slotPos.y) * 3 + std::abs(mousePos.x - slotPos.x);
			});
			ActiveStashSlot = InvalidStashPoint;
			BeltReturnsToStash = false;
		}
	}
	if (dir.y == AxisDirectionY_UP) {
		if (ActiveStashSlot.y > 0) {
			const StashStruct::StashCell itemIdAtActiveStashSlot = Stash.GetItemIdAtPosition(ActiveStashSlot);
			ActiveStashSlot.y--;
			if (holdItem.isEmpty() && itemIdAtActiveStashSlot != StashStruct::EmptyCell) {
				while (ActiveStashSlot.y > 0 && itemIdAtActiveStashSlot == Stash.GetItemIdAtPosition(ActiveStashSlot)) {
					ActiveStashSlot.y--;
				}
			}
		}
	} else if (dir.y == AxisDirectionY_DOWN) {
		if (ActiveStashSlot.y < 10 - itemSize.height) {
			const StashStruct::StashCell itemIdAtActiveStashSlot = Stash.GetItemIdAtPosition(ActiveStashSlot);
			ActiveStashSlot.y++;
			if (holdItem.isEmpty() && itemIdAtActiveStashSlot != StashStruct::EmptyCell) {
				while (ActiveStashSlot.y < 10 - itemSize.height && itemIdAtActiveStashSlot == Stash.GetItemIdAtPosition(ActiveStashSlot)) {
					ActiveStashSlot.y++;
				}
			}
		} else if ((holdItem.isEmpty() || CanBePlacedOnBelt(*MyPlayer, holdItem)) && ActiveStashSlot.x > 1) {
			int beltSlot = ActiveStashSlot.x - 2;
			Slot = SLOTXY_BELT_FIRST + beltSlot;
			ActiveStashSlot = InvalidStashPoint;
			BeltReturnsToStash = true;
		}
	}

	if (Slot >= 0) {
		ResetInvCursorPosition();
		return;
	}

	if (ActiveStashSlot != InvalidStashPoint) {
		Point mousePos = GetStashSlotCoord(ActiveStashSlot);
		// At this point itemSize is the size of the item we're currently holding.
		// We need to offset the mouse position to account for items (we're holding or hovering over) with a dimension larger than a single cell.
		if (holdItem.isEmpty()) {
			const StashStruct::StashCell itemIdAtActiveStashSlot = Stash.GetItemIdAtPosition(ActiveStashSlot);
			if (itemIdAtActiveStashSlot != StashStruct::EmptyCell) {
				const Item stashItem = Stash.stashList[itemIdAtActiveStashSlot];
				const Point firstSlotOnItem = FindFirstStashSlotOnItem(itemIdAtActiveStashSlot);
				itemSize = GetInventorySize(stashItem);
				mousePos = GetStashSlotCoord(firstSlotOnItem);
			}
		}

		mousePos += Displacement { itemSize.width * INV_SLOT_HALF_SIZE_PX, itemSize.height * INV_SLOT_HALF_SIZE_PX };
		SetCursorPos(mousePos);
		return;
	}

	FocusOnInventory();
}

void HotSpellMove(AxisDirection dir)
{
	static AxisDirectionRepeater repeater;
	dir = repeater.Get(dir);
	if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE)
		return;

	auto spellListItems = GetSpellListItems();

	Point position = MousePosition;
	int shortestDistance = std::numeric_limits<int>::max();
	for (auto &spellListItem : spellListItems) {
		Point center = spellListItem.location + Displacement { SPLICONLENGTH / 2, -SPLICONLENGTH / 2 };
		int distance = MousePosition.ManhattanDistance(center);
		if (distance < shortestDistance) {
			position = center;
			shortestDistance = distance;
		}
	}

	const auto search = [&](AxisDirection dir, bool searchForward) {
		if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE)
			return;

		for (size_t i = 0; i < spellListItems.size(); i++) {
			const size_t index = searchForward ? spellListItems.size() - i - 1 : i;

			auto &spellListItem = spellListItems[index];
			if (spellListItem.isSelected)
				continue;

			Point center = spellListItem.location + Displacement { SPLICONLENGTH / 2, -SPLICONLENGTH / 2 };
			if (dir.x == AxisDirectionX_LEFT && center.x >= MousePosition.x)
				continue;
			if (dir.x == AxisDirectionX_RIGHT && center.x <= MousePosition.x)
				continue;
			if (dir.x == AxisDirectionX_NONE && center.x != position.x)
				continue;
			if (dir.y == AxisDirectionY_UP && center.y >= MousePosition.y)
				continue;
			if (dir.y == AxisDirectionY_DOWN && center.y <= MousePosition.y)
				continue;
			if (dir.y == AxisDirectionY_NONE && center.y != position.y)
				continue;

			position = center;
			break;
		}
	};
	search({ AxisDirectionX_NONE, dir.y }, dir.y == AxisDirectionY_DOWN);
	search({ dir.x, AxisDirectionY_NONE }, dir.x == AxisDirectionX_RIGHT);

	if (position != MousePosition) {
		SetCursorPos(position);
	}
}

void SpellBookMove(AxisDirection dir)
{
	static AxisDirectionRepeater repeater;
	dir = repeater.Get(dir);

	if (dir.x == AxisDirectionX_LEFT) {
		if (SpellbookTab > 0)
			SpellbookTab--;
	} else if (dir.x == AxisDirectionX_RIGHT) {
		if ((gbIsHellfire && SpellbookTab < 4) || (!gbIsHellfire && SpellbookTab < 3))
			SpellbookTab++;
	}
}

/**
 * @brief check if stepping in direction (dir) from position is blocked.
 *
 * If you step from A to B, at least one of the Xs need to be clear:
 *
 *  AX
 *  XB
 *
 *  @return true if step is blocked
 */
bool IsPathBlocked(Point position, Direction dir)
{
	if (IsNoneOf(dir, Direction::North, Direction::East, Direction::South, Direction::West))
		return false; // Steps along a major axis don't need to check corners

	auto leftStep { position + Left(dir) };
	auto rightStep { position + Right(dir) };

	if (IsTileNotSolid(leftStep) && IsTileNotSolid(rightStep))
		return false;

	Player &myPlayer = *MyPlayer;

	return !PosOkPlayer(myPlayer, leftStep) && !PosOkPlayer(myPlayer, rightStep);
}

void WalkInDir(Player &player, AxisDirection dir)
{
	if (dir.x == AxisDirectionX_NONE && dir.y == AxisDirectionY_NONE) {
		if (ControlMode != ControlTypes::KeyboardAndMouse && player.walkpath[0] != WALK_NONE && player.destAction == ACTION_NONE)
			NetSendCmdLoc(player.getId(), true, CMD_WALKXY, player.position.future); // Stop walking
		return;
	}

	const Direction pdir = FaceDir[static_cast<std::size_t>(dir.x)][static_cast<std::size_t>(dir.y)];
	const auto delta = player.position.future + pdir;

	if (!player.isWalking() && player.CanChangeAction())
		player._pdir = pdir;

	if (IsStandingGround()) {
		if (player._pmode == PM_STAND)
			StartStand(player, pdir);
		return;
	}

	if (PosOkPlayer(player, delta) && IsPathBlocked(player.position.future, pdir)) {
		if (player._pmode == PM_STAND)
			StartStand(player, pdir);
		return; // Don't start backtrack around obstacles
	}

	NetSendCmdLoc(player.getId(), true, CMD_WALKXY, delta);
}

void QuestLogMove(AxisDirection moveDir)
{
	static AxisDirectionRepeater repeater;
	moveDir = repeater.Get(moveDir);
	if (moveDir.y == AxisDirectionY_UP)
		QuestlogUp();
	else if (moveDir.y == AxisDirectionY_DOWN)
		QuestlogDown();
}

void StoreMove(AxisDirection moveDir)
{
	static AxisDirectionRepeater repeater;
	moveDir = repeater.Get(moveDir);
	if (moveDir.y == AxisDirectionY_UP)
		StoreUp();
	else if (moveDir.y == AxisDirectionY_DOWN)
		StoreDown();
}

using HandleLeftStickOrDPadFn = void (*)(devilution::AxisDirection);

HandleLeftStickOrDPadFn GetLeftStickOrDPadGameUIHandler()
{
	if (SpellSelectFlag) {
		return &HotSpellMove;
	}
	if (IsStashOpen) {
		return &StashMove;
	}
	if (invflag) {
		return &CheckInventoryMove;
	}
	if (CharFlag && MyPlayer->_pStatPts > 0) {
		return &AttrIncBtnSnap;
	}
	if (QuestLogIsOpen) {
		return &QuestLogMove;
	}
	if (SpellbookFlag) {
		return &SpellBookMove;
	}
	if (IsPlayerInStore()) {
		return &StoreMove;
	}
	return nullptr;
}

void ProcessLeftStickOrDPadGameUI()
{
	HandleLeftStickOrDPadFn handler = GetLeftStickOrDPadGameUIHandler();
	if (handler != nullptr)
		handler(GetLeftStickOrDpadDirection(false));
}

void Movement(Player &player)
{
	if (PadMenuNavigatorActive || PadHotspellMenuActive || InGameMenu())
		return;

	if (GetLeftStickOrDPadGameUIHandler() == nullptr) {
		WalkInDir(player, GetMoveDirection());
	}
}

struct RightStickAccumulator {

	RightStickAccumulator()
	{
		lastTc = SDL_GetTicks();
		hiresDX = 0;
		hiresDY = 0;
	}

	void Pool(int *x, int *y, int slowdown)
	{
		const Uint32 tc = SDL_GetTicks();
		const int dtc = tc - lastTc;
		hiresDX += rightStickX * dtc;
		hiresDY += rightStickY * dtc;
		const int dx = static_cast<int>(hiresDX / slowdown);
		const int dy = static_cast<int>(hiresDY / slowdown);
		*x += dx;
		*y -= dy;
		lastTc = tc;
		// keep track of remainder for sub-pixel motion
		hiresDX -= dx * slowdown;
		hiresDY -= dy * slowdown;
	}

	void Clear()
	{
		lastTc = SDL_GetTicks();
	}

	uint32_t lastTc;
	float hiresDX;
	float hiresDY;
};

bool IsStickMovementSignificant()
{
	return leftStickX >= 0.5 || leftStickX <= -0.5
	    || leftStickY >= 0.5 || leftStickY <= -0.5
	    || rightStickX != 0 || rightStickY != 0;
}

ControlTypes GetInputTypeFromEvent(const SDL_Event &event)
{
	if (IsAnyOf(event.type, SDL_KEYDOWN, SDL_KEYUP))
		return ControlTypes::KeyboardAndMouse;
#ifdef USE_SDL1
	if (IsAnyOf(event.type, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP, SDL_MOUSEMOTION))
		return ControlTypes::KeyboardAndMouse;
#else
	if (IsAnyOf(event.type, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP))
		return event.button.which == SDL_TOUCH_MOUSEID ? ControlTypes::VirtualGamepad : ControlTypes::KeyboardAndMouse;
	if (event.type == SDL_MOUSEMOTION)
		return event.motion.which == SDL_TOUCH_MOUSEID ? ControlTypes::VirtualGamepad : ControlTypes::KeyboardAndMouse;
	if (event.type == SDL_MOUSEWHEEL)
		return event.wheel.which == SDL_TOUCH_MOUSEID ? ControlTypes::VirtualGamepad : ControlTypes::KeyboardAndMouse;
	if (IsAnyOf(event.type, SDL_FINGERDOWN, SDL_FINGERUP, SDL_FINGERMOTION))
		return ControlTypes::VirtualGamepad;
	if (event.type == SDL_CONTROLLERAXISMOTION
	    && (event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT
	        || IsStickMovementSignificant()))
		return ControlTypes::Gamepad;
	if (event.type >= SDL_CONTROLLERBUTTONDOWN && event.type <= SDL_CONTROLLERDEVICEREMAPPED)
		return ControlTypes::Gamepad;
	if (IsAnyOf(event.type, SDL_JOYDEVICEADDED, SDL_JOYDEVICEREMOVED))
		return ControlTypes::Gamepad;
#endif
	if (event.type == SDL_JOYAXISMOTION && IsStickMovementSignificant())
		return ControlTypes::Gamepad;
	if (event.type >= SDL_JOYBALLMOTION && event.type <= SDL_JOYBUTTONUP)
		return ControlTypes::Gamepad;

	return ControlTypes::None;
}

float rightStickLastMove = 0;

bool ContinueSimulatedMouseEvent(const SDL_Event &event, const ControllerButtonEvent &gamepadEvent)
{
	if (!gbRunGame || AutomapActive)
		return false;

#if !defined(USE_SDL1) && !defined(JOY_AXIS_RIGHTX) && !defined(JOY_AXIS_RIGHTY)
	if (IsAnyOf(event.type, SDL_JOYAXISMOTION, SDL_JOYHATMOTION, SDL_JOYBUTTONDOWN, SDL_JOYBUTTONUP)) {
		if (!GameController::All().empty())
			return true;
	}
#endif

	if (rightStickX != 0 || rightStickY != 0 || rightStickLastMove != 0) {
		rightStickLastMove = rightStickX + rightStickY; // Allow stick to come to a rest with out breaking simulation
		return true;
	}

	return SimulatingMouseWithPadmapper || IsSimulatedMouseClickBinding(gamepadEvent);
}

std::string_view ControlTypeToString(ControlTypes controlType)
{
	switch (controlType) {
	case ControlTypes::None:
		return "None";
	case ControlTypes::KeyboardAndMouse:
		return "KeyboardAndMouse";
	case ControlTypes::Gamepad:
		return "Gamepad";
	case ControlTypes::VirtualGamepad:
		return "VirtualGamepad";
	}
	return "Invalid";
}

void LogControlDeviceAndModeChange(ControlTypes newControlDevice, ControlTypes newControlMode)
{
	if (SDL_LogGetPriority(SDL_LOG_CATEGORY_APPLICATION) > SDL_LOG_PRIORITY_VERBOSE)
		return;
	if (newControlDevice == ControlDevice && newControlMode == ControlMode)
		return;
	constexpr auto DebugChange = [](ControlTypes before, ControlTypes after) -> std::string {
		if (before == after)
			return std::string { ControlTypeToString(before) };
		return StrCat(ControlTypeToString(before), " -> ", ControlTypeToString(after));
	};
	LogVerbose("Control: device {}, mode {}", DebugChange(ControlDevice, newControlDevice), DebugChange(ControlMode, newControlMode));
}

#ifndef USE_SDL1
std::string_view GamepadTypeToString(GamepadLayout gamepadLayout)
{
	switch (gamepadLayout) {
	case GamepadLayout::Nintendo:
		return "Nintendo";
	case GamepadLayout::PlayStation:
		return "PlayStation";
	case GamepadLayout::Xbox:
		return "Xbox";
	case GamepadLayout::Generic:
		return "Unknown";
	}
	return "Invalid";
}

void LogGamepadChange(GamepadLayout newGamepad)
{
	if (SDL_LogGetPriority(SDL_LOG_CATEGORY_APPLICATION) > SDL_LOG_PRIORITY_VERBOSE)
		return;
	constexpr auto DebugChange = [](GamepadLayout before, GamepadLayout after) -> std::string {
		if (before == after)
			return std::string { GamepadTypeToString(before) };
		return StrCat(GamepadTypeToString(before), " -> ", GamepadTypeToString(after));
	};
	LogVerbose("Control: gamepad {}", DebugChange(GamepadType, newGamepad));
}
#endif

} // namespace

void DetectInputMethod(const SDL_Event &event, const ControllerButtonEvent &gamepadEvent)
{
	ControlTypes inputType = GetInputTypeFromEvent(event);

	if (inputType == ControlTypes::None)
		return;

#ifdef __vita__
	if (inputType == ControlTypes::VirtualGamepad) {
		inputType = ControlTypes::Gamepad;
	}
#endif

#if HAS_KBCTRL == 1
	if (inputType == ControlTypes::KeyboardAndMouse && gamepadEvent.button != ControllerButton_NONE) {
		inputType = ControlTypes::Gamepad;
	}
#endif

	ControlTypes newControlDevice = inputType;
	ControlTypes newControlMode = inputType;
	if (ContinueSimulatedMouseEvent(event, gamepadEvent)) {
		newControlMode = ControlMode;
	}

	LogControlDeviceAndModeChange(newControlDevice, newControlMode);

	if (newControlDevice != ControlDevice) {
		ControlDevice = newControlDevice;

#ifndef USE_SDL1
		if (ControlDevice != ControlTypes::KeyboardAndMouse) {
			if (IsHardwareCursor())
				SetHardwareCursor(CursorInfo::UnknownCursor());
		} else {
			ResetCursor();
		}
		if (ControlDevice == ControlTypes::Gamepad) {
			GamepadLayout newGamepadLayout = GameController::getLayout(event);
			if (newGamepadLayout != GamepadType) {
				LogGamepadChange(newGamepadLayout);
				GamepadType = newGamepadLayout;
			}
		}
#endif
	}

	if (newControlMode != ControlMode) {
		ControlMode = newControlMode;
		CalculatePanelAreas();
	}
}

void ProcessGameAction(const GameAction &action)
{
	switch (action.type) {
	case GameActionType_NONE:
	case GameActionType_SEND_KEY:
		break;
	case GameActionType_USE_HEALTH_POTION:
		UseBeltItem(BeltItemType::Healing);
		break;
	case GameActionType_USE_MANA_POTION:
		UseBeltItem(BeltItemType::Mana);
		break;
	case GameActionType_PRIMARY_ACTION:
		PerformPrimaryAction();
		break;
	case GameActionType_SECONDARY_ACTION:
		PerformSecondaryAction();
		break;
	case GameActionType_CAST_SPELL:
		if (!InGameMenu())
			PerformSpellAction();
		break;
	case GameActionType_TOGGLE_QUICK_SPELL_MENU:
		if (!invflag || BlurInventory()) {
			if (!SpellSelectFlag)
				DoSpeedBook();
			else
				SpellSelectFlag = false;
			CloseCharPanel();
			QuestLogIsOpen = false;
			SpellbookFlag = false;
			CloseGoldWithdraw();
			CloseStash();
		}
		break;
	case GameActionType_TOGGLE_CHARACTER_INFO:
		ToggleCharPanel();
		if (CharFlag) {
			SpellSelectFlag = false;
			if (pcurs == CURSOR_DISARM)
				NewCursor(CURSOR_HAND);
			FocusOnCharInfo();
		}
		break;
	case GameActionType_TOGGLE_QUEST_LOG:
		if (!QuestLogIsOpen) {
			StartQuestlog();
			CloseCharPanel();
			CloseGoldWithdraw();
			CloseStash();
			SpellSelectFlag = false;
		} else {
			QuestLogIsOpen = false;
		}
		break;
	case GameActionType_TOGGLE_INVENTORY:
		if (invflag) {
			BlurInventory();
		} else {
			SpellbookFlag = false;
			SpellSelectFlag = false;
			invflag = true;
			if (pcurs == CURSOR_DISARM)
				NewCursor(CURSOR_HAND);
			FocusOnInventory();
		}
		break;
	case GameActionType_TOGGLE_SPELL_BOOK:
		if (BlurInventory()) {
			CloseInventory();
			SpellSelectFlag = false;
			SpellbookFlag = !SpellbookFlag;
		}
		break;
	}
}

void HandleRightStickMotion()
{
	static RightStickAccumulator acc;
	// deadzone is handled in ScaleJoystickAxes() already
	if (rightStickX == 0 && rightStickY == 0) {
		acc.Clear();
		return;
	}

	if (AutomapActive) { // move map
		int dx = 0;
		int dy = 0;
		acc.Pool(&dx, &dy, 32);
		AutomapOffset.deltaX += dy + dx;
		AutomapOffset.deltaY += dy - dx;
		return;
	}

	{ // move cursor
		InvalidateInventorySlot();
		int x = MousePosition.x;
		int y = MousePosition.y;
		acc.Pool(&x, &y, 2);
		x = std::min(std::max(x, 0), gnScreenWidth - 1);
		y = std::min(std::max(y, 0), gnScreenHeight - 1);

		// We avoid calling `SetCursorPos` within the same SDL tick because
		// that can cause all stick motion events to arrive before all
		// cursor position events.
		static int lastMouseSetTick = 0;
		const int now = SDL_GetTicks();
		if (now - lastMouseSetTick > 0) {
			ResetCursor();
			SetCursorPos({ x, y });
			LogControlDeviceAndModeChange(ControlDevice, ControlTypes::KeyboardAndMouse);

			ControlMode = ControlTypes::KeyboardAndMouse;
			lastMouseSetTick = now;
		}
	}
}

void InvalidateInventorySlot()
{
	Slot = -1;
	ActiveStashSlot = InvalidStashPoint;
}

/**
 * @brief Moves the mouse to the first inventory slot.
 */
void FocusOnInventory()
{
	Slot = SLOTXY_INV_FIRST;
	ResetInvCursorPosition();
}

bool PointAndClickState = false;

void SetPointAndClick(bool value)
{
	PointAndClickState = value;
}

bool IsPointAndClick()
{
	return PointAndClickState;
}

bool IsMovementHandlerActive()
{
	return GetLeftStickOrDPadGameUIHandler() != nullptr;
}

void plrctrls_after_check_curs_move()
{
	// check for monsters first, then items, then towners.
	if (ControlMode == ControlTypes::KeyboardAndMouse || IsPointAndClick()) {
		return;
	}

	// While holding the button down we should retain target (but potentially lose it if it dies, goes out of view, etc)
	if (ControllerActionHeld != GameActionType_NONE && IsNoneOf(LastMouseButtonAction, MouseActionType::None, MouseActionType::Attack, MouseActionType::Spell)) {
		InvalidateTargets();

		if (pcursmonst == -1 && ObjectUnderCursor == nullptr && pcursitem == -1 && pcursinvitem == -1 && pcursstashitem == StashStruct::EmptyCell && PlayerUnderCursor == nullptr) {
			FindTrigger();
		}
		return;
	}

	// Clear focus set by cursor
	PlayerUnderCursor = nullptr;
	pcursmonst = -1;
	pcursitem = -1;
	ObjectUnderCursor = nullptr;

	pcursmissile = nullptr;
	pcurstrig = -1;
	pcursquest = Q_INVALID;
	cursPosition = { -1, -1 };
	if (MyPlayer->_pInvincible) {
		return;
	}
	if (DoomFlag) {
		return;
	}
	if (!invflag) {
		InfoString = StringOrView {};
		FindActor();
		FindItemOrObject();
		FindTrigger();
	}
}

void plrctrls_every_frame()
{
	ProcessLeftStickOrDPadGameUI();
	HandleRightStickMotion();
}

void plrctrls_after_game_logic()
{
	Movement(*MyPlayer);
}

void UseBeltItem(BeltItemType type)
{
	for (int i = 0; i < MaxBeltItems; i++) {
		Item &item = MyPlayer->SpdList[i];
		if (item.isEmpty()) {
			continue;
		}

		bool isRejuvenation = IsAnyOf(item._iMiscId, IMISC_REJUV, IMISC_FULLREJUV) || (item._iMiscId == IMISC_ARENAPOT && MyPlayer->isOnArenaLevel());
		bool isHealing = isRejuvenation || IsAnyOf(item._iMiscId, IMISC_HEAL, IMISC_FULLHEAL) || item.isScrollOf(SpellID::Healing);
		bool isMana = isRejuvenation || IsAnyOf(item._iMiscId, IMISC_MANA, IMISC_FULLMANA);

		if ((type == BeltItemType::Healing && isHealing) || (type == BeltItemType::Mana && isMana)) {
			UseInvItem(INVITEM_BELT_FIRST + i);
			break;
		}
	}
}

void PerformPrimaryAction()
{
	if (SpellSelectFlag) {
		SetSpell();
		return;
	}

	if (invflag) { // inventory is open
		if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM) {
			if (pcurs == CURSOR_HOURGLASS)
				return;
			TryIconCurs();
			NewCursor(CURSOR_HAND);
		} else if (GetRightPanel().contains(MousePosition) || GetMainPanel().contains(MousePosition)) {
			LiftInventoryItem();
		} else if (IsStashOpen && GetLeftPanel().contains(MousePosition)) {
			LiftStashItem();
		}
		return;
	}

	if (CharFlag && !CharPanelButtonActive && MyPlayer->_pStatPts > 0) {
		CheckChrBtns();
		if (CharPanelButtonActive)
			ReleaseChrBtns(false);
		return;
	}

	Interact();
}

bool SpellHasActorTarget()
{
	SpellID spl = MyPlayer->_pRSpell;
	if (spl == SpellID::TownPortal || spl == SpellID::Teleport)
		return false;

	if (IsWallSpell(spl) && pcursmonst != -1) {
		cursPosition = Monsters[pcursmonst].position.tile;
	}

	return PlayerUnderCursor != nullptr || pcursmonst != -1;
}

void UpdateSpellTarget(SpellID spell)
{
	if (SpellHasActorTarget())
		return;

	PlayerUnderCursor = nullptr;
	pcursmonst = -1;

	Player &myPlayer = *MyPlayer;

	int range = spell == SpellID::Teleport ? 4 : 1;

	cursPosition = myPlayer.position.future + Displacement(myPlayer._pdir) * range;
}

/**
 * @brief Try dropping item in all 9 possible places
 */
bool TryDropItem()
{
	Player &myPlayer = *MyPlayer;

	if (myPlayer.HoldItem.isEmpty()) {
		return false;
	}

	if (leveltype == DTYPE_TOWN) {
		if (UseItemOpensHive(myPlayer.HoldItem, myPlayer.position.tile)) {
			OpenHive();
			NewCursor(CURSOR_HAND);
			return true;
		}
		if (UseItemOpensGrave(myPlayer.HoldItem, myPlayer.position.tile)) {
			OpenGrave();
			NewCursor(CURSOR_HAND);
			return true;
		}
	}

	std::optional<Point> itemTile = FindAdjacentPositionForItem(myPlayer.position.future, myPlayer._pdir);
	if (!itemTile) {
		myPlayer.Say(HeroSpeech::WhereWouldIPutThis);
		return false;
	}

	NetSendCmdPItem(true, CMD_PUTITEM, *itemTile, myPlayer.HoldItem);
	myPlayer.HoldItem.clear();
	NewCursor(CURSOR_HAND);
	return true;
}

void PerformSpellAction()
{
	if (SpellSelectFlag) {
		SetSpell();
		return;
	}

	if (QuestLogIsOpen)
		return;

	if (invflag) {
		if (!MyPlayer->HoldItem.isEmpty())
			TryDropItem();
		else if (pcurs > CURSOR_HAND) {
			TryIconCurs();
			NewCursor(CURSOR_HAND);
		} else if (pcursinvitem != -1) {
			int itemId = GetItemIdOnSlot(Slot);
			CheckInvItem(true, false);
			if (itemId != GetItemIdOnSlot(Slot))
				ResetInvCursorPosition();
		} else if (pcursstashitem != StashStruct::EmptyCell) {
			CheckStashItem(MousePosition, true, false);
		}
		return;
	}

	if (!MyPlayer->HoldItem.isEmpty() && !TryDropItem())
		return;
	if (pcurs > CURSOR_HAND)
		NewCursor(CURSOR_HAND);

	const Player &myPlayer = *MyPlayer;
	SpellID spl = myPlayer._pRSpell;
	if ((PlayerUnderCursor == nullptr && (spl == SpellID::Resurrect || spl == SpellID::HealOther))
	    || (ObjectUnderCursor == nullptr && spl == SpellID::TrapDisarm)) {
		myPlayer.Say(HeroSpeech::ICantCastThatHere);
		return;
	}

	UpdateSpellTarget(myPlayer._pRSpell);
	CheckPlrSpell(false);
	if (PlayerUnderCursor != nullptr)
		LastMouseButtonAction = MouseActionType::SpellPlayerTarget;
	else if (pcursmonst != -1)
		LastMouseButtonAction = MouseActionType::SpellMonsterTarget;
	else
		LastMouseButtonAction = MouseActionType::Spell;
}

void CtrlUseInvItem()
{
	if (pcursinvitem == -1) {
		return;
	}

	Player &myPlayer = *MyPlayer;
	Item &item = GetInventoryItem(myPlayer, pcursinvitem);
	if (item.isScroll()) {
		if (TargetsMonster(item._iSpell)) {
			return;
		}
		if (GetSpellData(item._iSpell).isTargeted()) {
			UpdateSpellTarget(item._iSpell);
		}
	}

	int itemId = GetItemIdOnSlot(Slot);
	if (item.isEquipment()) {
		CheckInvItem(true, false); // auto-equip if it's an equipment
	} else {
		UseInvItem(pcursinvitem);
	}
	if (itemId != GetItemIdOnSlot(Slot)) {
		ResetInvCursorPosition();
	}
}

void CtrlUseStashItem()
{
	if (pcursstashitem == StashStruct::EmptyCell) {
		return;
	}

	const Item &item = Stash.stashList[pcursstashitem];
	if (item.isScroll()) {
		if (TargetsMonster(item._iSpell)) {
			return;
		}
		if (GetSpellData(item._iSpell).isTargeted()) {
			UpdateSpellTarget(item._iSpell);
		}
	}

	if (item.isEquipment()) {
		CheckStashItem(MousePosition, true, false); // Auto-equip if it's equipment
	} else {
		UseStashItem(pcursstashitem);
	}
	// Todo reset cursor position if item is moved
}

void PerformSecondaryAction()
{
	Player &myPlayer = *MyPlayer;
	if (invflag) {
		if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM) {
			TryIconCurs();
			NewCursor(CURSOR_HAND);
		} else if (IsStashOpen) {
			if (pcursstashitem != StashStruct::EmptyCell) {
				TransferItemToInventory(myPlayer, pcursstashitem);
			} else if (pcursinvitem != -1) {
				TransferItemToStash(myPlayer, pcursinvitem);
			}
		} else {
			CtrlUseInvItem();
		}
		return;
	}

	if (!MyPlayer->HoldItem.isEmpty() && !TryDropItem())
		return;
	if (pcurs > CURSOR_HAND)
		NewCursor(CURSOR_HAND);

	if (pcursitem != -1) {
		NetSendCmdLocParam1(true, CMD_GOTOAGETITEM, cursPosition, pcursitem);
	} else if (ObjectUnderCursor != nullptr) {
		NetSendCmdLoc(MyPlayerId, true, CMD_OPOBJXY, cursPosition);
		LastMouseButtonAction = MouseActionType::OperateObject;
	} else {
		if (pcursmissile != nullptr) {
			MakePlrPath(myPlayer, pcursmissile->position.tile, true);
			myPlayer.destAction = ACTION_WALK;
		} else if (pcurstrig != -1) {
			MakePlrPath(myPlayer, trigs[pcurstrig].position, true);
			myPlayer.destAction = ACTION_WALK;
		} else if (pcursquest != Q_INVALID) {
			MakePlrPath(myPlayer, Quests[pcursquest].position, true);
			myPlayer.destAction = ACTION_WALK;
		}
	}
}

void QuickCast(size_t slot)
{
	MouseActionType prevMouseButtonAction = LastMouseButtonAction;
	Player &myPlayer = *MyPlayer;
	SpellID spell = myPlayer._pSplHotKey[slot];
	SpellType spellType = myPlayer._pSplTHotKey[slot];

	if (ControlMode != ControlTypes::KeyboardAndMouse) {
		UpdateSpellTarget(spell);
	}

	CheckPlrSpell(false, spell, spellType);
	LastMouseButtonAction = prevMouseButtonAction;
}

} // namespace devilution
