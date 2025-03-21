/**
 * @file msg.cpp
 *
 * Implementation of function for sending and receiving network messages.
 */
#include <climits>
#include <cmath>
#include <cstdint>
#include <list>
#include <memory>

#include <ankerl/unordered_dense.h>
#include <fmt/format.h>

#if !defined(UNPACKED_MPQS) || !defined(UNPACKED_SAVES) || !defined(NONET)
#define USE_PKWARE
#include "encrypt.h"
#endif

#include "DiabloUI/diabloui.h"
#include "automap.h"
#include "config.h"
#include "control.h"
#include "dead.h"
#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "engine/world_tile.hpp"
#include "gamemenu.h"
#include "levels/crypt.h"
#include "levels/town.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "missiles.h"
#include "nthread.h"
#include "objects.h"
#include "options.h"
#include "pack.h"
#include "pfile.h"
#include "plrmsg.h"
#include "spells.h"
#include "storm/storm_net.hpp"
#include "sync.h"
#include "tmsg.h"
#include "towners.h"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

#define ValidateField(logValue, condition)                    \
	do {                                                      \
		if (!(condition)) {                                   \
			LogFailedPacket(#condition, #logValue, logValue); \
			EventFailedPacket(player._pName);                 \
			return false;                                     \
		}                                                     \
	} while (0)

#define ValidateFields(logValue1, logValue2, condition)                                \
	do {                                                                               \
		if (!(condition)) {                                                            \
			LogFailedPacket(#condition, #logValue1, logValue1, #logValue2, logValue2); \
			EventFailedPacket(player._pName);                                          \
			return false;                                                              \
		}                                                                              \
	} while (0)

namespace devilution {

void EventFailedPacket(const char *playerName)
{
	std::string message = fmt::format("Player '{}' sent an invalid packet.", playerName);
	EventPlrMsg(message);
}

template <typename T>
void LogFailedPacket(const char *condition, const char *name, T value)
{
	LogDebug("Remote player packet validation failed: ValidateField({}: {}, {})", name, value, condition);
}

template <typename T1, typename T2>
void LogFailedPacket(const char *condition, const char *name1, T1 value1, const char *name2, T2 value2)
{
	LogDebug("Remote player packet validation failed: ValidateFields({}: {}, {}: {}, {})", name1, value1, name2, value2, condition);
}

// #define LOG_RECEIVED_MESSAGES

uint8_t gbBufferMsgs;
int dwRecCount;

namespace {

#ifdef LOG_RECEIVED_MESSAGES
std::string_view CmdIdString(_cmd_id cmd)
{
	// clang-format off
	switch (cmd) {
	case CMD_STAND: return "CMD_STAND";
	case CMD_WALKXY: return "CMD_WALKXY";
	case CMD_ACK_PLRINFO: return "CMD_ACK_PLRINFO";
	case CMD_ADDSTR: return "CMD_ADDSTR";
	case CMD_ADDMAG: return "CMD_ADDMAG";
	case CMD_ADDDEX: return "CMD_ADDDEX";
	case CMD_ADDVIT: return "CMD_ADDVIT";
	case CMD_GETITEM: return "CMD_GETITEM";
	case CMD_AGETITEM: return "CMD_AGETITEM";
	case CMD_PUTITEM: return "CMD_PUTITEM";
	case CMD_SPAWNITEM: return "CMD_SPAWNITEM";
	case CMD_RESPAWNITEM: return "CMD_RESPAWNITEM";
	case CMD_ATTACKXY: return "CMD_ATTACKXY";
	case CMD_RATTACKXY: return "CMD_RATTACKXY";
	case CMD_SPELLXY: return "CMD_SPELLXY";
	case CMD_OPOBJXY: return "CMD_OPOBJXY";
	case CMD_DISARMXY: return "CMD_DISARMXY";
	case CMD_ATTACKID: return "CMD_ATTACKID";
	case CMD_ATTACKPID: return "CMD_ATTACKPID";
	case CMD_RATTACKID: return "CMD_RATTACKID";
	case CMD_RATTACKPID: return "CMD_RATTACKPID";
	case CMD_SPELLID: return "CMD_SPELLID";
	case CMD_SPELLPID: return "CMD_SPELLPID";
	case CMD_RESURRECT: return "CMD_RESURRECT";
	case CMD_OPOBJT: return "CMD_OPOBJT";
	case CMD_KNOCKBACK: return "CMD_KNOCKBACK";
	case CMD_TALKXY: return "CMD_TALKXY";
	case CMD_NEWLVL: return "CMD_NEWLVL";
	case CMD_WARP: return "CMD_WARP";
	case CMD_CHEAT_EXPERIENCE: return "CMD_CHEAT_EXPERIENCE";
	case CMD_CHEAT_SPELL_LEVEL: return "CMD_CHEAT_SPELL_LEVEL";
	case CMD_DEBUG: return "CMD_DEBUG";
	case CMD_SYNCDATA: return "CMD_SYNCDATA";
	case CMD_MONSTDEATH: return "CMD_MONSTDEATH";
	case CMD_MONSTDAMAGE: return "CMD_MONSTDAMAGE";
	case CMD_PLRDEAD: return "CMD_PLRDEAD";
	case CMD_REQUESTGITEM: return "CMD_REQUESTGITEM";
	case CMD_REQUESTAGITEM: return "CMD_REQUESTAGITEM";
	case CMD_GOTOGETITEM: return "CMD_GOTOGETITEM";
	case CMD_GOTOAGETITEM: return "CMD_GOTOAGETITEM";
	case CMD_OPENDOOR: return "CMD_OPENDOOR";
	case CMD_CLOSEDOOR: return "CMD_CLOSEDOOR";
	case CMD_OPERATEOBJ: return "CMD_OPERATEOBJ";
	case CMD_BREAKOBJ: return "CMD_BREAKOBJ";
	case CMD_CHANGEPLRITEMS: return "CMD_CHANGEPLRITEMS";
	case CMD_DELPLRITEMS: return "CMD_DELPLRITEMS";
	case CMD_CHANGEINVITEMS: return "CMD_CHANGEINVITEMS";
	case CMD_DELINVITEMS: return "CMD_DELINVITEMS";
	case CMD_CHANGEBELTITEMS: return "CMD_CHANGEBELTITEMS";
	case CMD_DELBELTITEMS: return "CMD_DELBELTITEMS";
	case CMD_PLRDAMAGE: return "CMD_PLRDAMAGE";
	case CMD_PLRLEVEL: return "CMD_PLRLEVEL";
	case CMD_DROPITEM: return "CMD_DROPITEM";
	case CMD_PLAYER_JOINLEVEL: return "CMD_PLAYER_JOINLEVEL";
	case CMD_SEND_PLRINFO: return "CMD_SEND_PLRINFO";
	case CMD_SATTACKXY: return "CMD_SATTACKXY";
	case CMD_ACTIVATEPORTAL: return "CMD_ACTIVATEPORTAL";
	case CMD_DEACTIVATEPORTAL: return "CMD_DEACTIVATEPORTAL";
	case CMD_DLEVEL: return "CMD_DLEVEL";
	case CMD_DLEVEL_JUNK: return "CMD_DLEVEL_JUNK";
	case CMD_DLEVEL_END: return "CMD_DLEVEL_END";
	case CMD_HEALOTHER: return "CMD_HEALOTHER";
	case CMD_STRING: return "CMD_STRING";
	case CMD_FRIENDLYMODE: return "CMD_FRIENDLYMODE";
	case CMD_SETSTR: return "CMD_SETSTR";
	case CMD_SETMAG: return "CMD_SETMAG";
	case CMD_SETDEX: return "CMD_SETDEX";
	case CMD_SETVIT: return "CMD_SETVIT";
	case CMD_RETOWN: return "CMD_RETOWN";
	case CMD_SPELLXYD: return "CMD_SPELLXYD";
	case CMD_ITEMEXTRA: return "CMD_ITEMEXTRA";
	case CMD_SYNCPUTITEM: return "CMD_SYNCPUTITEM";
	case CMD_KILLGOLEM: return "CMD_KILLGOLEM";
	case CMD_SYNCQUEST: return "CMD_SYNCQUEST";
	case CMD_AWAKEGOLEM: return "CMD_AWAKEGOLEM";
	case CMD_SETSHIELD: return "CMD_SETSHIELD";
	case CMD_REMSHIELD: return "CMD_REMSHIELD";
	case CMD_SETREFLECT: return "CMD_SETREFLECT";
	case CMD_NAKRUL: return "CMD_NAKRUL";
	case CMD_OPENHIVE: return "CMD_OPENHIVE";
	case CMD_OPENGRAVE: return "CMD_OPENGRAVE";
	case CMD_SPAWNMONSTER: return "CMD_SPAWNMONSTER";
	case FAKE_CMD_SETID: return "FAKE_CMD_SETID";
	case FAKE_CMD_DROPID: return "FAKE_CMD_DROPID";
	case CMD_INVALID: return "CMD_INVALID";
	default: return "";
	}
	// clang-format on
}
#endif // LOG_RECEIVED_MESSAGES

struct TMegaPkt {
	size_t spaceLeft;
	std::byte data[32000];

	TMegaPkt()
	    : spaceLeft(sizeof(data))
	{
	}
};

#pragma pack(push, 1)
struct DMonsterStr {
	WorldTilePosition position;
	uint8_t _menemy;
	uint8_t _mactive;
	int32_t hitPoints;
	int8_t mWhoHit;
};
#pragma pack(pop)

struct DObjectStr {
	_cmd_id bCmd;
};

struct DSpawnedMonster {
	size_t typeIndex;
	uint32_t seed;
};

struct DLevel {
	TCmdPItem item[MAXITEMS];
	ankerl::unordered_dense::map<WorldTilePosition, DObjectStr> object;
	ankerl::unordered_dense::map<size_t, DSpawnedMonster> spawnedMonsters;
	DMonsterStr monster[MaxMonsters];
};

#pragma pack(push, 1)
struct LocalLevel {
	LocalLevel(const uint8_t (&other)[DMAXX][DMAXY])
	{
		memcpy(&automapsv, &other, sizeof(automapsv));
	}
	uint8_t automapsv[DMAXX][DMAXY];
};

struct DPortal {
	uint8_t x;
	uint8_t y;
	uint8_t level;
	uint8_t ltype;
	uint8_t setlvl;
};

struct MultiQuests {
	quest_state qstate;
	uint8_t qlog;
	uint8_t qvar1;
	uint8_t qvar2;
	int16_t qmsg;
};

struct DJunk {
	DPortal portal[MAXPORTAL];
	MultiQuests quests[MAXQUESTS];
};
#pragma pack(pop)

constexpr size_t MAX_MULTIPLAYERLEVELS = NUMLEVELS + SL_LAST;
constexpr size_t MAX_CHUNKS = MAX_MULTIPLAYERLEVELS + 4;

uint32_t sgdwOwnerWait;
uint32_t sgdwRecvOffset;
int sgnCurrMegaPlayer;
ankerl::unordered_dense::map<uint8_t, DLevel> DeltaLevels;
uint8_t sbLastCmd;
/**
 * @brief buffer used to receive level deltas, size is the worst expected case assuming every object on a level was touched
 */
std::byte sgRecvBuf[1U + sizeof(DLevel::item) + sizeof(uint8_t) + (sizeof(WorldTilePosition) + sizeof(_cmd_id)) * MAXOBJECTS + sizeof(DLevel::monster)];
_cmd_id sgbRecvCmd;
ankerl::unordered_dense::map<uint8_t, LocalLevel> LocalLevels;
DJunk sgJunk;
uint8_t sgbDeltaChunks;
std::list<TMegaPkt> MegaPktList;
Item ItemLimbo;

/** @brief Last sent player command for the local player. */
TCmdLocParam5 lastSentPlayerCmd;

uint8_t GetLevelForMultiplayer(uint8_t level, bool isSetLevel)
{
	if (isSetLevel)
		return level + NUMLEVELS;
	return level;
}

/** @brief Gets a delta level. */
DLevel &GetDeltaLevel(uint8_t level)
{
	auto keyIt = DeltaLevels.find(level);
	if (keyIt != DeltaLevels.end())
		return keyIt->second;
	DLevel &deltaLevel = DeltaLevels[level];
	memset(&deltaLevel.item, 0xFF, sizeof(deltaLevel.item));
	memset(&deltaLevel.monster, 0xFF, sizeof(deltaLevel.monster));
	return deltaLevel;
}

/** @brief Gets a delta level. */
DLevel &GetDeltaLevel(const Player &player)
{
	uint8_t level = GetLevelForMultiplayer(player);
	return GetDeltaLevel(level);
}

Point GetItemPosition(Point position)
{
	if (CanPut(position))
		return position;

	for (int k = 1; k < 50; k++) {
		for (int j = -k; j <= k; j++) {
			int yy = position.y + j;
			for (int l = -k; l <= k; l++) {
				int xx = position.x + l;
				if (CanPut({ xx, yy }))
					return { xx, yy };
			}
		}
	}

	return position;
}

/**
 * @brief Throttles that a player command is only sent once per game tick.
 * This is a workaround for a desync that happens when a command is processed in different game ticks for different clients. See https://github.com/diasurgical/devilutionX/issues/2681 for details.
 * When a proper fix is implemented this workaround can be removed.
 */
bool WasPlayerCmdAlreadyRequested(_cmd_id bCmd, Point position = {}, uint16_t wParam1 = 0, uint16_t wParam2 = 0, uint16_t wParam3 = 0, uint16_t wParam4 = 0, uint16_t wParam5 = 0)
{
	switch (bCmd) {
	// All known commands that result in a changed player action (player.destAction)
	case _cmd_id::CMD_RATTACKID:
	case _cmd_id::CMD_SPELLID:
	case _cmd_id::CMD_ATTACKID:
	case _cmd_id::CMD_RATTACKPID:
	case _cmd_id::CMD_SPELLPID:
	case _cmd_id::CMD_ATTACKPID:
	case _cmd_id::CMD_ATTACKXY:
	case _cmd_id::CMD_SATTACKXY:
	case _cmd_id::CMD_RATTACKXY:
	case _cmd_id::CMD_SPELLXY:
	case _cmd_id::CMD_SPELLXYD:
	case _cmd_id::CMD_WALKXY:
	case _cmd_id::CMD_TALKXY:
	case _cmd_id::CMD_DISARMXY:
	case _cmd_id::CMD_OPOBJXY:
	case _cmd_id::CMD_GOTOGETITEM:
	case _cmd_id::CMD_GOTOAGETITEM:
		break;
	default:
		// None player actions should work normally
		return false;
	}

	TCmdLocParam5 newSendParam = { bCmd, static_cast<uint8_t>(position.x), static_cast<uint8_t>(position.y),
		SDL_SwapLE16(wParam1), SDL_SwapLE16(wParam2), SDL_SwapLE16(wParam3), SDL_SwapLE16(wParam4), SDL_SwapLE16(wParam5) };

	if (lastSentPlayerCmd.bCmd == newSendParam.bCmd && lastSentPlayerCmd.x == newSendParam.x && lastSentPlayerCmd.y == newSendParam.y
	    && lastSentPlayerCmd.wParam1 == newSendParam.wParam1 && lastSentPlayerCmd.wParam2 == newSendParam.wParam2
	    && lastSentPlayerCmd.wParam3 == newSendParam.wParam3 && lastSentPlayerCmd.wParam4 == newSendParam.wParam4
	    && lastSentPlayerCmd.wParam5 == newSendParam.wParam5) {
		// Command already send in this game tick => don't send again / throttle
		return true;
	}

	lastSentPlayerCmd = newSendParam;

	return false;
}

void GetNextPacket()
{
	MegaPktList.emplace_back();
}

void FreePackets()
{
	MegaPktList.clear();
}

void PrePacket()
{
	uint8_t playerId = std::numeric_limits<uint8_t>::max();
	for (TMegaPkt &pkt : MegaPktList) {
		std::byte *data = pkt.data;
		size_t spaceLeft = sizeof(pkt.data);
		while (spaceLeft != pkt.spaceLeft) {
			auto cmdId = static_cast<_cmd_id>(*data);

			if (cmdId == FAKE_CMD_SETID) {
				auto *cmd = (TFakeCmdPlr *)data;
				data += sizeof(*cmd);
				spaceLeft -= sizeof(*cmd);
				playerId = cmd->bPlr;
				continue;
			}

			if (cmdId == FAKE_CMD_DROPID) {
				auto *cmd = (TFakeDropPlr *)data;
				data += sizeof(*cmd);
				spaceLeft -= sizeof(*cmd);
				multi_player_left(cmd->bPlr, SDL_SwapLE32(cmd->dwReason));
				continue;
			}

			if (playerId >= Players.size()) {
				Log("Missing source of network message");
				return;
			}

			size_t size = ParseCmd(playerId, (TCmd *)data);
			if (size == 0) {
				Log("Discarding bad network message");
				return;
			}
			data += size;
			spaceLeft -= size;
		}
	}
}

void SendPacket(uint8_t pnum, const void *packet, size_t dwSize)
{
	if (pnum != sgnCurrMegaPlayer) {
		sgnCurrMegaPlayer = pnum;
		TFakeCmdPlr cmd;
		cmd.bCmd = FAKE_CMD_SETID;
		cmd.bPlr = pnum;
		SendPacket(pnum, &cmd, sizeof(cmd));
	}
	if (MegaPktList.back().spaceLeft < dwSize)
		GetNextPacket();

	TMegaPkt &currMegaPkt = MegaPktList.back();
	memcpy(currMegaPkt.data + sizeof(currMegaPkt.data) - currMegaPkt.spaceLeft, packet, dwSize);
	currMegaPkt.spaceLeft -= dwSize;
}

void SendPacket(const Player &player, const void *packet, size_t dwSize)
{
	SendPacket(player.getId(), packet, dwSize);
}

int WaitForTurns()
{
	uint32_t turns;

	if (sgbDeltaChunks == 0) {
		nthread_send_and_recv_turn(0, 0);
		SNetGetOwnerTurnsWaiting(&turns);
		if (SDL_GetTicks() - sgdwOwnerWait <= 2000 && turns < gdwTurnsInTransit)
			return 0;
		sgbDeltaChunks++;
	}
	multi_process_network_packets();
	nthread_send_and_recv_turn(0, 0);
	if (nthread_has_500ms_passed()) {
		nthread_recv_turns();
	}

	if (gbGameDestroyed)
		return 100;
	if (gbDeltaSender >= Players.size()) {
		sgbDeltaChunks = 0;
		sgbRecvCmd = CMD_DLEVEL_END;
		gbDeltaSender = MyPlayerId;
		nthread_set_turn_upper_bit();
	}
	if (sgbDeltaChunks == MAX_CHUNKS - 1) {
		sgbDeltaChunks = MAX_CHUNKS;
		return 99;
	}
	return 100 * sgbDeltaChunks / MAX_CHUNKS;
}

std::byte *DeltaExportItem(std::byte *dst, const TCmdPItem *src)
{
	for (int i = 0; i < MAXITEMS; i++, src++) {
		if (src->bCmd == CMD_INVALID) {
			*dst++ = std::byte { 0xFF };
		} else {
			memcpy(dst, src, sizeof(TCmdPItem));
			dst += sizeof(TCmdPItem);
		}
	}

	return dst;
}

size_t DeltaImportItem(const std::byte *src, TCmdPItem *dst)
{
	size_t size = 0;
	for (int i = 0; i < MAXITEMS; i++, dst++) {
		if (src[size] == std::byte { 0xFF }) {
			memset(dst, 0xFF, sizeof(TCmdPItem));
			size++;
		} else {
			memcpy(dst, &src[size], sizeof(TCmdPItem));
			size += sizeof(TCmdPItem);
		}
	}

	return size;
}

std::byte *DeltaExportObject(std::byte *dst, const ankerl::unordered_dense::map<WorldTilePosition, DObjectStr> &src)
{
	*dst++ = static_cast<std::byte>(src.size());
	for (const auto &[position, obj] : src) {
		*dst++ = static_cast<std::byte>(position.x);
		*dst++ = static_cast<std::byte>(position.y);
		*dst++ = static_cast<std::byte>(obj.bCmd);
	}

	return dst;
}

const std::byte *DeltaImportObjects(const std::byte *src, ankerl::unordered_dense::map<WorldTilePosition, DObjectStr> &dst)
{
	dst.clear();

	uint8_t numDeltas = static_cast<uint8_t>(*src++);
	dst.reserve(numDeltas);

	for (unsigned i = 0; i < numDeltas; i++) {
		WorldTilePosition objectPosition { static_cast<WorldTileCoord>(src[0]), static_cast<WorldTileCoord>(src[1]) };
		src += 2;
		dst[objectPosition] = DObjectStr { static_cast<_cmd_id>(*src++) };
	}

	return src;
}

std::byte *DeltaExportMonster(std::byte *dst, const DMonsterStr *src)
{
	for (size_t i = 0; i < MaxMonsters; i++, src++) {
		if (src->position.x == 0xFF) {
			*dst++ = std::byte { 0xFF };
		} else {
			memcpy(dst, src, sizeof(DMonsterStr));
			dst += sizeof(DMonsterStr);
		}
	}

	return dst;
}

size_t DeltaImportMonster(const std::byte *src, DMonsterStr *dst)
{
	size_t size = 0;
	for (size_t i = 0; i < MaxMonsters; i++, dst++) {
		if (src[size] == std::byte { 0xFF }) {
			memset(dst, 0xFF, sizeof(DMonsterStr));
			size++;
		} else {
			memcpy(dst, &src[size], sizeof(DMonsterStr));
			size += sizeof(DMonsterStr);
		}
	}

	return size;
}

std::byte *DeltaExportSpawnedMonsters(std::byte *dst, const ankerl::unordered_dense::map<size_t, DSpawnedMonster> &spawnedMonsters)
{
	auto &size = *reinterpret_cast<uint16_t *>(dst);
	size = static_cast<uint16_t>(spawnedMonsters.size());
	dst += sizeof(uint16_t);

	for (const auto &deltaSpawnedMonster : spawnedMonsters) {
		auto &monsterId = *reinterpret_cast<uint16_t *>(dst);
		monsterId = static_cast<uint16_t>(deltaSpawnedMonster.first);
		dst += sizeof(uint16_t);

		memcpy(dst, &deltaSpawnedMonster.second, sizeof(DSpawnedMonster));
		dst += sizeof(DSpawnedMonster);
	}

	return dst;
}

const std::byte *DeltaImportSpawnedMonsters(const std::byte *src, ankerl::unordered_dense::map<size_t, DSpawnedMonster> &spawnedMonsters)
{
	uint16_t size = *reinterpret_cast<const uint16_t *>(src);
	src += sizeof(uint16_t);

	for (size_t i = 0; i < size; i++) {
		uint16_t monsterId = *reinterpret_cast<const uint16_t *>(src);
		src += sizeof(uint16_t);
		DSpawnedMonster spawnedMonster;
		memcpy(&spawnedMonster, src, sizeof(DSpawnedMonster));
		src += sizeof(DSpawnedMonster);
		spawnedMonsters.emplace(monsterId, spawnedMonster);
	}

	return src;
}

std::byte *DeltaExportJunk(std::byte *dst)
{
	for (auto &portal : sgJunk.portal) {
		if (portal.x == 0xFF) {
			*dst++ = std::byte { 0xFF };
		} else {
			memcpy(dst, &portal, sizeof(DPortal));
			dst += sizeof(DPortal);
		}
	}

	int q = 0;
	for (auto &quest : Quests) {
		if (QuestsData[quest._qidx].isSinglePlayerOnly && UseMultiplayerQuests()) {
			continue;
		}
		sgJunk.quests[q].qlog = quest._qlog ? 1 : 0;
		sgJunk.quests[q].qstate = quest._qactive;
		sgJunk.quests[q].qvar1 = quest._qvar1;
		sgJunk.quests[q].qvar2 = quest._qvar2;
		sgJunk.quests[q].qmsg = static_cast<int16_t>(quest._qmsg);
		memcpy(dst, &sgJunk.quests[q], sizeof(MultiQuests));
		dst += sizeof(MultiQuests);
		q++;
	}

	return dst;
}

void DeltaImportJunk(const std::byte *src)
{
	for (int i = 0; i < MAXPORTAL; i++) {
		if (*src == std::byte { 0xFF }) {
			memset(&sgJunk.portal[i], 0xFF, sizeof(DPortal));
			src++;
		} else {
			memcpy(&sgJunk.portal[i], src, sizeof(DPortal));
			src += sizeof(DPortal);
		}
	}

	int q = 0;
	for (int qidx = 0; qidx < MAXQUESTS; qidx++) {
		if (QuestsData[qidx].isSinglePlayerOnly && UseMultiplayerQuests()) {
			continue;
		}
		memcpy(&sgJunk.quests[q], src, sizeof(MultiQuests));
		src += sizeof(MultiQuests);
		q++;
	}
}

uint32_t CompressData(std::byte *buffer, std::byte *end)
{
#ifdef USE_PKWARE
	const auto size = static_cast<uint32_t>(end - buffer - 1);
	const uint32_t pkSize = PkwareCompress(buffer + 1, size);

	*buffer = size != pkSize ? std::byte { 1 } : std::byte { 0 };

	return pkSize + 1;
#else
	*buffer = std::byte { 0 };
	return end - buffer;
#endif
}

void DeltaImportData(_cmd_id cmd, uint32_t recvOffset)
{
#ifdef USE_PKWARE
	if (sgRecvBuf[0] != std::byte { 0 })
		PkwareDecompress(&sgRecvBuf[1], recvOffset, sizeof(sgRecvBuf) - 1);
#endif

	const std::byte *src = &sgRecvBuf[1];
	if (cmd == CMD_DLEVEL_JUNK) {
		DeltaImportJunk(src);
	} else if (cmd == CMD_DLEVEL) {
		uint8_t i = static_cast<uint8_t>(src[0]);
		src += sizeof(uint8_t);
		DLevel &deltaLevel = GetDeltaLevel(i);
		src += DeltaImportItem(src, deltaLevel.item);
		src = DeltaImportObjects(src, deltaLevel.object);
		src += DeltaImportMonster(src, deltaLevel.monster);
		src = DeltaImportSpawnedMonsters(src, deltaLevel.spawnedMonsters);
	} else {
		app_fatal(StrCat("Unknown network message type: ", cmd));
	}

	sgbDeltaChunks++;
}

size_t OnLevelData(uint8_t pnum, const TCmd *pCmd)
{
	const auto &message = *reinterpret_cast<const TCmdPlrInfoHdr *>(pCmd);
	const uint16_t wBytes = SDL_SwapLE16(message.wBytes);
	const uint16_t wOffset = SDL_SwapLE16(message.wOffset);

	if (gbDeltaSender != pnum) {
		if (message.bCmd != CMD_DLEVEL_END && (message.bCmd != CMD_DLEVEL || wOffset != 0)) {
			return wBytes + sizeof(message);
		}

		gbDeltaSender = pnum;
		sgbRecvCmd = CMD_DLEVEL_END;
	}

	if (sgbRecvCmd == CMD_DLEVEL_END) {
		if (message.bCmd == CMD_DLEVEL_END) {
			sgbDeltaChunks = MAX_CHUNKS - 1;
			return wBytes + sizeof(message);
		}
		if (message.bCmd != CMD_DLEVEL || wOffset != 0) {
			return wBytes + sizeof(message);
		}

		sgdwRecvOffset = 0;
		sgbRecvCmd = message.bCmd;
	} else if (sgbRecvCmd != message.bCmd || wOffset == 0) {
		DeltaImportData(sgbRecvCmd, sgdwRecvOffset);
		if (message.bCmd == CMD_DLEVEL_END) {
			sgbDeltaChunks = MAX_CHUNKS - 1;
			sgbRecvCmd = CMD_DLEVEL_END;
			return wBytes + sizeof(message);
		}
		sgdwRecvOffset = 0;
		sgbRecvCmd = message.bCmd;
	}

	assert(wOffset == sgdwRecvOffset);
	memcpy(&sgRecvBuf[wOffset], &message + 1, wBytes);
	sgdwRecvOffset += wBytes;
	return wBytes + sizeof(message);
}

void DeltaSyncGolem(const TCmdGolem &message, const Player &player, uint8_t level)
{
	if (!gbIsMultiplayer)
		return;

	DMonsterStr &monster = GetDeltaLevel(level).monster[player.getId()];
	monster.position.x = message._mx;
	monster.position.y = message._my;
	monster._mactive = UINT8_MAX;
	monster._menemy = message._menemy;
	monster.hitPoints = SDL_SwapLE32(message._mhitpoints);
}

void DeltaLeaveSync(uint8_t bLevel)
{
	if (!gbIsMultiplayer)
		return;
	if (leveltype == DTYPE_TOWN) {
		DungeonSeeds[0] = GenerateSeed();
		return;
	}

	DLevel &deltaLevel = GetDeltaLevel(bLevel);

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const unsigned ma = ActiveMonsters[i];
		Monster &monster = Monsters[ma];
		if (monster.hitPoints == 0)
			continue;
		DMonsterStr &delta = deltaLevel.monster[ma];
		delta.position = monster.position.tile;
		delta._menemy = encode_enemy(monster);
		delta.hitPoints = monster.hitPoints;
		delta._mactive = monster.activeForTicks;
		delta.mWhoHit = monster.whoHit;
	}
	LocalLevels.insert_or_assign(bLevel, AutomapView);
}

void DeltaSyncObject(WorldTilePosition position, _cmd_id bCmd, const Player &player)
{
	if (!gbIsMultiplayer)
		return;

	auto &objectDeltas = GetDeltaLevel(player).object;
	objectDeltas[position].bCmd = bCmd;
}

bool DeltaGetItem(const TCmdGItem &message, uint8_t bLevel)
{
	if (!gbIsMultiplayer)
		return true;

	DLevel &deltaLevel = GetDeltaLevel(bLevel);

	for (TCmdPItem &item : deltaLevel.item) {
		if (item.bCmd == CMD_INVALID || item.def.wIndx != message.def.wIndx
		    || item.def.wCI != message.def.wCI || item.def.dwSeed != message.def.dwSeed) {
			continue;
		}

		if (item.bCmd == TCmdPItem::PickedUpItem) {
			return true;
		}
		if (item.bCmd == TCmdPItem::FloorItem) {
			item.bCmd = TCmdPItem::PickedUpItem;
			return true;
		}
		if (item.bCmd == TCmdPItem::DroppedItem) {
			item.bCmd = CMD_INVALID;
			return true;
		}

		app_fatal("delta:1");
	}

	if ((message.def.wCI & CF_PREGEN) == 0)
		return false;

	for (TCmdPItem &delta : deltaLevel.item) {
		if (delta.bCmd == CMD_INVALID) {
			delta.bCmd = TCmdPItem::PickedUpItem;
			delta.x = message.x;
			delta.y = message.y;
			delta.def.wIndx = message.def.wIndx;
			delta.def.wCI = message.def.wCI;
			delta.def.dwSeed = message.def.dwSeed;
			if (message.def.wIndx == IDI_EAR) {
				delta.ear.bCursval = message.ear.bCursval;
				CopyUtf8(delta.ear.heroname, message.ear.heroname, sizeof(delta.ear.heroname));
			} else {
				delta.item.bId = message.item.bId;
				delta.item.bDur = message.item.bDur;
				delta.item.bMDur = message.item.bMDur;
				delta.item.bCh = message.item.bCh;
				delta.item.bMCh = message.item.bMCh;
				delta.item.wValue = message.item.wValue;
				delta.item.dwBuff = message.item.dwBuff;
				delta.item.wToHit = message.item.wToHit;
			}
			break;
		}
	}
	return true;
}

void DeltaPutItem(const TCmdPItem &message, Point position, const Player &player)
{
	if (!gbIsMultiplayer)
		return;

	DLevel &deltaLevel = GetDeltaLevel(player);

	for (const TCmdPItem &item : deltaLevel.item) {
		if (item.bCmd != TCmdPItem::PickedUpItem
		    && item.bCmd != CMD_INVALID
		    && item.def.wIndx == message.def.wIndx
		    && item.def.wCI == message.def.wCI
		    && item.def.dwSeed == message.def.dwSeed) {
			if (item.bCmd == TCmdPItem::DroppedItem)
				return;
			app_fatal(_("Trying to drop a floor item?"));
		}
	}

	for (TCmdPItem &item : deltaLevel.item) {
		if (item.bCmd == CMD_INVALID) {
			memcpy(&item, &message, sizeof(TCmdPItem));
			item.bCmd = TCmdPItem::DroppedItem;
			item.x = position.x;
			item.y = position.y;
			return;
		}
	}
}

void DeltaOpenPortal(size_t pnum, Point position, uint8_t bLevel, dungeon_type bLType, bool bSetLvl)
{
	sgJunk.portal[pnum].x = position.x;
	sgJunk.portal[pnum].y = position.y;
	sgJunk.portal[pnum].level = bLevel;
	sgJunk.portal[pnum].ltype = bLType;
	sgJunk.portal[pnum].setlvl = bSetLvl ? 1 : 0;
}

void NetSendCmdGItem2(bool usonly, _cmd_id bCmd, uint8_t mast, uint8_t pnum, const TCmdGItem &item)
{
	TCmdGItem cmd;

	memcpy(&cmd, &item, sizeof(cmd));
	cmd.bPnum = pnum;
	cmd.bCmd = bCmd;
	cmd.bMaster = mast;

	if (!usonly) {
		cmd.dwTime = 0;
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
		return;
	}

	int ticks = SDL_GetTicks();
	if (cmd.dwTime == 0) {
		cmd.dwTime = SDL_SwapLE32(ticks);
	} else if (ticks - SDL_SwapLE32(cmd.dwTime) > 5000) {
		return;
	}

	tmsg_add((std::byte *)&cmd, sizeof(cmd));
}

bool NetSendCmdReq2(_cmd_id bCmd, const Player &player, uint8_t pnum, const TCmdGItem &item)
{
	TCmdGItem cmd;

	memcpy(&cmd, &item, sizeof(cmd));
	cmd.bCmd = bCmd;
	cmd.bPnum = pnum;
	cmd.bMaster = player.getId();

	int ticks = SDL_GetTicks();
	if (cmd.dwTime == 0)
		cmd.dwTime = SDL_SwapLE32(ticks);
	else if (ticks - SDL_SwapLE32(cmd.dwTime) > 5000)
		return false;

	tmsg_add((std::byte *)&cmd, sizeof(cmd));

	return true;
}

void NetSendCmdExtra(const TCmdGItem &item)
{
	TCmdGItem cmd;

	memcpy(&cmd, &item, sizeof(cmd));
	cmd.dwTime = 0;
	cmd.bCmd = CMD_ITEMEXTRA;
	NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

size_t OnWalk(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLoc *>(pCmd);
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position)) {
		ClrPlrPath(player);
		MakePlrPath(player, position, true);
		player.destAction = ACTION_NONE;
	}

	return sizeof(message);
}

size_t OnAddStrength(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);

	if (gbBufferMsgs == 1)
		SendPacket(player, &message, sizeof(message));
	else if (message.wParam1 <= 256)
		ModifyPlrStr(player, SDL_SwapLE16(message.wParam1));

	return sizeof(message);
}

size_t OnAddMagic(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);

	if (gbBufferMsgs == 1)
		SendPacket(player, &message, sizeof(message));
	else if (message.wParam1 <= 256)
		ModifyPlrMag(player, SDL_SwapLE16(message.wParam1));

	return sizeof(message);
}

size_t OnAddDexterity(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);

	if (gbBufferMsgs == 1)
		SendPacket(player, &message, sizeof(message));
	else if (message.wParam1 <= 256)
		ModifyPlrDex(player, SDL_SwapLE16(message.wParam1));

	return sizeof(message);
}

size_t OnAddVitality(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);

	if (gbBufferMsgs == 1)
		SendPacket(player, &message, sizeof(message));
	else if (message.wParam1 <= 256)
		ModifyPlrVit(player, SDL_SwapLE16(message.wParam1));

	return sizeof(message);
}

size_t OnGotoGetItem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLocParam1 *>(pCmd);
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position) && SDL_SwapLE16(message.wParam1) < MAXITEMS + 1) {
		MakePlrPath(player, position, false);
		player.destAction = ACTION_PICKUPITEM;
		player.destParam1 = SDL_SwapLE16(message.wParam1);
	}

	return sizeof(message);
}

bool IsGItemValid(const TCmdGItem &message)
{
	if (message.bMaster >= Players.size())
		return false;
	if (message.bPnum >= Players.size())
		return false;
	if (message.bCursitem >= MAXITEMS + 1)
		return false;
	if (!IsValidLevelForMultiplayer(message.bLevel))
		return false;

	if (!InDungeonBounds({ message.x, message.y }))
		return false;

	return IsItemAvailable(static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx)));
}

bool IsPItemValid(const TCmdPItem &message, const Player &player)
{
	if (!gbIsMultiplayer)
		return true;

	const Point position { message.x, message.y };

	if (!InDungeonBounds(position))
		return false;

	auto idx = static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx));

	if (idx != IDI_EAR) {
		uint16_t creationFlags = SDL_SwapLE16(message.item.wCI);
		uint32_t dwBuff = SDL_SwapLE16(message.item.dwBuff);

		if (idx != IDI_GOLD)
			ValidateField(creationFlags, IsCreationFlagComboValid(creationFlags));
		if ((creationFlags & CF_TOWN) != 0)
			ValidateField(creationFlags, IsTownItemValid(creationFlags, player));
		else if ((creationFlags & CF_USEFUL) == CF_UPER15)
			ValidateFields(creationFlags, dwBuff, IsUniqueMonsterItemValid(creationFlags, dwBuff));
		else if ((dwBuff & CF_HELLFIRE) != 0 && AllItemsList[idx].iMiscId == IMISC_BOOK)
			return RecreateHellfireSpellBook(player, message.item);
		else
			ValidateFields(creationFlags, dwBuff, IsDungeonItemValid(creationFlags, dwBuff));
	}

	return IsItemAvailable(idx);
}

void PrepareItemForNetwork(const Item &item, TCmdGItem &message)
{
	message.def.wIndx = static_cast<_item_indexes>(SDL_SwapLE16(item.IDidx));
	message.def.wCI = SDL_SwapLE16(item._iCreateInfo);
	message.def.dwSeed = SDL_SwapLE32(item._iSeed);

	if (item.IDidx == IDI_EAR)
		PrepareEarForNetwork(item, message.ear);
	else
		PrepareItemForNetwork(item, message.item);
}

void PrepareItemForNetwork(const Item &item, TCmdPItem &message)
{
	message.def.wIndx = static_cast<_item_indexes>(SDL_SwapLE16(item.IDidx));
	message.def.wCI = SDL_SwapLE16(item._iCreateInfo);
	message.def.dwSeed = SDL_SwapLE32(item._iSeed);

	if (item.IDidx == IDI_EAR)
		PrepareEarForNetwork(item, message.ear);
	else
		PrepareItemForNetwork(item, message.item);
}

void PrepareItemForNetwork(const Item &item, TCmdChItem &message)
{
	message.def.wIndx = static_cast<_item_indexes>(SDL_SwapLE16(item.IDidx));
	message.def.wCI = SDL_SwapLE16(item._iCreateInfo);
	message.def.dwSeed = SDL_SwapLE32(item._iSeed);

	if (item.IDidx == IDI_EAR)
		PrepareEarForNetwork(item, message.ear);
	else
		PrepareItemForNetwork(item, message.item);
}

void RecreateItem(const Player &player, const TCmdPItem &message, Item &item)
{
	if (message.def.wIndx == SDL_SwapLE16(IDI_EAR))
		RecreateEar(item, SDL_SwapLE16(message.ear.wCI), SDL_SwapLE32(message.ear.dwSeed), message.ear.bCursval, message.ear.heroname);
	else
		RecreateItem(player, message.item, item);
}

void RecreateItem(const Player &player, const TCmdChItem &message, Item &item)
{
	if (message.def.wIndx == SDL_SwapLE16(IDI_EAR))
		RecreateEar(item, SDL_SwapLE16(message.ear.wCI), SDL_SwapLE32(message.ear.dwSeed), message.ear.bCursval, message.ear.heroname);
	else
		RecreateItem(player, message.item, item);
}

int SyncDropItem(Point position, const TItem &item)
{
	return SyncDropItem(
	    position,
	    static_cast<_item_indexes>(SDL_SwapLE16(item.wIndx)),
	    SDL_SwapLE16(item.wCI),
	    SDL_SwapLE32(item.dwSeed),
	    item.bId,
	    item.bDur,
	    item.bMDur,
	    item.bCh,
	    item.bMCh,
	    SDL_SwapLE16(item.wValue),
	    SDL_SwapLE32(item.dwBuff),
	    SDL_SwapLE16(item.wToHit),
	    SDL_SwapLE16(item.wMaxDam));
}

int SyncDropEar(Point position, const TEar &ear)
{
	return SyncDropEar(
	    position,
	    SDL_SwapLE16(ear.wCI),
	    SDL_SwapLE32(ear.dwSeed),
	    ear.bCursval,
	    ear.heroname);
}

int SyncDropItem(const TCmdGItem &message)
{
	Point position = GetItemPosition({ message.x, message.y });
	if (message.def.wIndx == IDI_EAR) {
		return SyncDropEar(
		    position,
		    message.ear);
	}

	return SyncDropItem(
	    position,
	    message.item);
}

int SyncDropItem(const TCmdPItem &message)
{
	Point position = GetItemPosition({ message.x, message.y });
	if (message.def.wIndx == IDI_EAR) {
		return SyncDropEar(
		    position,
		    message.ear);
	}

	return SyncDropItem(
	    position,
	    message.item);
}

size_t OnRequestGetItem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdGItem *>(pCmd);

	if (gbBufferMsgs != 1 && player.isLevelOwnedByLocalClient() && IsGItemValid(message)) {
		const Point position { message.x, message.y };
		const uint32_t dwSeed = SDL_SwapLE32(message.def.dwSeed);
		const uint16_t wCI = SDL_SwapLE16(message.def.wCI);
		const _item_indexes wIndx = static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx));
		if (GetItemRecord(dwSeed, wCI, wIndx)) {
			int ii = -1;
			if (InDungeonBounds(position)) {
				ii = std::abs(dItem[position.x][position.y]) - 1;
				if (ii >= 0 && !Items[ii].keyAttributesMatch(dwSeed, wIndx, wCI)) {
					ii = -1;
				}
			}

			if (ii == -1) {
				// No item at the target position or the key attributes don't match, so try find a matching item.
				int activeItemIndex = FindGetItem(dwSeed, wIndx, wCI);
				if (activeItemIndex != -1) {
					ii = ActiveItems[activeItemIndex];
				}
			}

			if (ii != -1) {
				NetSendCmdGItem2(false, CMD_GETITEM, MyPlayerId, message.bPnum, message);
				if (message.bPnum != MyPlayerId)
					SyncGetItem(position, dwSeed, wIndx, wCI);
				else
					InvGetItem(*MyPlayer, ii);
				SetItemRecord(dwSeed, wCI, wIndx);
			} else if (!NetSendCmdReq2(CMD_REQUESTGITEM, *MyPlayer, message.bPnum, message)) {
				NetSendCmdExtra(message);
			}
		}
	}

	return sizeof(message);
}

size_t OnGetItem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdGItem *>(pCmd);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (IsGItemValid(message)) {
		const Point position { message.x, message.y };
		const uint32_t dwSeed = SDL_SwapLE32(message.def.dwSeed);
		const uint16_t wCI = SDL_SwapLE16(message.def.wCI);
		const _item_indexes wIndx = static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx));
		if (DeltaGetItem(message, message.bLevel)) {
			bool isOnActiveLevel = GetLevelForMultiplayer(*MyPlayer) == message.bLevel;
			if ((isOnActiveLevel || message.bPnum == MyPlayerId) && message.bMaster != MyPlayerId) {
				if (message.bPnum == MyPlayerId) {
					if (!isOnActiveLevel) {
						int ii = SyncDropItem(message);
						if (ii != -1)
							InvGetItem(*MyPlayer, ii);
					} else {
						int activeItemIndex = FindGetItem(dwSeed, wIndx, wCI);
						InvGetItem(*MyPlayer, ActiveItems[activeItemIndex]);
					}
				} else {
					SyncGetItem(position, dwSeed, wIndx, wCI);
				}
			}
		} else {
			NetSendCmdGItem2(true, CMD_GETITEM, message.bMaster, message.bPnum, message);
		}
	}

	return sizeof(message);
}

size_t OnGotoAutoGetItem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLocParam1 *>(pCmd);
	const Point position { message.x, message.y };

	const uint16_t itemIdx = SDL_SwapLE16(message.wParam1);
	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position) && itemIdx < MAXITEMS + 1) {
		MakePlrPath(player, position, false);
		player.destAction = ACTION_PICKUPAITEM;
		player.destParam1 = itemIdx;
	}

	return sizeof(message);
}

size_t OnRequestAutoGetItem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdGItem *>(pCmd);

	if (gbBufferMsgs != 1 && player.isLevelOwnedByLocalClient() && IsGItemValid(message)) {
		const Point position { message.x, message.y };
		const uint32_t dwSeed = SDL_SwapLE32(message.def.dwSeed);
		const uint16_t wCI = SDL_SwapLE16(message.def.wCI);
		const _item_indexes wIndx = static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx));
		if (GetItemRecord(dwSeed, wCI, wIndx)) {
			if (FindGetItem(dwSeed, wIndx, wCI) != -1) {
				NetSendCmdGItem2(false, CMD_AGETITEM, MyPlayerId, message.bPnum, message);
				if (message.bPnum != MyPlayerId)
					SyncGetItem(position, dwSeed, wIndx, wCI);
				else
					AutoGetItem(*MyPlayer, &Items[message.bCursitem], message.bCursitem);
				SetItemRecord(dwSeed, wCI, wIndx);
			} else if (!NetSendCmdReq2(CMD_REQUESTAGITEM, *MyPlayer, message.bPnum, message)) {
				NetSendCmdExtra(message);
			}
		}
	}

	return sizeof(message);
}

size_t OnAutoGetItem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdGItem *>(pCmd);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (IsGItemValid(message)) {
		const Point position { message.x, message.y };
		if (DeltaGetItem(message, message.bLevel)) {
			uint8_t localLevel = GetLevelForMultiplayer(*MyPlayer);
			if ((localLevel == message.bLevel || message.bPnum == MyPlayerId) && message.bMaster != MyPlayerId) {
				if (message.bPnum == MyPlayerId) {
					if (localLevel != message.bLevel) {
						int ii = SyncDropItem(message);
						if (ii != -1)
							AutoGetItem(*MyPlayer, &Items[ii], ii);
					} else {
						AutoGetItem(*MyPlayer, &Items[message.bCursitem], message.bCursitem);
					}
				} else {
					SyncGetItem(position, SDL_SwapLE32(message.def.dwSeed), static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx)), SDL_SwapLE16(message.def.wCI));
				}
			}
		} else {
			NetSendCmdGItem2(true, CMD_AGETITEM, message.bMaster, message.bPnum, message);
		}
	}

	return sizeof(message);
}

size_t OnItemExtra(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdGItem *>(pCmd);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (IsGItemValid(message)) {
		DeltaGetItem(message, message.bLevel);
		if (player.isOnActiveLevel()) {
			const Point position { message.x, message.y };
			SyncGetItem(position, SDL_SwapLE32(message.def.dwSeed), static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx)), SDL_SwapLE16(message.def.wCI));
		}
	}

	return sizeof(message);
}

size_t OnPutItem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdPItem *>(pCmd);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (IsPItemValid(message, player)) {
		const Point position { message.x, message.y };
		bool isSelf = &player == MyPlayer;
		const int32_t dwSeed = SDL_SwapLE32(message.def.dwSeed);
		const uint16_t wCI = SDL_SwapLE16(message.def.wCI);
		const _item_indexes wIndx = static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx));
		if (player.isOnActiveLevel()) {
			int ii;
			if (isSelf) {
				std::optional<Point> itemTile = FindAdjacentPositionForItem(player.position.tile, GetDirection(player.position.tile, position));
				if (itemTile)
					ii = PlaceItemInWorld(std::move(ItemLimbo), *itemTile);
				else
					ii = -1;
			} else
				ii = SyncDropItem(message);
			if (ii != -1) {
				PutItemRecord(dwSeed, wCI, wIndx);
				DeltaPutItem(message, Items[ii].position, player);
				if (isSelf)
					pfile_update(true);
			}
			return sizeof(message);
		} else {
			PutItemRecord(dwSeed, wCI, wIndx);
			DeltaPutItem(message, position, player);
			if (isSelf)
				pfile_update(true);
		}
	}

	return sizeof(message);
}

size_t OnSyncPutItem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdPItem *>(pCmd);

	if (gbBufferMsgs == 1)
		SendPacket(player, &message, sizeof(message));
	else if (IsPItemValid(message, player)) {
		const int32_t dwSeed = SDL_SwapLE32(message.def.dwSeed);
		const uint16_t wCI = SDL_SwapLE16(message.def.wCI);
		const _item_indexes wIndx = static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx));
		if (player.isOnActiveLevel()) {
			int ii = SyncDropItem(message);
			if (ii != -1) {
				PutItemRecord(dwSeed, wCI, wIndx);
				DeltaPutItem(message, Items[ii].position, player);
				if (&player == MyPlayer)
					pfile_update(true);
			}
			return sizeof(message);
		} else {
			PutItemRecord(dwSeed, wCI, wIndx);
			DeltaPutItem(message, { message.x, message.y }, player);
			if (&player == MyPlayer)
				pfile_update(true);
		}
	}

	return sizeof(message);
}

size_t OnAttackTile(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLoc *>(pCmd);
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position)) {
		MakePlrPath(player, position, false);
		player.destAction = ACTION_ATTACK;
		player.destParam1 = position.x;
		player.destParam2 = position.y;
	}

	return sizeof(message);
}

size_t OnStandingAttackTile(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLoc *>(pCmd);
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position)) {
		ClrPlrPath(player);
		player.destAction = ACTION_ATTACK;
		player.destParam1 = position.x;
		player.destParam2 = position.y;
	}

	return sizeof(message);
}

size_t OnRangedAttackTile(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLoc *>(pCmd);
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position)) {
		ClrPlrPath(player);
		player.destAction = ACTION_RATTACK;
		player.destParam1 = position.x;
		player.destParam2 = position.y;
	}

	return sizeof(message);
}

bool InitNewSpell(Player &player, uint16_t wParamSpellID, uint16_t wParamSpellType, uint16_t wParamSpellFrom)
{
	wParamSpellID = SDL_SwapLE16(wParamSpellID);
	wParamSpellType = SDL_SwapLE16(wParamSpellType);
	wParamSpellFrom = SDL_SwapLE16(wParamSpellFrom);

	if (wParamSpellID > static_cast<int8_t>(SpellID::LAST))
		return false;
	auto spellID = static_cast<SpellID>(wParamSpellID);
	if (!IsValidSpell(spellID)) {
		LogError(_("{:s} has cast an invalid spell."), player._pName);
		return false;
	}
	if (leveltype == DTYPE_TOWN && !GetSpellData(spellID).isAllowedInTown()) {
		LogError(_("{:s} has cast an illegal spell."), player._pName);
		return false;
	}

	if (wParamSpellType > static_cast<uint8_t>(SpellType::Invalid))
		return false;

	if (wParamSpellFrom > INVITEM_BELT_LAST)
		return false;
	auto spellFrom = static_cast<int8_t>(wParamSpellFrom);
	if (!IsValidSpellFrom(spellFrom))
		return false;

	player.queuedSpell.spellId = spellID;
	player.queuedSpell.spellType = static_cast<SpellType>(wParamSpellType);
	player.queuedSpell.spellFrom = spellFrom;

	return true;
}

size_t OnSpellWall(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLocParam4 *>(pCmd);
	const Point position { message.x, message.y };

	if (gbBufferMsgs == 1)
		return sizeof(message);
	if (!player.isOnActiveLevel())
		return sizeof(message);
	if (!InDungeonBounds(position))
		return sizeof(message);
	const int16_t wParamDirection = SDL_SwapLE16(message.wParam3);
	if (wParamDirection > static_cast<uint16_t>(Direction::SouthEast))
		return sizeof(message);

	if (!InitNewSpell(player, message.wParam1, message.wParam2, message.wParam4))
		return sizeof(message);

	ClrPlrPath(player);
	player.destAction = ACTION_SPELLWALL;
	player.destParam1 = position.x;
	player.destParam2 = position.y;
	player.destParam3 = wParamDirection;
	player.destParam4 = player.GetSpellLevel(player.queuedSpell.spellId);

	return sizeof(message);
}

size_t OnSpellTile(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLocParam3 *>(pCmd);
	const Point position { message.x, message.y };

	if (gbBufferMsgs == 1)
		return sizeof(message);
	if (!player.isOnActiveLevel())
		return sizeof(message);
	if (!InDungeonBounds(position))
		return sizeof(message);

	if (!InitNewSpell(player, message.wParam1, message.wParam2, message.wParam3))
		return sizeof(message);

	ClrPlrPath(player);
	player.destAction = ACTION_SPELL;
	player.destParam1 = position.x;
	player.destParam2 = position.y;
	player.destParam3 = player.GetSpellLevel(player.queuedSpell.spellId);

	return sizeof(message);
}

size_t OnObjectTileAction(const TCmd &cmd, Player &player, action_id action, bool pathToObject = true)
{
	const auto &message = reinterpret_cast<const TCmdLoc &>(cmd);
	const Point position { message.x, message.y };
	const Object *object = FindObjectAtPosition(position);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && object != nullptr) {
		if (pathToObject)
			MakePlrPath(player, position, !object->_oSolidFlag && !object->_oDoorFlag);

		player.destAction = action;
		player.destParam1 = object->GetId();
	}

	return sizeof(message);
}

size_t OnAttackMonster(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t monsterIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && monsterIdx < MaxMonsters) {
		Point position = Monsters[monsterIdx].position.future;
		if (player.position.tile.WalkingDistance(position) > 1)
			MakePlrPath(player, position, false);
		player.destAction = ACTION_ATTACKMON;
		player.destParam1 = monsterIdx;
	}

	return sizeof(message);
}

size_t OnAttackPlayer(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t playerIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && playerIdx < Players.size()) {
		MakePlrPath(player, Players[playerIdx].position.future, false);
		player.destAction = ACTION_ATTACKPLR;
		player.destParam1 = playerIdx;
	}

	return sizeof(message);
}

size_t OnRangedAttackMonster(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t monsterIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && monsterIdx < MaxMonsters) {
		ClrPlrPath(player);
		player.destAction = ACTION_RATTACKMON;
		player.destParam1 = monsterIdx;
	}

	return sizeof(message);
}

size_t OnRangedAttackPlayer(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t playerIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && playerIdx < Players.size()) {
		ClrPlrPath(player);
		player.destAction = ACTION_RATTACKPLR;
		player.destParam1 = playerIdx;
	}

	return sizeof(message);
}

size_t OnSpellMonster(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam4 *>(pCmd);

	if (gbBufferMsgs == 1)
		return sizeof(message);
	if (!player.isOnActiveLevel())
		return sizeof(message);
	const uint16_t monsterIdx = SDL_SwapLE16(message.wParam1);
	if (monsterIdx >= MaxMonsters)
		return sizeof(message);

	if (!InitNewSpell(player, message.wParam2, message.wParam3, message.wParam4))
		return sizeof(message);

	ClrPlrPath(player);
	player.destAction = ACTION_SPELLMON;
	player.destParam1 = monsterIdx;
	player.destParam2 = player.GetSpellLevel(player.queuedSpell.spellId);

	return sizeof(message);
}

size_t OnSpellPlayer(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam4 *>(pCmd);

	if (gbBufferMsgs == 1)
		return sizeof(message);
	if (!player.isOnActiveLevel())
		return sizeof(message);
	const uint16_t playerIdx = SDL_SwapLE16(message.wParam1);
	if (playerIdx >= Players.size())
		return sizeof(message);

	if (!InitNewSpell(player, message.wParam2, message.wParam3, message.wParam4))
		return sizeof(message);

	ClrPlrPath(player);
	player.destAction = ACTION_SPELLPLR;
	player.destParam1 = playerIdx;
	player.destParam2 = player.GetSpellLevel(player.queuedSpell.spellId);

	return sizeof(message);
}

size_t OnKnockback(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t monsterIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && monsterIdx < MaxMonsters) {
		Monster &monster = Monsters[monsterIdx];
		M_GetKnockback(monster, player.position.tile);
		M_StartHit(monster, player, 0);
	}

	return sizeof(message);
}

size_t OnResurrect(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t playerIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (playerIdx < Players.size()) {
		DoResurrect(player, Players[playerIdx]);
		if (&player == MyPlayer)
			pfile_update(true);
	}

	return sizeof(message);
}

size_t OnHealOther(const TCmd *pCmd, const Player &caster)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t playerIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (caster.isOnActiveLevel() && playerIdx < Players.size()) {
			DoHealOther(caster, Players[playerIdx]);
		}
	}

	return sizeof(message);
}

size_t OnTalkXY(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLocParam1 *>(pCmd);
	const Point position { message.x, message.y };
	const uint16_t townerIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position) && townerIdx < NUM_TOWNERS) {
		MakePlrPath(player, position, false);
		player.destAction = ACTION_TALK;
		player.destParam1 = townerIdx;
	}

	return sizeof(message);
}

size_t OnNewLevel(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam2 *>(pCmd);
	const uint16_t eventIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (&player != MyPlayer) {
		if (eventIdx < WM_FIRST || eventIdx > WM_LAST)
			return sizeof(message);

		auto mode = static_cast<interface_mode>(eventIdx);

		const auto levelId = static_cast<uint8_t>(SDL_SwapLE16(message.wParam2));
		if (!IsValidLevel(levelId, mode == WM_DIABSETLVL)) {
			return sizeof(message);
		}

		StartNewLvl(player, mode, levelId);
	}

	return sizeof(message);
}

size_t OnWarp(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t portalIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (portalIdx < MAXPORTAL) {
		StartWarpLvl(player, portalIdx);
	}

	return sizeof(message);
}

size_t OnMonstDeath(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLocParam1 *>(pCmd);
	const Point position { message.x, message.y };
	const uint16_t monsterIdx = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (&player != MyPlayer && InDungeonBounds(position) && monsterIdx < MaxMonsters) {
			Monster &monster = Monsters[monsterIdx];
			if (player.isOnActiveLevel())
				M_SyncStartKill(monster, position, player);
			delta_kill_monster(monster, position, player);
		}
	} else {
		SendPacket(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnKillGolem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLoc *>(pCmd);
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1) {
		if (&player != MyPlayer && InDungeonBounds(position)) {
			Monster &monster = Monsters[player.getId()];
			if (player.isOnActiveLevel())
				M_SyncStartKill(monster, position, player);
			delta_kill_monster(monster, position, player); // BUGFIX: should be p->wParam1, plrlevel will be incorrect if golem is killed because player changed levels
		}
	} else {
		SendPacket(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnAwakeGolem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdGolem *>(pCmd);
	const Point position { message._mx, message._my };

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (InDungeonBounds(position)) {
		if (!player.isOnActiveLevel()) {
			DeltaSyncGolem(message, player, message._currlevel);
		} else if (&player != MyPlayer) {
			// Check if this player already has an active golem
			for (auto &missile : Missiles) {
				if (missile._mitype == MissileID::Golem && &Players[missile._misource] == &player) {
					return sizeof(message);
				}
			}

			AddMissile(player.position.tile, position, message._mdir, MissileID::Golem, TARGET_MONSTERS, player, 0, 1);
		}
	}

	return sizeof(message);
}

size_t OnMonstDamage(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdMonDamage *>(pCmd);
	const uint16_t monsterIdx = SDL_SwapLE16(message.wMon);

	if (gbBufferMsgs != 1) {
		if (&player != MyPlayer) {
			if (player.isOnActiveLevel() && monsterIdx < MaxMonsters) {
				Monster &monster = Monsters[monsterIdx];
				monster.tag(player);
				if (monster.hitPoints > 0) {
					monster.hitPoints -= SDL_SwapLE32(message.dwDam);
					if ((monster.hitPoints >> 6) < 1)
						monster.hitPoints = 1 << 6;
					delta_monster_hp(monster, player);
				}
			}
		}
	} else {
		SendPacket(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnPlayerDeath(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const DeathReason deathReason = static_cast<DeathReason>(SDL_SwapLE16(message.wParam1));

	if (gbBufferMsgs != 1) {
		if (&player != MyPlayer)
			StartPlayerKill(player, deathReason);
		else
			pfile_update(true);
	} else {
		SendPacket(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnPlayerDamage(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdDamage *>(pCmd);
	const uint32_t damage = SDL_SwapLE32(message.dwDam);

	Player &target = Players[message.bPlr];
	if (&target == MyPlayer && leveltype != DTYPE_TOWN && gbBufferMsgs != 1) {
		if (player.isOnActiveLevel() && damage <= 192000 && target._pHitPoints >> 6 > 0) {
			ApplyPlrDamage(message.damageType, target, 0, 0, damage, DeathReason::Player);
		}
	}

	return sizeof(message);
}

size_t OnOperateObject(const TCmd &pCmd, Player &player)
{
	const auto &message = reinterpret_cast<const TCmdLoc &>(pCmd);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else {
		WorldTilePosition position { message.x, message.y };
		assert(InDungeonBounds(position));
		if (player.isOnActiveLevel()) {
			Object *object = FindObjectAtPosition(position);
			if (object != nullptr)
				SyncOpObject(player, message.bCmd, *object);
		}
		DeltaSyncObject(position, message.bCmd, player);
	}

	return sizeof(message);
}

size_t OnBreakObject(const TCmd &pCmd, Player &player)
{
	const auto &message = reinterpret_cast<const TCmdLoc &>(pCmd);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else {
		WorldTilePosition position { message.x, message.y };
		assert(InDungeonBounds(position));
		if (player.isOnActiveLevel()) {
			Object *object = FindObjectAtPosition(position);
			if (object != nullptr)
				SyncBreakObj(player, *object);
		}
		DeltaSyncObject(position, CMD_BREAKOBJ, player);
	}

	return sizeof(message);
}

size_t OnChangePlayerItems(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdChItem *>(pCmd);

	if (message.bLoc >= NUM_INVLOC)
		return sizeof(message);

	auto bodyLocation = static_cast<inv_body_loc>(message.bLoc);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (&player != MyPlayer && IsItemAvailable(static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx)))) {
		Item &item = player.InvBody[message.bLoc];
		item = {};
		RecreateItem(player, message, item);
		CheckInvSwap(player, bodyLocation);
	}

	player.ReadySpellFromEquipment(bodyLocation, message.forceSpell);

	return sizeof(message);
}

size_t OnDeletePlayerItems(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdDelItem *>(pCmd);

	if (gbBufferMsgs != 1) {
		if (&player != MyPlayer && message.bLoc < NUM_INVLOC)
			inv_update_rem_item(player, static_cast<inv_body_loc>(message.bLoc));
	} else {
		SendPacket(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnChangeInventoryItems(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdChItem *>(pCmd);

	if (message.bLoc >= InventoryGridCells)
		return sizeof(message);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (&player != MyPlayer && IsItemAvailable(static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx)))) {
		Item item {};
		RecreateItem(player, message, item);
		CheckInvSwap(player, item, message.bLoc);
	}

	return sizeof(message);
}

size_t OnDeleteInventoryItems(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t invGridIndex = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (&player != MyPlayer && invGridIndex < InventoryGridCells) {
		CheckInvRemove(player, invGridIndex);
	}

	return sizeof(message);
}

size_t OnChangeBeltItems(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdChItem *>(pCmd);

	if (message.bLoc >= MaxBeltItems)
		return sizeof(message);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (&player != MyPlayer && IsItemAvailable(static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx)))) {
		Item &item = player.SpdList[message.bLoc];
		item = {};
		RecreateItem(player, message, item);
	}

	return sizeof(message);
}

size_t OnDeleteBeltItems(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t spdBarIndex = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (&player != MyPlayer && spdBarIndex < MaxBeltItems) {
		player.RemoveSpdBarItem(spdBarIndex);
	}

	return sizeof(message);
}

size_t OnPlayerLevel(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t playerLevel = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (playerLevel <= player.getMaxCharacterLevel() && &player != MyPlayer)
			player.setCharacterLevel(static_cast<uint8_t>(playerLevel));
	} else {
		SendPacket(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnDropItem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdPItem *>(pCmd);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (IsPItemValid(message, player)) {
		DeltaPutItem(message, { message.x, message.y }, player);
	}

	return sizeof(message);
}

size_t OnSpawnItem(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdPItem *>(pCmd);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (IsPItemValid(message, player)) {
		if (player.isOnActiveLevel() && &player != MyPlayer) {
			SyncDropItem(message);
		}
		PutItemRecord(SDL_SwapLE32(message.def.dwSeed), SDL_SwapLE16(message.def.wCI), static_cast<_item_indexes>(SDL_SwapLE16(message.def.wIndx)));
		DeltaPutItem(message, { message.x, message.y }, player);
	}

	return sizeof(message);
}

size_t OnSendPlayerInfo(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdPlrInfoHdr *>(pCmd);
	const uint16_t wBytes = SDL_SwapLE16(message.wBytes);

	if (gbBufferMsgs == 1)
		SendPacket(player, &message, wBytes + sizeof(message));
	else
		recv_plrinfo(player, message, message.bCmd == CMD_ACK_PLRINFO);

	return wBytes + sizeof(message);
}

size_t OnPlayerJoinLevel(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLocParam2 *>(pCmd);
	const Point position { message.x, message.y };

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
		return sizeof(message);
	}

	const uint8_t playerLevel = static_cast<uint8_t>(SDL_SwapLE16(message.wParam1));
	bool isSetLevel = message.wParam2 != 0;
	if (!IsValidLevel(playerLevel, isSetLevel) || !InDungeonBounds(position)) {
		return sizeof(message);
	}

	player._pLvlChanging = false;
	if (player._pName[0] != '\0' && !player.plractive) {
		ResetPlayerGFX(player);
		player.plractive = true;
		gbActivePlayers++;
		EventPlrMsg(fmt::format(fmt::runtime(_("Player '{:s}' (level {:d}) just joined the game")), player._pName, player.getCharacterLevel()));
	}

	if (player.plractive && &player != MyPlayer) {
		player.position.tile = position;
		SetPlayerOld(player);
		if (isSetLevel)
			player.setLevel(static_cast<_setlevels>(playerLevel));
		else
			player.setLevel(playerLevel);
		ResetPlayerGFX(player);
		if (player.isOnActiveLevel()) {
			SyncInitPlr(player);
			if ((player._pHitPoints >> 6) > 0) {
				StartStand(player, Direction::South);
			} else {
				player._pgfxnum &= ~0xFU;
				player._pmode = PM_DEATH;
				NewPlrAnim(player, player_graphic::Death, Direction::South);
				player.AnimInfo.currentFrame = player.AnimInfo.numberOfFrames - 2;
				dFlags[player.position.tile.x][player.position.tile.y] |= DungeonFlag::DeadPlayer;
			}

			ActivateVision(player.position.tile, player._pLightRad, player.getId());
		}
	}

	return sizeof(message);
}

size_t OnActivatePortal(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdLocParam3 *>(pCmd);
	const Point position { message.x, message.y };
	const uint8_t level = static_cast<uint8_t>(SDL_SwapLE16(message.wParam1));
	const uint16_t dungeonTypeIdx = SDL_SwapLE16(message.wParam2);
	const bool isSetLevel = message.wParam3 != 0;

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else if (InDungeonBounds(position) && IsValidLevel(level, isSetLevel) && dungeonTypeIdx <= DTYPE_LAST) {
		auto dungeonType = static_cast<dungeon_type>(dungeonTypeIdx);

		ActivatePortal(player, position, level, dungeonType, isSetLevel);
		if (&player != MyPlayer) {
			if (leveltype == DTYPE_TOWN) {
				AddPortalInTown(player);
			} else if (player.isOnActiveLevel()) {
				bool addPortal = true;
				for (auto &missile : Missiles) {
					if (missile._mitype == MissileID::TownPortal && &Players[missile._misource] == &player) {
						addPortal = false;
						break;
					}
				}
				if (addPortal) {
					AddPortalMissile(player, position, false);
				}
			} else {
				RemovePortalMissile(player);
			}
		}
		DeltaOpenPortal(player.getId(), position, level, dungeonType, isSetLevel);
	}

	return sizeof(message);
}

size_t OnDeactivatePortal(const TCmd *pCmd, Player &player)
{
	if (gbBufferMsgs == 1) {
		SendPacket(player, pCmd, sizeof(*pCmd));
	} else {
		if (PortalOnLevel(player))
			RemovePortalMissile(player);
		DeactivatePortal(player);
		delta_close_portal(player);
	}

	return sizeof(*pCmd);
}

size_t OnRestartTown(const TCmd *pCmd, Player &player)
{
	if (gbBufferMsgs == 1) {
		SendPacket(player, pCmd, sizeof(*pCmd));
	} else {
		if (&player == MyPlayer) {
			MyPlayerIsDead = false;
			gamemenu_off();
		}
		RestartTownLvl(player);
	}

	return sizeof(*pCmd);
}

size_t OnSetStrength(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t value = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (value <= 750 && &player != MyPlayer)
			SetPlrStr(player, value);
	} else {
		SendPacket(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnSetDexterity(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t value = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (value <= 750 && &player != MyPlayer)
			SetPlrDex(player, value);
	} else {
		SendPacket(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnSetMagic(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t value = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (value <= 750 && &player != MyPlayer)
			SetPlrMag(player, value);
	} else {
		SendPacket(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnSetVitality(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);
	const uint16_t value = SDL_SwapLE16(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (value <= 750 && &player != MyPlayer)
			SetPlrVit(player, value);
	} else {
		SendPacket(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnString(const TCmd *pCmd, Player &player)
{
	auto *p = (TCmdString *)pCmd;

	size_t len = strlen(p->str);
	if (gbBufferMsgs == 0)
		SendPlrMsg(player, p->str);

	return len + 2; // length of string + nul terminator + sizeof(p->bCmd)
}

size_t OnFriendlyMode(const TCmd *pCmd, Player &player) // NOLINT(misc-unused-parameters)
{
	player.friendlyMode = !player.friendlyMode;
	RedrawEverything();
	return sizeof(*pCmd);
}

size_t OnSyncQuest(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdQuest *>(pCmd);

	if (gbBufferMsgs == 1) {
		SendPacket(player, &message, sizeof(message));
	} else {
		if (&player != MyPlayer && message.q < MAXQUESTS && message.qstate <= QUEST_HIVE_DONE)
			SetMultiQuest(message.q, message.qstate, message.qlog != 0, message.qvar1, message.qvar2, message.qmsg);
	}

	return sizeof(message);
}

size_t OnCheatExperience(const TCmd *pCmd, Player &player) // NOLINT(misc-unused-parameters)
{
#ifdef _DEBUG
	if (gbBufferMsgs == 1)
		SendPacket(player, pCmd, sizeof(*pCmd));
	else if (!player.isMaxCharacterLevel()) {
		player._pExperience = player.getNextExperienceThreshold();
		if (*GetOptions().Gameplay.experienceBar) {
			RedrawEverything();
		}
		NextPlrLevel(player);
	}
#endif
	return sizeof(*pCmd);
}

size_t OnChangeSpellLevel(const TCmd *pCmd, Player &player) // NOLINT(misc-unused-parameters)
{
	const auto &message = *reinterpret_cast<const TCmdParam2 *>(pCmd);
	const SpellID spellID = static_cast<SpellID>(SDL_SwapLE16(message.wParam1));
	const uint8_t spellLevel = std::min(static_cast<uint8_t>(SDL_SwapLE16(message.wParam2)), MaxSpellLevel);

	if (gbBufferMsgs == 1) {
		SendPacket(player, pCmd, sizeof(*pCmd));
	} else {
		player._pMemSpells |= GetSpellBitmask(spellID);
		player._pSplLvl[static_cast<size_t>(spellID)] = spellLevel;
	}

	return sizeof(message);
}

size_t OnDebug(const TCmd *pCmd)
{
	return sizeof(*pCmd);
}

size_t OnSetShield(const TCmd *pCmd, Player &player)
{
	if (gbBufferMsgs != 1)
		player.pManaShield = true;

	return sizeof(*pCmd);
}

size_t OnRemoveShield(const TCmd *pCmd, Player &player)
{
	if (gbBufferMsgs != 1)
		player.pManaShield = false;

	return sizeof(*pCmd);
}

size_t OnSetReflect(const TCmd *pCmd, Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdParam1 *>(pCmd);

	if (gbBufferMsgs != 1)
		player.wReflections = SDL_SwapLE16(message.wParam1);

	return sizeof(message);
}

size_t OnNakrul(const TCmd *pCmd)
{
	if (gbBufferMsgs != 1) {
		if (currlevel == 24) {
			PlaySfxLoc(SfxID::CryptDoorOpen, { UberRow, UberCol });
			SyncNakrulRoom();
		}
		IsUberRoomOpened = true;
		Quests[Q_NAKRUL]._qactive = QUEST_DONE;
		WeakenNaKrul();
	}
	return sizeof(*pCmd);
}

size_t OnOpenHive(const TCmd *pCmd, Player &player)
{
	if (gbBufferMsgs != 1) {
		AddMissile({ 0, 0 }, { 0, 0 }, Direction::South, MissileID::OpenNest, TARGET_MONSTERS, player, 0, 0);
		TownOpenHive();
		InitTownTriggers();
	}

	return sizeof(*pCmd);
}

size_t OnOpenGrave(const TCmd *pCmd)
{
	if (gbBufferMsgs != 1) {
		TownOpenGrave();
		InitTownTriggers();
		if (leveltype == DTYPE_TOWN)
			PlaySFX(SfxID::Sarcophagus);
	}
	return sizeof(*pCmd);
}

size_t OnSpawnMonster(const TCmd *pCmd, const Player &player)
{
	const auto &message = *reinterpret_cast<const TCmdSpawnMonster *>(pCmd);
	if (gbBufferMsgs == 1)
		return sizeof(message);

	const WorldTilePosition position { message.x, message.y };

	size_t typeIndex = static_cast<size_t>(SDL_SwapLE16(message.typeIndex));
	size_t monsterId = static_cast<size_t>(SDL_SwapLE16(message.monsterId));

	DLevel &deltaLevel = GetDeltaLevel(player);

	deltaLevel.spawnedMonsters[monsterId] = { typeIndex, message.seed };
	// Override old monster delta information
	auto &deltaMonster = deltaLevel.monster[monsterId];
	deltaMonster.position = position;
	deltaMonster.hitPoints = -1;
	deltaMonster._menemy = 0;
	deltaMonster._mactive = 0;

	if (player.isOnActiveLevel() && &player != MyPlayer)
		InitializeSpawnedMonster(position, message.dir, typeIndex, monsterId, message.seed);
	return sizeof(message);
}

} // namespace

void PrepareItemForNetwork(const Item &item, TItem &messageItem)
{
	messageItem.bId = item._iIdentified ? 1 : 0;
	messageItem.bDur = item._iDurability;
	messageItem.bMDur = item._iMaxDur;
	messageItem.bCh = item._iCharges;
	messageItem.bMCh = item._iMaxCharges;
	messageItem.wValue = SDL_SwapLE16(item._ivalue);
	messageItem.wToHit = SDL_SwapLE16(item._iPLToHit);
	messageItem.wMaxDam = SDL_SwapLE16(item._iMaxDam);
	messageItem.dwBuff = SDL_SwapLE32(item.dwBuff);
}

void PrepareEarForNetwork(const Item &item, TEar &ear)
{
	ear.bCursval = item._ivalue | ((item._iCurs - ICURS_EAR_SORCERER) << 6);
	CopyUtf8(ear.heroname, item._iIName, sizeof(ear.heroname));
}

void RecreateItem(const Player &player, const TItem &messageItem, Item &item)
{
	const uint32_t dwBuff = SDL_SwapLE32(messageItem.dwBuff);
	RecreateItem(player, item,
	    static_cast<_item_indexes>(SDL_SwapLE16(messageItem.wIndx)), SDL_SwapLE16(messageItem.wCI),
	    SDL_SwapLE32(messageItem.dwSeed), SDL_SwapLE16(messageItem.wValue), dwBuff);
	if (messageItem.bId != 0)
		item._iIdentified = true;
	item._iMaxDur = messageItem.bMDur;
	item._iDurability = ClampDurability(item, messageItem.bDur);
	item._iMaxCharges = std::clamp<int>(messageItem.bMCh, 0, item._iMaxCharges);
	item._iCharges = std::clamp<int>(messageItem.bCh, 0, item._iMaxCharges);
	if (gbIsHellfire) {
		item._iPLToHit = ClampToHit(item, static_cast<uint8_t>(SDL_SwapLE16(messageItem.wToHit)));
		item._iMaxDam = ClampMaxDam(item, static_cast<uint8_t>(SDL_SwapLE16(messageItem.wMaxDam)));
	}
}

void ClearLastSentPlayerCmd()
{
	lastSentPlayerCmd = {};
}

void msg_send_drop_pkt(uint8_t pnum, int reason)
{
	TFakeDropPlr cmd;

	cmd.dwReason = SDL_SwapLE32(reason);
	cmd.bCmd = FAKE_CMD_DROPID;
	cmd.bPlr = pnum;
	SendPacket(pnum, &cmd, sizeof(cmd));
}

bool msg_wait_resync()
{
	bool success;

	GetNextPacket();
	sgbDeltaChunks = 0;
	sgnCurrMegaPlayer = -1;
	sgbRecvCmd = CMD_DLEVEL_END;
	gbBufferMsgs = 1;
	sgdwOwnerWait = SDL_GetTicks();
	success = UiProgressDialog(WaitForTurns);
	gbBufferMsgs = 0;
	if (!success) {
		FreePackets();
		return false;
	}

	if (gbGameDestroyed) {
		UiErrorOkDialog(PROJECT_NAME, _("The game ended"), /*error=*/false);
		FreePackets();
		return false;
	}

	if (sgbDeltaChunks != MAX_CHUNKS) {
		UiErrorOkDialog(PROJECT_NAME, _("Unable to get level data"), /*error=*/false);
		FreePackets();
		return false;
	}

	return true;
}

void run_delta_info()
{
	if (!gbIsMultiplayer)
		return;

	gbBufferMsgs = 2;
	PrePacket();
	gbBufferMsgs = 0;
	FreePackets();
}

void DeltaExportData(uint8_t pnum)
{
	for (const auto &[levelNum, deltaLevel] : DeltaLevels) {
		const size_t bufferSize = 1U                                                      /* marker byte, always 0 */
		    + sizeof(uint8_t)                                                             /* level id */
		    + sizeof(deltaLevel.item)                                                     /* items spawned during dungeon generation which have been picked up, and items dropped by a player during a game */
		    + sizeof(uint8_t)                                                             /* count of object interactions which caused a state change since dungeon generation */
		    + (sizeof(WorldTilePosition) + sizeof(DObjectStr)) * deltaLevel.object.size() /* location/action pairs for the object interactions */
		    + sizeof(deltaLevel.monster)                                                  /* latest monster state */
		    + sizeof(uint16_t)                                                            /* spanwned monster count */
		    + (sizeof(uint16_t) + sizeof(DSpawnedMonster)) * MaxMonsters;                 /* spanwned monsters */
		std::unique_ptr<std::byte[]> dst { new std::byte[bufferSize] };

		std::byte *dstEnd = &dst.get()[1];
		*dstEnd = static_cast<std::byte>(levelNum);
		dstEnd += sizeof(uint8_t);
		dstEnd = DeltaExportItem(dstEnd, deltaLevel.item);
		dstEnd = DeltaExportObject(dstEnd, deltaLevel.object);
		dstEnd = DeltaExportMonster(dstEnd, deltaLevel.monster);
		dstEnd = DeltaExportSpawnedMonsters(dstEnd, deltaLevel.spawnedMonsters);
		uint32_t size = CompressData(dst.get(), dstEnd);
		multi_send_zero_packet(pnum, CMD_DLEVEL, dst.get(), size);
	}

	std::byte dst[sizeof(DJunk) + 1];
	std::byte *dstEnd = &dst[1];
	dstEnd = DeltaExportJunk(dstEnd);
	uint32_t size = CompressData(dst, dstEnd);
	multi_send_zero_packet(pnum, CMD_DLEVEL_JUNK, dst, size);

	std::byte src[1] = { static_cast<std::byte>(0) };
	multi_send_zero_packet(pnum, CMD_DLEVEL_END, src, 1);
}

void delta_init()
{
	memset(&sgJunk, 0xFF, sizeof(sgJunk));
	DeltaLevels.clear();
	LocalLevels.clear();
}

void DeltaClearLevel(uint8_t level)
{
	DeltaLevels.erase(level);
	LocalLevels.erase(level);
}

void delta_kill_monster(const Monster &monster, Point position, const Player &player)
{
	if (!gbIsMultiplayer)
		return;

	DMonsterStr *pD = &GetDeltaLevel(player).monster[monster.getId()];
	pD->position = position;
	pD->hitPoints = 0;
}

void delta_monster_hp(const Monster &monster, const Player &player)
{
	if (!gbIsMultiplayer)
		return;

	DMonsterStr *pD = &GetDeltaLevel(player).monster[monster.getId()];
	if (pD->hitPoints > monster.hitPoints)
		pD->hitPoints = monster.hitPoints;
}

void delta_sync_monster(const TSyncMonster &monsterSync, uint8_t level)
{
	if (!gbIsMultiplayer)
		return;

	assert(level <= MAX_MULTIPLAYERLEVELS);

	DMonsterStr &monster = GetDeltaLevel(level).monster[monsterSync._mndx];
	if (monster.hitPoints == 0)
		return;

	monster.position.x = monsterSync._mx;
	monster.position.y = monsterSync._my;
	monster._mactive = UINT8_MAX;
	monster._menemy = monsterSync._menemy;
	monster.hitPoints = SDL_SwapLE32(monsterSync._mhitpoints);
	monster.mWhoHit = monsterSync.mWhoHit;
}

void DeltaSyncJunk()
{
	for (int i = 0; i < MAXPORTAL; i++) {
		if (sgJunk.portal[i].x == 0xFF) {
			SetPortalStats(i, false, { 0, 0 }, 0, DTYPE_TOWN, false);
		} else {
			SetPortalStats(
			    i,
			    true,
			    { sgJunk.portal[i].x, sgJunk.portal[i].y },
			    sgJunk.portal[i].level,
			    (dungeon_type)sgJunk.portal[i].ltype,
			    sgJunk.portal[i].setlvl);
		}
	}

	int q = 0;
	for (auto &quest : Quests) {
		if (QuestsData[quest._qidx].isSinglePlayerOnly && UseMultiplayerQuests()) {
			continue;
		}
		if (sgJunk.quests[q].qstate != QUEST_INVALID) {
			quest._qlog = sgJunk.quests[q].qlog != 0;
			quest._qactive = sgJunk.quests[q].qstate;
			quest._qvar1 = sgJunk.quests[q].qvar1;
			quest._qvar2 = sgJunk.quests[q].qvar2;
			quest._qmsg = static_cast<_speech_id>(sgJunk.quests[q].qmsg);
		}
		q++;
	}
}

void DeltaAddItem(int ii)
{
	if (!gbIsMultiplayer)
		return;

	uint8_t localLevel = GetLevelForMultiplayer(*MyPlayer);
	DLevel &deltaLevel = GetDeltaLevel(localLevel);

	for (const TCmdPItem &item : deltaLevel.item) {
		if (item.bCmd != CMD_INVALID
		    && static_cast<_item_indexes>(SDL_SwapLE16(item.def.wIndx)) == Items[ii].IDidx
		    && SDL_SwapLE16(item.def.wCI) == Items[ii]._iCreateInfo
		    && static_cast<uint32_t>(SDL_SwapLE32(item.def.dwSeed)) == Items[ii]._iSeed
		    && IsAnyOf(item.bCmd, TCmdPItem::PickedUpItem, TCmdPItem::FloorItem)) {
			return;
		}
	}

	for (TCmdPItem &delta : deltaLevel.item) {
		if (delta.bCmd != CMD_INVALID)
			continue;

		delta.bCmd = TCmdPItem::FloorItem;
		delta.x = Items[ii].position.x;
		delta.y = Items[ii].position.y;
		PrepareItemForNetwork(Items[ii], delta);
		return;
	}
}

void DeltaSaveLevel()
{
	if (!gbIsMultiplayer)
		return;

	for (Player &player : Players) {
		if (&player != MyPlayer)
			ResetPlayerGFX(player);
	}
	uint8_t localLevel;
	if (setlevel) {
		localLevel = GetLevelForMultiplayer(static_cast<uint8_t>(setlvlnum), setlevel);
		MyPlayer->_pSLvlVisited[static_cast<uint8_t>(setlvlnum)] = true;
	} else {
		localLevel = GetLevelForMultiplayer(currlevel, setlevel);
		MyPlayer->_pLvlVisited[currlevel] = true;
	}
	DeltaLeaveSync(localLevel);
}

uint8_t GetLevelForMultiplayer(const Player &player)
{
	return GetLevelForMultiplayer(player.plrlevel, player.plrIsOnSetLevel);
}

bool IsValidLevelForMultiplayer(uint8_t level)
{
	return level <= MAX_MULTIPLAYERLEVELS;
}

bool IsValidLevel(uint8_t level, bool isSetLevel)
{
	if (isSetLevel)
		return level <= SL_LAST;
	return level < NUMLEVELS;
}

void DeltaLoadLevel()
{
	if (!gbIsMultiplayer)
		return;

	uint8_t localLevel = GetLevelForMultiplayer(*MyPlayer);
	DLevel &deltaLevel = GetDeltaLevel(localLevel);
	if (leveltype != DTYPE_TOWN) {
		for (auto &deltaSpawnedMonster : deltaLevel.spawnedMonsters) {
			LoadDeltaSpawnedMonster(deltaSpawnedMonster.second.typeIndex, deltaSpawnedMonster.first, deltaSpawnedMonster.second.seed);
			assert(deltaLevel.monster[deltaSpawnedMonster.first].position.x != 0xFF);
		}
		for (size_t i = 0; i < MaxMonsters; i++) {
			if (deltaLevel.monster[i].position.x == 0xFF)
				continue;

			Monster &monster = Monsters[i];
			M_ClearSquares(monster);
			{
				const WorldTilePosition position = deltaLevel.monster[i].position;
				monster.position.tile = position;
				monster.position.old = position;
				monster.position.future = position;
				if (monster.lightId != NO_LIGHT)
					ChangeLightXY(monster.lightId, position);
			}
			if (deltaLevel.monster[i].hitPoints != -1) {
				monster.hitPoints = deltaLevel.monster[i].hitPoints;
				monster.whoHit = deltaLevel.monster[i].mWhoHit;
			}
			if (deltaLevel.monster[i].hitPoints == 0) {
				M_ClearSquares(monster);
				if (monster.ai != MonsterAIID::Diablo) {
					if (monster.isUnique()) {
						AddCorpse(monster.position.tile, monster.corpseId, monster.direction);
					} else {
						AddCorpse(monster.position.tile, monster.type().corpseId, monster.direction);
					}
				}
				monster.isInvalid = true;
				M_UpdateRelations(monster);
			} else {
				decode_enemy(monster, deltaLevel.monster[i]._menemy);
				if (monster.position.tile != Point { 0, 0 } && monster.position.tile != GolemHoldingCell)
					monster.occupyTile(monster.position.tile, false);
				if (monster.type().type == MT_GOLEM) {
					GolumAi(monster);
					monster.flags |= (MFLAG_TARGETS_MONSTER | MFLAG_GOLEM);
				} else {
					M_StartStand(monster, monster.direction);
				}
				monster.activeForTicks = deltaLevel.monster[i]._mactive;
			}
		}
		auto localLevelIt = LocalLevels.find(localLevel);
		if (localLevelIt != LocalLevels.end())
			memcpy(AutomapView, &localLevelIt->second, sizeof(AutomapView));
		else
			memset(AutomapView, 0, sizeof(AutomapView));
	}

	if (leveltype != DTYPE_TOWN) {
		for (auto it = deltaLevel.object.begin(); it != deltaLevel.object.end();) {
			Object *object = FindObjectAtPosition(it->first);
			if (object == nullptr) {
				it = deltaLevel.object.erase(it);
				continue;
			}

			switch (it->second.bCmd) {
			case CMD_OPENDOOR:
			case CMD_OPERATEOBJ:
				DeltaSyncOpObject(*object);
				it++;
				break;
			case CMD_CLOSEDOOR:
				DeltaSyncCloseObj(*object);
				it++;
				break;
			case CMD_BREAKOBJ:
				DeltaSyncBreakObj(*object);
				it++;
				break;
			default:
				it = deltaLevel.object.erase(it); // discard invalid commands
				break;
			}
		}

		for (int i = 0; i < ActiveObjectCount; i++) {
			Object &object = Objects[ActiveObjects[i]];
			if (object.IsTrap()) {
				UpdateTrapState(object);
			}
		}
	}

	for (int i = 0; i < MAXITEMS; i++) {
		if (deltaLevel.item[i].bCmd == CMD_INVALID)
			continue;

		if (deltaLevel.item[i].bCmd == TCmdPItem::PickedUpItem) {
			int activeItemIndex = FindGetItem(
			    SDL_SwapLE32(deltaLevel.item[i].def.dwSeed),
			    static_cast<_item_indexes>(SDL_SwapLE16(deltaLevel.item[i].def.wIndx)),
			    SDL_SwapLE16(deltaLevel.item[i].def.wCI));
			if (activeItemIndex != -1) {
				const auto &position = Items[ActiveItems[activeItemIndex]].position;
				if (dItem[position.x][position.y] == ActiveItems[activeItemIndex] + 1)
					dItem[position.x][position.y] = 0;
				DeleteItem(activeItemIndex);
			}
		}
		if (deltaLevel.item[i].bCmd == TCmdPItem::DroppedItem) {
			int ii = AllocateItem();
			auto &item = Items[ii];
			RecreateItem(*MyPlayer, deltaLevel.item[i], item);

			int x = deltaLevel.item[i].x;
			int y = deltaLevel.item[i].y;
			item.position = GetItemPosition({ x, y });
			dItem[item.position.x][item.position.y] = ii + 1;
			RespawnItem(Items[ii], false);
		}
	}
}

void NetSendCmd(bool bHiPri, _cmd_id bCmd)
{
	TCmd cmd;

	cmd.bCmd = bCmd;
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdGolem(uint8_t mx, uint8_t my, Direction dir, uint8_t menemy, int hp, uint8_t cl)
{
	TCmdGolem cmd;

	cmd.bCmd = CMD_AWAKEGOLEM;
	cmd._mx = mx;
	cmd._my = my;
	cmd._mdir = dir;
	cmd._menemy = menemy;
	cmd._mhitpoints = hp;
	cmd._currlevel = cl;
	NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdSpawnMonster(Point position, Direction dir, uint16_t typeIndex, uint16_t monsterId, uint32_t seed)
{
	TCmdSpawnMonster cmd;

	cmd.bCmd = CMD_SPAWNMONSTER;
	cmd.x = position.x;
	cmd.y = position.y;
	cmd.dir = dir;
	cmd.typeIndex = SDL_SwapLE16(typeIndex);
	cmd.monsterId = SDL_SwapLE16(monsterId);
	cmd.seed = SDL_SwapLE32(seed);
	NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdLoc(uint8_t playerId, bool bHiPri, _cmd_id bCmd, Point position)
{
	if (playerId == MyPlayerId && WasPlayerCmdAlreadyRequested(bCmd, position))
		return;

	TCmdLoc cmd;

	cmd.bCmd = bCmd;
	cmd.x = position.x;
	cmd.y = position.y;
	if (bHiPri)
		NetSendHiPri(playerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(playerId, (std::byte *)&cmd, sizeof(cmd));

	MyPlayer->UpdatePreviewCelSprite(bCmd, position, 0, 0);
}

void NetSendCmdLocParam1(bool bHiPri, _cmd_id bCmd, Point position, uint16_t wParam1)
{
	if (WasPlayerCmdAlreadyRequested(bCmd, position, wParam1))
		return;

	TCmdLocParam1 cmd;

	cmd.bCmd = bCmd;
	cmd.x = position.x;
	cmd.y = position.y;
	cmd.wParam1 = SDL_SwapLE16(wParam1);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));

	MyPlayer->UpdatePreviewCelSprite(bCmd, position, wParam1, 0);
}

void NetSendCmdLocParam2(bool bHiPri, _cmd_id bCmd, Point position, uint16_t wParam1, uint16_t wParam2)
{
	if (WasPlayerCmdAlreadyRequested(bCmd, position, wParam1, wParam2))
		return;

	TCmdLocParam2 cmd;

	cmd.bCmd = bCmd;
	cmd.x = position.x;
	cmd.y = position.y;
	cmd.wParam1 = SDL_SwapLE16(wParam1);
	cmd.wParam2 = SDL_SwapLE16(wParam2);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));

	MyPlayer->UpdatePreviewCelSprite(bCmd, position, wParam1, wParam2);
}

void NetSendCmdLocParam3(bool bHiPri, _cmd_id bCmd, Point position, uint16_t wParam1, uint16_t wParam2, uint16_t wParam3)
{
	if (WasPlayerCmdAlreadyRequested(bCmd, position, wParam1, wParam2, wParam3))
		return;

	TCmdLocParam3 cmd;

	cmd.bCmd = bCmd;
	cmd.x = position.x;
	cmd.y = position.y;
	cmd.wParam1 = SDL_SwapLE16(wParam1);
	cmd.wParam2 = SDL_SwapLE16(wParam2);
	cmd.wParam3 = SDL_SwapLE16(wParam3);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));

	MyPlayer->UpdatePreviewCelSprite(bCmd, position, wParam1, wParam2);
}

void NetSendCmdLocParam4(bool bHiPri, _cmd_id bCmd, Point position, uint16_t wParam1, uint16_t wParam2, uint16_t wParam3, uint16_t wParam4)
{
	if (WasPlayerCmdAlreadyRequested(bCmd, position, wParam1, wParam2, wParam3, wParam4))
		return;

	TCmdLocParam4 cmd;

	cmd.bCmd = bCmd;
	cmd.x = position.x;
	cmd.y = position.y;
	cmd.wParam1 = SDL_SwapLE16(wParam1);
	cmd.wParam2 = SDL_SwapLE16(wParam2);
	cmd.wParam3 = SDL_SwapLE16(wParam3);
	cmd.wParam4 = SDL_SwapLE16(wParam4);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));

	MyPlayer->UpdatePreviewCelSprite(bCmd, position, wParam1, wParam3);
}

void NetSendCmdParam1(bool bHiPri, _cmd_id bCmd, uint16_t wParam1)
{
	if (WasPlayerCmdAlreadyRequested(bCmd, {}, wParam1))
		return;

	TCmdParam1 cmd;

	cmd.bCmd = bCmd;
	cmd.wParam1 = SDL_SwapLE16(wParam1);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));

	MyPlayer->UpdatePreviewCelSprite(bCmd, {}, wParam1, 0);
}

void NetSendCmdParam2(bool bHiPri, _cmd_id bCmd, uint16_t wParam1, uint16_t wParam2)
{
	TCmdParam2 cmd;

	cmd.bCmd = bCmd;
	cmd.wParam1 = SDL_SwapLE16(wParam1);
	cmd.wParam2 = SDL_SwapLE16(wParam2);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdParam4(bool bHiPri, _cmd_id bCmd, uint16_t wParam1, uint16_t wParam2, uint16_t wParam3, uint16_t wParam4)
{
	if (WasPlayerCmdAlreadyRequested(bCmd, {}, wParam1, wParam2, wParam3, wParam4))
		return;

	TCmdParam4 cmd;

	cmd.bCmd = bCmd;
	cmd.wParam1 = SDL_SwapLE16(wParam1);
	cmd.wParam2 = SDL_SwapLE16(wParam2);
	cmd.wParam3 = SDL_SwapLE16(wParam3);
	cmd.wParam4 = SDL_SwapLE16(wParam4);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));

	MyPlayer->UpdatePreviewCelSprite(bCmd, {}, wParam1, wParam2);
}

void NetSendCmdQuest(bool bHiPri, const Quest &quest)
{
	TCmdQuest cmd;
	cmd.bCmd = CMD_SYNCQUEST;
	cmd.q = quest._qidx,
	cmd.qstate = quest._qactive;
	cmd.qlog = quest._qlog ? 1 : 0;
	cmd.qvar1 = quest._qvar1;
	cmd.qvar2 = quest._qvar2;
	cmd.qmsg = quest._qmsg;

	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdGItem(bool bHiPri, _cmd_id bCmd, const Player &player, uint8_t ii)
{
	uint8_t pnum = player.getId();

	TCmdGItem cmd;

	cmd.bCmd = bCmd;
	cmd.bPnum = pnum;
	cmd.bMaster = pnum;
	cmd.bLevel = GetLevelForMultiplayer(*MyPlayer);
	cmd.bCursitem = ii;
	cmd.dwTime = 0;
	cmd.x = Items[ii].position.x;
	cmd.y = Items[ii].position.y;
	PrepareItemForNetwork(Items[ii], cmd);

	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdPItem(bool bHiPri, _cmd_id bCmd, Point position, const Item &item)
{
	TCmdPItem cmd {};

	cmd.bCmd = bCmd;
	cmd.x = position.x;
	cmd.y = position.y;
	PrepareItemForNetwork(item, cmd);

	ItemLimbo = item;

	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdChItem(bool bHiPri, uint8_t bLoc, bool forceSpellChange)
{
	TCmdChItem cmd {};

	Item &item = MyPlayer->InvBody[bLoc];

	cmd.bCmd = CMD_CHANGEPLRITEMS;
	cmd.bLoc = bLoc;
	cmd.forceSpell = forceSpellChange;
	PrepareItemForNetwork(item, cmd);

	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdDelItem(bool bHiPri, uint8_t bLoc)
{
	TCmdDelItem cmd;

	cmd.bLoc = bLoc;
	cmd.bCmd = CMD_DELPLRITEMS;
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSyncInvItem(const Player &player, int invListIndex)
{
	if (&player != MyPlayer)
		return;

	for (int j = 0; j < InventoryGridCells; j++) {
		if (player.InvGrid[j] == invListIndex + 1) {
			NetSendCmdChInvItem(false, j);
			break;
		}
	}
}

void NetSendCmdChInvItem(bool bHiPri, int invGridIndex)
{
	TCmdChItem cmd {};

	int8_t invListIndex = std::abs(MyPlayer->InvGrid[invGridIndex]) - 1;
	const Item &item = MyPlayer->InvList[invListIndex];

	cmd.bCmd = CMD_CHANGEINVITEMS;
	cmd.bLoc = invGridIndex;
	PrepareItemForNetwork(item, cmd);

	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdChBeltItem(bool bHiPri, int beltIndex)
{
	TCmdChItem cmd {};

	const Item &item = MyPlayer->SpdList[beltIndex];

	cmd.bCmd = CMD_CHANGEBELTITEMS;
	cmd.bLoc = beltIndex;
	PrepareItemForNetwork(item, cmd);

	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdDamage(bool bHiPri, const Player &player, uint32_t dwDam, DamageType damageType)
{
	TCmdDamage cmd;

	cmd.bCmd = CMD_PLRDAMAGE;
	cmd.bPlr = player.getId();
	cmd.dwDam = dwDam;
	cmd.damageType = damageType;
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdMonDmg(bool bHiPri, uint16_t wMon, uint32_t dwDam)
{
	TCmdMonDamage cmd;

	cmd.bCmd = CMD_MONSTDAMAGE;
	cmd.wMon = wMon;
	cmd.dwDam = dwDam;
	if (bHiPri)
		NetSendHiPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, (std::byte *)&cmd, sizeof(cmd));
}

void NetSendCmdString(uint32_t pmask, const char *pszStr)
{
	TCmdString cmd;

	cmd.bCmd = CMD_STRING;
	CopyUtf8(cmd.str, pszStr, sizeof(cmd.str));
	multi_send_msg_packet(pmask, (std::byte *)&cmd, strlen(cmd.str) + 2);
}

void delta_close_portal(const Player &player)
{
	memset(&sgJunk.portal[player.getId()], 0xFF, sizeof(sgJunk.portal[player.getId()]));
}

size_t ParseCmd(uint8_t pnum, const TCmd *pCmd)
{
	sbLastCmd = pCmd->bCmd;
	if (sgwPackPlrOffsetTbl[pnum] != 0 && sbLastCmd != CMD_ACK_PLRINFO && sbLastCmd != CMD_SEND_PLRINFO)
		return 0;

	Player &player = Players[pnum];

#ifdef LOG_RECEIVED_MESSAGES
	Log("📥 {}", CmdIdString(pCmd->bCmd));
#endif

	switch (pCmd->bCmd) {
	case CMD_SYNCDATA:
		return OnSyncData(pCmd, player);
	case CMD_WALKXY:
		return OnWalk(pCmd, player);
	case CMD_ADDSTR:
		return OnAddStrength(pCmd, player);
	case CMD_ADDDEX:
		return OnAddDexterity(pCmd, player);
	case CMD_ADDMAG:
		return OnAddMagic(pCmd, player);
	case CMD_ADDVIT:
		return OnAddVitality(pCmd, player);
	case CMD_GOTOGETITEM:
		return OnGotoGetItem(pCmd, player);
	case CMD_REQUESTGITEM:
		return OnRequestGetItem(pCmd, player);
	case CMD_GETITEM:
		return OnGetItem(pCmd, player);
	case CMD_GOTOAGETITEM:
		return OnGotoAutoGetItem(pCmd, player);
	case CMD_REQUESTAGITEM:
		return OnRequestAutoGetItem(pCmd, player);
	case CMD_AGETITEM:
		return OnAutoGetItem(pCmd, player);
	case CMD_ITEMEXTRA:
		return OnItemExtra(pCmd, player);
	case CMD_PUTITEM:
		return OnPutItem(pCmd, player);
	case CMD_SYNCPUTITEM:
		return OnSyncPutItem(pCmd, player);
	case CMD_SPAWNITEM:
		return OnSpawnItem(pCmd, player);
	case CMD_ATTACKXY:
		return OnAttackTile(pCmd, player);
	case CMD_SATTACKXY:
		return OnStandingAttackTile(pCmd, player);
	case CMD_RATTACKXY:
		return OnRangedAttackTile(pCmd, player);
	case CMD_SPELLXYD:
		return OnSpellWall(pCmd, player);
	case CMD_SPELLXY:
		return OnSpellTile(pCmd, player);
	case CMD_OPOBJXY:
		return OnObjectTileAction(*pCmd, player, ACTION_OPERATE);
	case CMD_DISARMXY:
		return OnObjectTileAction(*pCmd, player, ACTION_DISARM);
	case CMD_OPOBJT:
		return OnObjectTileAction(*pCmd, player, ACTION_OPERATETK, false);
	case CMD_ATTACKID:
		return OnAttackMonster(pCmd, player);
	case CMD_ATTACKPID:
		return OnAttackPlayer(pCmd, player);
	case CMD_RATTACKID:
		return OnRangedAttackMonster(pCmd, player);
	case CMD_RATTACKPID:
		return OnRangedAttackPlayer(pCmd, player);
	case CMD_SPELLID:
		return OnSpellMonster(pCmd, player);
	case CMD_SPELLPID:
		return OnSpellPlayer(pCmd, player);
	case CMD_KNOCKBACK:
		return OnKnockback(pCmd, player);
	case CMD_RESURRECT:
		return OnResurrect(pCmd, player);
	case CMD_HEALOTHER:
		return OnHealOther(pCmd, player);
	case CMD_TALKXY:
		return OnTalkXY(pCmd, player);
	case CMD_DEBUG:
		return OnDebug(pCmd);
	case CMD_NEWLVL:
		return OnNewLevel(pCmd, player);
	case CMD_WARP:
		return OnWarp(pCmd, player);
	case CMD_MONSTDEATH:
		return OnMonstDeath(pCmd, player);
	case CMD_KILLGOLEM:
		return OnKillGolem(pCmd, player);
	case CMD_AWAKEGOLEM:
		return OnAwakeGolem(pCmd, player);
	case CMD_MONSTDAMAGE:
		return OnMonstDamage(pCmd, player);
	case CMD_PLRDEAD:
		return OnPlayerDeath(pCmd, player);
	case CMD_PLRDAMAGE:
		return OnPlayerDamage(pCmd, player);
	case CMD_OPENDOOR:
	case CMD_CLOSEDOOR:
	case CMD_OPERATEOBJ:
		return OnOperateObject(*pCmd, player);
	case CMD_BREAKOBJ:
		return OnBreakObject(*pCmd, player);
	case CMD_CHANGEPLRITEMS:
		return OnChangePlayerItems(pCmd, player);
	case CMD_DELPLRITEMS:
		return OnDeletePlayerItems(pCmd, player);
	case CMD_CHANGEINVITEMS:
		return OnChangeInventoryItems(pCmd, player);
	case CMD_DELINVITEMS:
		return OnDeleteInventoryItems(pCmd, player);
	case CMD_CHANGEBELTITEMS:
		return OnChangeBeltItems(pCmd, player);
	case CMD_DELBELTITEMS:
		return OnDeleteBeltItems(pCmd, player);
	case CMD_PLRLEVEL:
		return OnPlayerLevel(pCmd, player);
	case CMD_DROPITEM:
		return OnDropItem(pCmd, player);
	case CMD_ACK_PLRINFO:
	case CMD_SEND_PLRINFO:
		return OnSendPlayerInfo(pCmd, player);
	case CMD_PLAYER_JOINLEVEL:
		return OnPlayerJoinLevel(pCmd, player);
	case CMD_ACTIVATEPORTAL:
		return OnActivatePortal(pCmd, player);
	case CMD_DEACTIVATEPORTAL:
		return OnDeactivatePortal(pCmd, player);
	case CMD_RETOWN:
		return OnRestartTown(pCmd, player);
	case CMD_SETSTR:
		return OnSetStrength(pCmd, player);
	case CMD_SETMAG:
		return OnSetMagic(pCmd, player);
	case CMD_SETDEX:
		return OnSetDexterity(pCmd, player);
	case CMD_SETVIT:
		return OnSetVitality(pCmd, player);
	case CMD_STRING:
		return OnString(pCmd, player);
	case CMD_FRIENDLYMODE:
		return OnFriendlyMode(pCmd, player);
	case CMD_SYNCQUEST:
		return OnSyncQuest(pCmd, player);
	case CMD_CHEAT_EXPERIENCE:
		return OnCheatExperience(pCmd, player);
	case CMD_CHANGE_SPELL_LEVEL:
		return OnChangeSpellLevel(pCmd, player);
	case CMD_SETSHIELD:
		return OnSetShield(pCmd, player);
	case CMD_REMSHIELD:
		return OnRemoveShield(pCmd, player);
	case CMD_SETREFLECT:
		return OnSetReflect(pCmd, player);
	case CMD_NAKRUL:
		return OnNakrul(pCmd);
	case CMD_OPENHIVE:
		return OnOpenHive(pCmd, player);
	case CMD_OPENGRAVE:
		return OnOpenGrave(pCmd);
	case CMD_SPAWNMONSTER:
		return OnSpawnMonster(pCmd, player);
	default:
		break;
	}

	if (pCmd->bCmd < CMD_DLEVEL || pCmd->bCmd > CMD_DLEVEL_END) {
		SNetDropPlayer(pnum, LEAVE_DROP);
		return 0;
	}

	return OnLevelData(pnum, pCmd);
}

} // namespace devilution
