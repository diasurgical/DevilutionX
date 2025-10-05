/**
 * @file sync.cpp
 *
 * Implementation of functionality for syncing game state with other players.
 */
#include <cstdint>

#include <limits>

#include "levels/gendung.h"
#include "lighting.h"
#include "monster.h"
#include "monsters/validation.hpp"
#include "player.h"
#include "utils/endian_swap.hpp"
#include "utils/is_of.hpp"

namespace devilution {

namespace {

uint16_t sgnMonsterPriority[MaxMonsters];
size_t sgnMonsters;
uint16_t sgwLRU[MaxMonsters];
int sgnSyncItem;
int sgnSyncPInv;

void SyncOneMonster()
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const unsigned m = ActiveMonsters[i];
		const Monster &monster = Monsters[m];
		sgnMonsterPriority[m] = MyPlayer->position.tile.ManhattanDistance(monster.position.tile);
		if (monster.activeForTicks == 0) {
			sgnMonsterPriority[m] += 0x1000;
		} else if (sgwLRU[m] != 0) {
			sgwLRU[m]--;
		}
	}
}

void SyncMonsterPos(TSyncMonster &monsterSync, int ndx)
{
	Monster &monster = Monsters[ndx];
	monsterSync._mndx = ndx;
	monsterSync._mx = monster.position.tile.x;
	monsterSync._my = monster.position.tile.y;
	monsterSync._menemy = encode_enemy(monster);
	monsterSync._mdelta = sgnMonsterPriority[ndx] > 255 ? 255 : sgnMonsterPriority[ndx];
	monsterSync.mWhoHit = monster.whoHit;
	monsterSync._mhitpoints = Swap32LE(monster.hitPoints);

	sgnMonsterPriority[ndx] = 0xFFFF;
	sgwLRU[ndx] = monster.activeForTicks == 0 ? 0xFFFF : 0xFFFE;
}

bool SyncMonsterActive(TSyncMonster &monsterSync)
{
	unsigned ndx = std::numeric_limits<unsigned>::max();
	uint32_t lru = 0xFFFFFFFF;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const unsigned m = ActiveMonsters[i];
		if (sgnMonsterPriority[m] < lru && sgwLRU[m] < 0xFFFE) {
			lru = sgnMonsterPriority[m];
			ndx = ActiveMonsters[i];
		}
	}

	if (ndx == std::numeric_limits<unsigned>::max()) {
		return false;
	}

	SyncMonsterPos(monsterSync, static_cast<int>(ndx));
	return true;
}

bool SyncMonsterActive2(TSyncMonster &monsterSync)
{
	unsigned ndx = std::numeric_limits<unsigned>::max();
	uint32_t lru = 0xFFFE;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		if (sgnMonsters >= ActiveMonsterCount) {
			sgnMonsters = 0;
		}
		const unsigned m = ActiveMonsters[sgnMonsters];
		if (sgwLRU[m] < lru) {
			lru = sgwLRU[m];
			ndx = ActiveMonsters[sgnMonsters];
		}
		sgnMonsters++;
	}

	if (ndx == std::numeric_limits<unsigned>::max()) {
		return false;
	}

	SyncMonsterPos(monsterSync, static_cast<int>(ndx));
	return true;
}

void SyncPlrInv(TSyncHeader *pHdr)
{
	pHdr->bItemI = -1;
	if (ActiveItemCount > 0) {
		if (sgnSyncItem >= ActiveItemCount) {
			sgnSyncItem = 0;
		}
		pHdr->bItemI = ActiveItems[sgnSyncItem];
		sgnSyncItem++;
		auto &item = Items[pHdr->bItemI];
		pHdr->bItemX = item.position.x;
		pHdr->bItemY = item.position.y;
		pHdr->wItemIndx = Swap16LE(item.IDidx);
		if (item.IDidx == IDI_EAR) {
			pHdr->wItemCI = Swap16LE((item._iIName[0] << 8) | item._iIName[1]);
			pHdr->dwItemSeed = Swap32LE((item._iIName[2] << 24) | (item._iIName[3] << 16) | (item._iIName[4] << 8) | item._iIName[5]);
			pHdr->bItemId = item._iIName[6];
			pHdr->bItemDur = item._iIName[7];
			pHdr->bItemMDur = item._iIName[8];
			pHdr->bItemCh = item._iIName[9];
			pHdr->bItemMCh = item._iIName[10];
			pHdr->wItemVal = Swap16LE((item._iIName[11] << 8) | ((item._iCurs - ICURS_EAR_SORCERER) << 6) | item._ivalue);
			pHdr->dwItemBuff = Swap32LE((item._iIName[12] << 24) | (item._iIName[13] << 16) | (item._iIName[14] << 8) | item._iIName[15]);
		} else {
			pHdr->wItemCI = Swap16LE(item._iCreateInfo);
			pHdr->dwItemSeed = Swap32LE(item._iSeed);
			pHdr->bItemId = item._iIdentified ? 1 : 0;
			pHdr->bItemDur = item._iDurability;
			pHdr->bItemMDur = item._iMaxDur;
			pHdr->bItemCh = item._iCharges;
			pHdr->bItemMCh = item._iMaxCharges;
			if (item.IDidx == IDI_GOLD) {
				pHdr->wItemVal = Swap16LE(item._ivalue);
			}
		}
	}

	pHdr->bPInvLoc = -1;
	assert(sgnSyncPInv > -1 && sgnSyncPInv < NUM_INVLOC);
	const auto &item = MyPlayer->InvBody[sgnSyncPInv];
	if (!item.isEmpty()) {
		pHdr->bPInvLoc = sgnSyncPInv;
		pHdr->wPInvIndx = Swap16LE(item.IDidx);
		pHdr->wPInvCI = Swap16LE(item._iCreateInfo);
		pHdr->dwPInvSeed = Swap32LE(item._iSeed);
		pHdr->bPInvId = item._iIdentified ? 1 : 0;
	}

	sgnSyncPInv++;
	if (sgnSyncPInv >= NUM_INVLOC) {
		sgnSyncPInv = 0;
	}
}

void SyncMonster(bool isOwner, const TSyncMonster &monsterSync)
{
	Monster &monster = Monsters[monsterSync._mndx];
	if (monster.hitPoints <= 0 || monster.mode == MonsterMode::Death) {
		return;
	}

	const Point position { monsterSync._mx, monsterSync._my };
	const uint8_t enemyId = monsterSync._menemy;

	if (monster.activeForTicks != 0) {
		uint32_t delta = MyPlayer->position.tile.ManhattanDistance(monster.position.tile);
		if (delta > 255) {
			delta = 255;
		}

		if (delta < monsterSync._mdelta || (delta == monsterSync._mdelta && isOwner)) {
			return;
		}
		if (monster.position.future == position) {
			return;
		}
	}
	if (IsAnyOf(monster.mode, MonsterMode::Charge, MonsterMode::Petrified)) {
		return;
	}

	if (monster.position.tile.WalkingDistance(position) <= 2) {
		if (!monster.isWalking()) {
			const Direction md = GetDirection(monster.position.tile, position);
			if (DirOK(monster, md)) {
				M_ClearSquares(monster);
				monster.occupyTile(monster.position.tile, false);
				Walk(monster, md);
				monster.activeForTicks = std::numeric_limits<uint8_t>::max();
			}
		}
	} else if (dMonster[position.x][position.y] == 0) {
		M_ClearSquares(monster);
		monster.occupyTile(position, false);
		monster.position.tile = position;
		if (monster.lightId != NO_LIGHT)
			ChangeLightXY(monster.lightId, position);
		decode_enemy(monster, enemyId);
		const Direction md = GetDirection(position, monster.enemyPosition);
		M_StartStand(monster, md);
		monster.activeForTicks = std::numeric_limits<uint8_t>::max();
	}

	decode_enemy(monster, enemyId);
	monster.whoHit = static_cast<int8_t>(monster.whoHit | monsterSync.mWhoHit);
}

bool IsTSyncMonsterValid(const TSyncMonster &monsterSync)
{
	const size_t monsterId = monsterSync._mndx;

	if (monsterId >= MaxMonsters)
		return false;

	if (!InDungeonBounds({ monsterSync._mx, monsterSync._my }))
		return false;

	if (!IsEnemyIdValid(monsterSync._menemy))
		return false;

	return true;
}

bool IsTSyncEnemyValid(const TSyncMonster &monsterSync)
{
	return IsEnemyValid(monsterSync._mndx, monsterSync._menemy);
}

} // namespace

size_t sync_all_monsters(std::byte *pbBuf, size_t dwMaxLen)
{
	if (ActiveMonsterCount < 1) {
		return dwMaxLen;
	}
	if (dwMaxLen < sizeof(TSyncHeader) + sizeof(TSyncMonster)) {
		return dwMaxLen;
	}
	if (MyPlayer->_pLvlChanging) {
		return dwMaxLen;
	}

	auto *pHdr = reinterpret_cast<TSyncHeader *>(pbBuf);
	pbBuf += sizeof(TSyncHeader);
	dwMaxLen -= sizeof(TSyncHeader);

	pHdr->bCmd = CMD_SYNCDATA;
	pHdr->bLevel = GetLevelForMultiplayer(*MyPlayer);
	pHdr->wLen = 0;
	SyncPlrInv(pHdr);
	assert(dwMaxLen <= 0xffff);
	SyncOneMonster();

	for (size_t i = 0; i < ActiveMonsterCount && dwMaxLen >= sizeof(TSyncMonster); i++) {
		auto &monsterSync = *reinterpret_cast<TSyncMonster *>(pbBuf);
		bool sync = false;
		if (i < 2) {
			sync = SyncMonsterActive2(monsterSync);
		}
		if (!sync) {
			sync = SyncMonsterActive(monsterSync);
		}
		if (!sync) {
			break;
		}
		pbBuf += sizeof(TSyncMonster);
		pHdr->wLen += sizeof(TSyncMonster);
		dwMaxLen -= sizeof(TSyncMonster);
	}
	pHdr->wLen = Swap16LE(pHdr->wLen);

	return dwMaxLen;
}

size_t OnSyncData(const TSyncHeader &header, size_t maxCmdSize, const Player &player)
{
	const uint16_t wLen = Swap16LE(header.wLen);

	if (!ValidateCmdSize(wLen + sizeof(header), maxCmdSize, player.getId()))
		return maxCmdSize;

	assert(gbBufferMsgs != 2);

	if (gbBufferMsgs == 1) {
		return wLen + sizeof(header);
	}
	if (&player == MyPlayer) {
		return wLen + sizeof(header);
	}

	assert(header.wLen % sizeof(TSyncMonster) == 0);
	const int monsterCount = static_cast<int>(wLen / sizeof(TSyncMonster));

	const uint8_t level = header.bLevel;
	const bool syncLocalLevel = !MyPlayer->_pLvlChanging && GetLevelForMultiplayer(*MyPlayer) == level;

	if (IsValidLevelForMultiplayer(level)) {
		const auto *monsterSyncs = reinterpret_cast<const TSyncMonster *>(&header + 1);
		const bool isOwner = player.getId() > MyPlayerId;

		for (int i = 0; i < monsterCount; i++) {
			if (!IsTSyncMonsterValid(monsterSyncs[i]))
				continue;

			if (syncLocalLevel) {
				if (!IsTSyncEnemyValid(monsterSyncs[i]))
					continue;
				SyncMonster(isOwner, monsterSyncs[i]);
			}

			delta_sync_monster(monsterSyncs[i], level);
		}
	}

	return wLen + sizeof(header);
}

void sync_init()
{
	sgnMonsters = static_cast<size_t>(16 * MyPlayerId);
	memset(sgwLRU, 255, sizeof(sgwLRU));
}

} // namespace devilution
