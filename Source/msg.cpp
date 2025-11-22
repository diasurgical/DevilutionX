/**
 * @file msg.cpp
 *
 * Implementation of function for sending and receiving network messages.
 */
#include "msg.h"

#include <climits>
#include <cmath>
#include <cstdint>
#include <list>
#include <memory>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

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
#include "items/validation.h"
#include "levels/crypt.h"
#include "levels/town.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "missiles.h"
#include "monster.h"
#include "monsters/validation.hpp"
#include "nthread.h"
#include "objects.h"
#include "options.h"
#include "pack.h"
#include "pfile.h"
#include "player.h"
#include "plrmsg.h"
#include "portals/validation.hpp"
#include "quests/validation.hpp"
#include "spells.h"
#include "storm/storm_net.hpp"
#include "sync.h"
#include "tmsg.h"
#include "towners.h"
#include "utils/endian_swap.hpp"
#include "utils/is_of.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"
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
	const std::string message = fmt::format("Player '{}' sent an invalid packet.", playerName);
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
	case CMD_CHANGE_SPELL_LEVEL: return "CMD_CHANGE_SPELL_LEVEL";
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
	case CMD_SYNCQUEST: return "CMD_SYNCQUEST";
	case CMD_REQUESTSPAWNGOLEM: return "CMD_REQUESTSPAWNGOLEM";
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
	uint8_t menemy;
	uint8_t mactive;
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
	uint8_t golemOwnerPlayerId;
	int16_t golemSpellLevel;
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

constexpr size_t MaxMultiplayerLevels = NUMLEVELS + SL_LAST;
constexpr size_t MaxChunks = MaxMultiplayerLevels + 4;

uint32_t sgdwOwnerWait;
uint32_t sgdwRecvOffset;
int sgnCurrMegaPlayer;
ankerl::unordered_dense::map<uint8_t, DLevel> DeltaLevels;
uint8_t sbLastCmd;

/**
 * @brief buffer used to receive level deltas, size is the worst expected case assuming every object on a level was touched
 */
std::byte sgRecvBuf[1U                                             /* marker byte, always 0 */
    + sizeof(uint8_t)                                              /* level id */
    + sizeof(DLevel::item)                                         /* items spawned during dungeon generation which have been picked up, and items dropped by a player during a game */
    + sizeof(uint8_t)                                              /* count of object interactions which caused a state change since dungeon generation */
    + (sizeof(WorldTilePosition) + sizeof(_cmd_id)) * MAXOBJECTS   /* location/action pairs for the object interactions */
    + sizeof(DLevel::monster)                                      /* latest monster state */
    + sizeof(uint16_t)                                             /* spawned monster count */
    + (sizeof(uint16_t) + sizeof(DSpawnedMonster)) * MaxMonsters]; /* spawned monsters */

_cmd_id sgbRecvCmd;
ankerl::unordered_dense::map<uint8_t, LocalLevel> LocalLevels;
DJunk sgJunk;
uint8_t sgbDeltaChunks;
std::list<TMegaPkt> MegaPktList;
Item ItemLimbo;

/** @brief Last sent player command for the local player. */
TCmdLocParam5 lastSentPlayerCmd;

void RecreateItem(const Player &player, const TCmdPItem &message, Item &item);

bool IsMonsterDeltaValid(const DMonsterStr &monster)
{
	return InDungeonBounds(monster.position) && monster.hitPoints >= 0;
}

bool IsPortalDeltaValid(const DPortal &portal)
{
	const WorldTilePosition position { portal.x, portal.y };
	return IsPortalDeltaValid(position, portal.level, portal.ltype, portal.setlvl != 0);
}

bool IsQuestDeltaValid(quest_id qidx, const MultiQuests &quest)
{
	return IsQuestDeltaValid(qidx, quest.qstate, quest.qlog, quest.qmsg);
}

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
	const uint8_t level = GetLevelForMultiplayer(player);
	return GetDeltaLevel(level);
}

Point GetItemPosition(Point position)
{
	if (CanPut(position))
		return position;

	for (int k = 1; k < 50; k++) {
		for (int j = -k; j <= k; j++) {
			const int yy = position.y + j;
			for (int l = -k; l <= k; l++) {
				const int xx = position.x + l;
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

	const TCmdLocParam5 newSendParam = { bCmd, static_cast<uint8_t>(position.x), static_cast<uint8_t>(position.y),
		Swap16LE(wParam1), Swap16LE(wParam2), Swap16LE(wParam3), Swap16LE(wParam4), Swap16LE(wParam5) };

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
		size_t remainingBytes = sizeof(pkt.data) - pkt.spaceLeft;
		while (remainingBytes > 0) {
			auto cmdId = static_cast<_cmd_id>(*data);

			if (cmdId == FAKE_CMD_SETID) {
				auto *cmd = reinterpret_cast<TFakeCmdPlr *>(data);
				data += sizeof(*cmd);
				remainingBytes -= sizeof(*cmd);
				playerId = cmd->bPlr;
				continue;
			}

			if (cmdId == FAKE_CMD_DROPID) {
				auto *cmd = reinterpret_cast<TFakeDropPlr *>(data);
				data += sizeof(*cmd);
				remainingBytes -= sizeof(*cmd);
				multi_player_left(cmd->bPlr, Swap32LE(cmd->dwReason));
				continue;
			}

			if (playerId >= Players.size()) {
				Log("Missing source of network message");
				return;
			}

			const size_t size = ParseCmd(playerId, reinterpret_cast<TCmd *>(data), remainingBytes);
			if (size == 0) {
				Log("Discarding bad network message");
				return;
			}
			data += size;
			remainingBytes -= size;
		}
	}
}

void BufferMessage(const void *message, size_t messageSize)
{
	if (MegaPktList.back().spaceLeft < messageSize)
		GetNextPacket();

	TMegaPkt &currMegaPkt = MegaPktList.back();
	memcpy(currMegaPkt.data + sizeof(currMegaPkt.data) - currMegaPkt.spaceLeft, message, messageSize);
	currMegaPkt.spaceLeft -= messageSize;
}

void BufferMessage(uint8_t pnum, const void *message, size_t messageSize)
{
	if (messageSize > sizeof(TMegaPkt::data)) {
		Log("Discarding enormous network message");
		return;
	}

	if (pnum != sgnCurrMegaPlayer) {
		sgnCurrMegaPlayer = pnum;

		TFakeCmdPlr cmd;
		cmd.bCmd = FAKE_CMD_SETID;
		cmd.bPlr = pnum;
		BufferMessage(&cmd, sizeof(cmd));
	}

	BufferMessage(message, messageSize);
}

void BufferMessage(const Player &player, const void *message, size_t messageSize)
{
	BufferMessage(player.getId(), message, messageSize);
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
	if (sgbDeltaChunks == MaxChunks - 1) {
		sgbDeltaChunks = MaxChunks;
		return 99;
	}
	return 100 * sgbDeltaChunks / static_cast<int>(MaxChunks);
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

const std::byte *DeltaImportItem(const std::byte *src, const std::byte *end, TCmdPItem *dst)
{
	size_t size = 0;
	for (int i = 0; i < MAXITEMS; i++, dst++) {
		if (&src[size] >= end)
			return nullptr;
		if (src[size] == std::byte { 0xFF }) {
			memset(dst, 0xFF, sizeof(TCmdPItem));
			size++;
		} else {
			if (&src[size] + sizeof(TCmdPItem) > end)
				return nullptr;
			memcpy(dst, &src[size], sizeof(TCmdPItem));
			if (!IsItemDeltaValid(*dst))
				memset(dst, 0xFF, sizeof(TCmdPItem));
			size += sizeof(TCmdPItem);
		}
	}

	return src + size;
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

const std::byte *DeltaImportObjects(const std::byte *src, const std::byte *end, ankerl::unordered_dense::map<WorldTilePosition, DObjectStr> &dst)
{
	dst.clear();

	if (src == nullptr || src == end)
		return nullptr;

	auto numDeltas = static_cast<uint8_t>(*src++);
	if (numDeltas > MAXOBJECTS)
		return nullptr;

	const size_t numBytes = (sizeof(WorldTilePosition) + sizeof(_cmd_id)) * numDeltas;
	if (src + numBytes > end)
		return nullptr;

	dst.reserve(numDeltas);

	for (unsigned i = 0; i < numDeltas; i++) {
		const WorldTilePosition objectPosition { static_cast<WorldTileCoord>(src[0]), static_cast<WorldTileCoord>(src[1]) };
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

const std::byte *DeltaImportMonster(const std::byte *src, const std::byte *end, DMonsterStr *dst)
{
	if (src == nullptr)
		return nullptr;

	size_t size = 0;
	for (size_t i = 0; i < MaxMonsters; i++, dst++) {
		if (&src[size] >= end)
			return nullptr;
		if (src[size] == std::byte { 0xFF }) {
			memset(dst, 0xFF, sizeof(DMonsterStr));
			size++;
		} else {
			if (&src[size] + sizeof(DMonsterStr) > end)
				return nullptr;
			memcpy(dst, &src[size], sizeof(DMonsterStr));
			size += sizeof(DMonsterStr);
		}
	}

	return src + size;
}

std::byte *DeltaExportSpawnedMonsters(std::byte *dst, const ankerl::unordered_dense::map<size_t, DSpawnedMonster> &spawnedMonsters)
{
	uint16_t size = Swap16LE(static_cast<uint16_t>(spawnedMonsters.size()));
	memcpy(dst, &size, sizeof(uint16_t));
	dst += sizeof(uint16_t);

	for (const auto &deltaSpawnedMonster : spawnedMonsters) {
		uint16_t monsterId = Swap16LE(static_cast<uint16_t>(deltaSpawnedMonster.first));
		memcpy(dst, &monsterId, sizeof(uint16_t));
		dst += sizeof(uint16_t);

		memcpy(dst, &deltaSpawnedMonster.second, sizeof(DSpawnedMonster));
		dst += sizeof(DSpawnedMonster);
	}

	return dst;
}

const std::byte *DeltaImportSpawnedMonsters(const std::byte *src, const std::byte *end, ankerl::unordered_dense::map<size_t, DSpawnedMonster> &spawnedMonsters)
{
	if (src == nullptr || src + sizeof(uint16_t) > end)
		return nullptr;

	uint16_t size;
	memcpy(&size, src, sizeof(uint16_t));
	size = Swap16LE(size);
	src += sizeof(uint16_t);
	if (size > MaxMonsters)
		return nullptr;

	const size_t requiredBytes = (sizeof(uint16_t) + sizeof(DSpawnedMonster)) * size;
	if (src + requiredBytes > end)
		return nullptr;

	for (size_t i = 0; i < size; i++) {
		uint16_t monsterId;
		memcpy(&monsterId, src, sizeof(uint16_t));
		monsterId = Swap16LE(monsterId);
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
		sgJunk.quests[q].qmsg = Swap16LE(static_cast<int16_t>(quest._qmsg));
		memcpy(dst, &sgJunk.quests[q], sizeof(MultiQuests));
		dst += sizeof(MultiQuests);
		q++;
	}

	return dst;
}

const std::byte *DeltaImportJunk(const std::byte *src, const std::byte *end)
{
	for (DPortal &portal : sgJunk.portal) {
		if (src >= end)
			return nullptr;
		if (*src == std::byte { 0xFF }) {
			memset(&portal, 0xFF, sizeof(DPortal));
			src++;
		} else {
			if (src + sizeof(DPortal) > end)
				return nullptr;
			memcpy(&portal, src, sizeof(DPortal));
			if (!IsPortalDeltaValid(portal))
				memset(&portal, 0xFF, sizeof(DPortal));
			src += sizeof(DPortal);
		}
	}

	int q = 0;
	for (int qidx = 0; qidx < MAXQUESTS; qidx++) {
		if (QuestsData[qidx].isSinglePlayerOnly && UseMultiplayerQuests()) {
			continue;
		}
		if (src + sizeof(MultiQuests) > end) {
			return nullptr;
		}
		memcpy(&sgJunk.quests[q], src, sizeof(MultiQuests));
		if (!IsQuestDeltaValid(static_cast<quest_id>(qidx), sgJunk.quests[q])) {
			sgJunk.quests[q].qstate = QUEST_INVALID;
		}
		src += sizeof(MultiQuests);
		q++;
	}

	return src;
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

void DeltaImportData(_cmd_id cmd, uint32_t recvOffset, int pnum)
{
	size_t deltaSize = recvOffset;

#ifdef USE_PKWARE
	if (sgRecvBuf[0] != std::byte { 0 }) {
		deltaSize = PkwareDecompress(&sgRecvBuf[1], static_cast<uint32_t>(deltaSize), sizeof(sgRecvBuf) - 1);
		if (deltaSize == 0) {
			Log("PKWare decompression failure, dropping player {}", pnum);
			SNetDropPlayer(pnum, LEAVE_DROP);
			return;
		}
	}
#endif

	const std::byte *src = &sgRecvBuf[1];
	const std::byte *end = src + deltaSize;
	if (cmd == CMD_DLEVEL_JUNK) {
		src = DeltaImportJunk(src, end);
	} else if (cmd == CMD_DLEVEL) {
		auto i = static_cast<uint8_t>(src[0]);
		src += sizeof(uint8_t);
		DLevel &deltaLevel = GetDeltaLevel(i);
		src = DeltaImportItem(src, end, deltaLevel.item);
		src = DeltaImportObjects(src, end, deltaLevel.object);
		src = DeltaImportMonster(src, end, deltaLevel.monster);
		src = DeltaImportSpawnedMonsters(src, end, deltaLevel.spawnedMonsters);
	} else {
		Log("Received invalid deltas, dropping player {}", pnum);
		SNetDropPlayer(pnum, LEAVE_DROP);
		return;
	}

	if (src == nullptr) {
		Log("Received invalid deltas, dropping player {}", pnum);
		SNetDropPlayer(pnum, LEAVE_DROP);
		return;
	}

	sgbDeltaChunks++;
}

void DeltaLoadSpawnedMonsters(const DLevel &deltaLevel)
{
	for (const auto &deltaSpawnedMonster : deltaLevel.spawnedMonsters) {
		const auto &monsterData = deltaSpawnedMonster.second;
		LoadDeltaSpawnedMonster(deltaSpawnedMonster.second.typeIndex, deltaSpawnedMonster.first, monsterData.seed, monsterData.golemOwnerPlayerId, monsterData.golemSpellLevel);
		assert(deltaLevel.monster[deltaSpawnedMonster.first].position.x != 0xFF);
	}
}

void DeltaLoadEnemies(const DLevel &deltaLevel)
{
	for (size_t i = 0; i < MaxMonsters; i++) {
		const DMonsterStr &deltaMonster = deltaLevel.monster[i];
		if (!IsMonsterDeltaValid(deltaMonster))
			continue;
		if (deltaMonster.hitPoints == 0)
			continue;
		Monster &monster = Monsters[i];
		if (IsEnemyValid(i, deltaMonster.menemy))
			decode_enemy(monster, deltaMonster.menemy);
		if (monster.position.tile != Point { 0, 0 } && monster.position.tile != GolemHoldingCell)
			monster.occupyTile(monster.position.tile, false);
		if (monster.type().type == MT_GOLEM) {
			GolumAi(monster);
			monster.flags |= (MFLAG_TARGETS_MONSTER | MFLAG_GOLEM);
		} else {
			M_StartStand(monster, monster.direction);
		}
		monster.activeForTicks = deltaMonster.mactive;
	}
}

void DeltaLoadMonsters(const DLevel &deltaLevel)
{
	for (size_t i = 0; i < MaxMonsters; i++) {
		const DMonsterStr &deltaMonster = deltaLevel.monster[i];
		if (!IsMonsterDeltaValid(deltaMonster))
			continue;

		Monster &monster = Monsters[i];
		M_ClearSquares(monster);
		{
			const WorldTilePosition position = deltaMonster.position;
			monster.position.tile = position;
			monster.position.old = position;
			monster.position.future = position;
			if (monster.lightId != NO_LIGHT)
				ChangeLightXY(monster.lightId, position);
		}

		monster.hitPoints = Swap32LE(deltaMonster.hitPoints);
		monster.whoHit = deltaMonster.mWhoHit;
		if (deltaMonster.hitPoints != 0)
			continue;

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
	}

	// Calling this here ensures that monster hitpoints
	// are synced before attempting to validate enemy IDs
	DeltaLoadEnemies(deltaLevel);
}

void DeltaLoadObjects(DLevel &deltaLevel)
{
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

void DeltaLoadItems(const DLevel &deltaLevel)
{
	for (const TCmdPItem &deltaItem : deltaLevel.item) {
		if (deltaItem.bCmd == CMD_INVALID)
			continue;

		if (deltaItem.bCmd == TCmdPItem::PickedUpItem) {
			const int activeItemIndex = FindGetItem(
			    Swap32LE(deltaItem.def.dwSeed),
			    static_cast<_item_indexes>(Swap16LE(deltaItem.def.wIndx)),
			    Swap16LE(deltaItem.def.wCI));
			if (activeItemIndex != -1) {
				const auto &position = Items[ActiveItems[activeItemIndex]].position;
				if (dItem[position.x][position.y] == ActiveItems[activeItemIndex] + 1)
					dItem[position.x][position.y] = 0;
				DeleteItem(activeItemIndex);
			}
		}
		if (deltaItem.bCmd == TCmdPItem::DroppedItem) {
			const int ii = AllocateItem();
			auto &item = Items[ii];
			RecreateItem(*MyPlayer, deltaItem, item);

			const int x = deltaItem.x;
			const int y = deltaItem.y;
			item.position = GetItemPosition({ x, y });
			dItem[item.position.x][item.position.y] = static_cast<int8_t>(ii + 1);
			RespawnItem(Items[ii], false);
		}
	}
}

size_t OnLevelData(const TCmdPlrInfoHdr &message, size_t maxCmdSize, const Player &player)
{
	const uint16_t wBytes = Swap16LE(message.wBytes);
	const uint16_t wOffset = Swap16LE(message.wOffset);

	if (!ValidateCmdSize(wBytes + sizeof(message), maxCmdSize, player.getId()))
		return maxCmdSize;

	if (gbDeltaSender != player.getId()) {
		if (message.bCmd != CMD_DLEVEL_END && (message.bCmd != CMD_DLEVEL || wOffset != 0)) {
			return wBytes + sizeof(message);
		}

		gbDeltaSender = player.getId();
		sgbRecvCmd = CMD_DLEVEL_END;
	}

	if (sgbRecvCmd == CMD_DLEVEL_END) {
		if (message.bCmd == CMD_DLEVEL_END) {
			sgbDeltaChunks = MaxChunks - 1;
			return wBytes + sizeof(message);
		}
		if (message.bCmd != CMD_DLEVEL || wOffset != 0) {
			return wBytes + sizeof(message);
		}

		sgdwRecvOffset = 0;
		sgbRecvCmd = message.bCmd;
	} else if (sgbRecvCmd != message.bCmd || wOffset == 0) {
		DeltaImportData(sgbRecvCmd, sgdwRecvOffset, player.getId());
		if (message.bCmd == CMD_DLEVEL_END) {
			sgbDeltaChunks = MaxChunks - 1;
			sgbRecvCmd = CMD_DLEVEL_END;
			return wBytes + sizeof(message);
		}
		sgdwRecvOffset = 0;
		sgbRecvCmd = message.bCmd;
	}

	if (sgdwRecvOffset + wBytes > sizeof(sgRecvBuf)) {
		Log("Received too many deltas, dropping player {}", player.getId());
		SNetDropPlayer(player.getId(), LEAVE_DROP);
		return wBytes + sizeof(message);
	}

	assert(wOffset == sgdwRecvOffset);
	memcpy(&sgRecvBuf[sgdwRecvOffset], &message + 1, wBytes);
	sgdwRecvOffset += wBytes;
	return wBytes + sizeof(message);
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
		delta.menemy = encode_enemy(monster);
		delta.hitPoints = monster.hitPoints;
		delta.mactive = monster.activeForTicks;
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

#ifdef _DEBUG
		app_fatal("delta:1");
#endif
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
			if (item.bCmd != TCmdPItem::DroppedItem) {
				Log("Suspicious floor item duplication, dropping player {}", player.getId());
				SNetDropPlayer(player.getId(), LEAVE_DROP);
			}
			return;
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
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
		return;
	}

	auto ticks = static_cast<int32_t>(SDL_GetTicks());
	if (cmd.dwTime == 0) {
		cmd.dwTime = Swap32LE(ticks);
	} else if (ticks - Swap32LE(cmd.dwTime) > 5000) {
		return;
	}

	tmsg_add(reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

bool NetSendCmdReq2(_cmd_id bCmd, const Player &player, uint8_t pnum, const TCmdGItem &item)
{
	TCmdGItem cmd;

	memcpy(&cmd, &item, sizeof(cmd));
	cmd.bCmd = bCmd;
	cmd.bPnum = pnum;
	cmd.bMaster = player.getId();

	auto ticks = static_cast<int32_t>(SDL_GetTicks());
	if (cmd.dwTime == 0)
		cmd.dwTime = Swap32LE(ticks);
	else if (ticks - Swap32LE(cmd.dwTime) > 5000)
		return false;

	tmsg_add(reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));

	return true;
}

void NetSendCmdExtra(const TCmdGItem &item)
{
	TCmdGItem cmd;

	memcpy(&cmd, &item, sizeof(cmd));
	cmd.dwTime = 0;
	cmd.bCmd = CMD_ITEMEXTRA;
	NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

size_t OnWalk(const TCmdLoc &message, Player &player)
{
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position)) {
		ClrPlrPath(player);
		MakePlrPath(player, position, true);
		player.destAction = ACTION_NONE;
	}

	return sizeof(message);
}

size_t OnAddStrength(const TCmdParam1 &message, Player &player)
{
	if (gbBufferMsgs == 1)
		BufferMessage(player, &message, sizeof(message));
	else if (message.wParam1 <= 256)
		ModifyPlrStr(player, Swap16LE(message.wParam1));

	return sizeof(message);
}

size_t OnAddMagic(const TCmdParam1 &message, Player &player)
{
	if (gbBufferMsgs == 1)
		BufferMessage(player, &message, sizeof(message));
	else if (message.wParam1 <= 256)
		ModifyPlrMag(player, Swap16LE(message.wParam1));

	return sizeof(message);
}

size_t OnAddDexterity(const TCmdParam1 &message, Player &player)
{
	if (gbBufferMsgs == 1)
		BufferMessage(player, &message, sizeof(message));
	else if (message.wParam1 <= 256)
		ModifyPlrDex(player, Swap16LE(message.wParam1));

	return sizeof(message);
}

size_t OnAddVitality(const TCmdParam1 &message, Player &player)
{
	if (gbBufferMsgs == 1)
		BufferMessage(player, &message, sizeof(message));
	else if (message.wParam1 <= 256)
		ModifyPlrVit(player, Swap16LE(message.wParam1));

	return sizeof(message);
}

size_t OnGotoGetItem(const TCmdLocParam1 &message, Player &player)
{
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position) && Swap16LE(message.wParam1) < MAXITEMS + 1) {
		MakePlrPath(player, position, false);
		player.destAction = ACTION_PICKUPITEM;
		player.destParam1 = Swap16LE(message.wParam1);
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

	return IsItemAvailable(static_cast<_item_indexes>(Swap16LE(message.def.wIndx)));
}

bool IsPItemValid(const TCmdPItem &message, const Player &player)
{
	if (!gbIsMultiplayer)
		return true;

	const Point position { message.x, message.y };

	if (!InDungeonBounds(position))
		return false;

	auto idx = static_cast<_item_indexes>(Swap16LE(message.def.wIndx));

	if (idx != IDI_EAR) {
		const uint16_t creationFlags = Swap16LE(message.item.wCI);
		const uint32_t dwBuff = Swap16LE(message.item.dwBuff);

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
	message.def.wIndx = static_cast<_item_indexes>(Swap16LE(item.IDidx));
	message.def.wCI = Swap16LE(item._iCreateInfo);
	message.def.dwSeed = Swap32LE(item._iSeed);

	if (item.IDidx == IDI_EAR)
		PrepareEarForNetwork(item, message.ear);
	else
		PrepareItemForNetwork(item, message.item);
}

void PrepareItemForNetwork(const Item &item, TCmdPItem &message)
{
	message.def.wIndx = static_cast<_item_indexes>(Swap16LE(item.IDidx));
	message.def.wCI = Swap16LE(item._iCreateInfo);
	message.def.dwSeed = Swap32LE(item._iSeed);

	if (item.IDidx == IDI_EAR)
		PrepareEarForNetwork(item, message.ear);
	else
		PrepareItemForNetwork(item, message.item);
}

void PrepareItemForNetwork(const Item &item, TCmdChItem &message)
{
	message.def.wIndx = static_cast<_item_indexes>(Swap16LE(item.IDidx));
	message.def.wCI = Swap16LE(item._iCreateInfo);
	message.def.dwSeed = Swap32LE(item._iSeed);

	if (item.IDidx == IDI_EAR)
		PrepareEarForNetwork(item, message.ear);
	else
		PrepareItemForNetwork(item, message.item);
}

void RecreateItem(const Player &player, const TCmdPItem &message, Item &item)
{
	if (message.def.wIndx == Swap16LE(IDI_EAR))
		RecreateEar(item, Swap16LE(message.ear.wCI), Swap32LE(message.ear.dwSeed), message.ear.bCursval, message.ear.heroname);
	else
		RecreateItem(player, message.item, item);
}

void RecreateItem(const Player &player, const TCmdChItem &message, Item &item)
{
	if (message.def.wIndx == Swap16LE(IDI_EAR))
		RecreateEar(item, Swap16LE(message.ear.wCI), Swap32LE(message.ear.dwSeed), message.ear.bCursval, message.ear.heroname);
	else
		RecreateItem(player, message.item, item);
}

int SyncDropItem(Point position, const TItem &item)
{
	return SyncDropItem(
	    position,
	    static_cast<_item_indexes>(Swap16LE(item.wIndx)),
	    Swap16LE(item.wCI),
	    Swap32LE(item.dwSeed),
	    item.bId,
	    item.bDur,
	    item.bMDur,
	    item.bCh,
	    item.bMCh,
	    Swap16LE(item.wValue),
	    Swap32LE(item.dwBuff),
	    Swap16LE(item.wToHit),
	    Swap16LE(item.wMaxDam));
}

int SyncDropEar(Point position, const TEar &ear)
{
	return SyncDropEar(
	    position,
	    Swap16LE(ear.wCI),
	    Swap32LE(ear.dwSeed),
	    ear.bCursval,
	    ear.heroname);
}

int SyncDropItem(const TCmdGItem &message)
{
	const Point position = GetItemPosition({ message.x, message.y });
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
	const Point position = GetItemPosition({ message.x, message.y });
	if (message.def.wIndx == IDI_EAR) {
		return SyncDropEar(
		    position,
		    message.ear);
	}

	return SyncDropItem(
	    position,
	    message.item);
}

size_t OnRequestGetItem(const TCmdGItem &message, Player &player)
{
	if (gbBufferMsgs == 1 || !player.isLevelOwnedByLocalClient() || !IsGItemValid(message))
		return sizeof(message);

	const Point position { message.x, message.y };
	const uint32_t dwSeed = Swap32LE(message.def.dwSeed);
	const uint16_t wCI = Swap16LE(message.def.wCI);
	const auto wIndx = static_cast<_item_indexes>(Swap16LE(message.def.wIndx));
	if (!GetItemRecord(dwSeed, wCI, wIndx))
		return sizeof(message);

	int ii = -1;
	if (InDungeonBounds(position)) {
		ii = std::abs(dItem[position.x][position.y]) - 1;
		if (ii >= 0 && !Items[ii].keyAttributesMatch(dwSeed, wIndx, wCI)) {
			ii = -1;
		}
	}

	if (ii == -1) {
		// No item at the target position or the key attributes don't match, so try find a matching item.
		const int activeItemIndex = FindGetItem(dwSeed, wIndx, wCI);
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

	return sizeof(message);
}

size_t OnGetItem(const TCmdGItem &message, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
		return sizeof(message);
	}

	if (!IsGItemValid(message))
		return sizeof(message);

	const Point position { message.x, message.y };
	const uint32_t dwSeed = Swap32LE(message.def.dwSeed);
	const uint16_t wCI = Swap16LE(message.def.wCI);
	const auto wIndx = static_cast<_item_indexes>(Swap16LE(message.def.wIndx));
	if (!DeltaGetItem(message, message.bLevel)) {
		NetSendCmdGItem2(true, CMD_GETITEM, message.bMaster, message.bPnum, message);
		return sizeof(message);
	}

	const bool isOnActiveLevel = GetLevelForMultiplayer(*MyPlayer) == message.bLevel;
	if ((!isOnActiveLevel && message.bPnum != MyPlayerId) || message.bMaster == MyPlayerId)
		return sizeof(message);

	if (message.bPnum != MyPlayerId) {
		SyncGetItem(position, dwSeed, wIndx, wCI);
		return sizeof(message);
	}

	if (!isOnActiveLevel) {
		const int ii = SyncDropItem(message);
		if (ii != -1)
			InvGetItem(*MyPlayer, ii);
	} else {
		const int activeItemIndex = FindGetItem(dwSeed, wIndx, wCI);
		InvGetItem(*MyPlayer, ActiveItems[activeItemIndex]);
	}

	return sizeof(message);
}

size_t OnGotoAutoGetItem(const TCmdLocParam1 &message, Player &player)
{
	const Point position { message.x, message.y };

	const uint16_t itemIdx = Swap16LE(message.wParam1);
	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position) && itemIdx < MAXITEMS + 1) {
		MakePlrPath(player, position, false);
		player.destAction = ACTION_PICKUPAITEM;
		player.destParam1 = itemIdx;
	}

	return sizeof(message);
}

size_t OnRequestAutoGetItem(const TCmdGItem &message, Player &player)
{
	if (gbBufferMsgs != 1 && player.isLevelOwnedByLocalClient() && IsGItemValid(message)) {
		const Point position { message.x, message.y };
		const uint32_t dwSeed = Swap32LE(message.def.dwSeed);
		const uint16_t wCI = Swap16LE(message.def.wCI);
		const auto wIndx = static_cast<_item_indexes>(Swap16LE(message.def.wIndx));
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

size_t OnAutoGetItem(const TCmdGItem &message, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
		return sizeof(message);
	}

	if (!IsGItemValid(message))
		return sizeof(message);

	const Point position { message.x, message.y };
	if (!DeltaGetItem(message, message.bLevel)) {
		NetSendCmdGItem2(true, CMD_AGETITEM, message.bMaster, message.bPnum, message);
		return sizeof(message);
	}

	const bool isOnActiveLevel = GetLevelForMultiplayer(*MyPlayer) == message.bLevel;
	if ((!isOnActiveLevel && message.bPnum != MyPlayerId) || message.bMaster == MyPlayerId)
		return sizeof(message);

	if (message.bPnum != MyPlayerId) {
		SyncGetItem(position, Swap32LE(message.def.dwSeed), static_cast<_item_indexes>(Swap16LE(message.def.wIndx)), Swap16LE(message.def.wCI));
		return sizeof(message);
	}

	if (!isOnActiveLevel) {
		const int ii = SyncDropItem(message);
		if (ii != -1)
			AutoGetItem(*MyPlayer, &Items[ii], ii);
	} else {
		AutoGetItem(*MyPlayer, &Items[message.bCursitem], message.bCursitem);
	}

	return sizeof(message);
}

size_t OnItemExtra(const TCmdGItem &message, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (IsGItemValid(message)) {
		DeltaGetItem(message, message.bLevel);
		if (player.isOnActiveLevel()) {
			const Point position { message.x, message.y };
			SyncGetItem(position, Swap32LE(message.def.dwSeed), static_cast<_item_indexes>(Swap16LE(message.def.wIndx)), Swap16LE(message.def.wCI));
		}
	}

	return sizeof(message);
}

size_t OnPutItem(const TCmdPItem &message, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (IsPItemValid(message, player)) {
		const Point position { message.x, message.y };
		const bool isSelf = &player == MyPlayer;
		const int32_t dwSeed = Swap32LE(message.def.dwSeed);
		const uint16_t wCI = Swap16LE(message.def.wCI);
		const auto wIndx = static_cast<_item_indexes>(Swap16LE(message.def.wIndx));
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
		} else {
			PutItemRecord(dwSeed, wCI, wIndx);
			DeltaPutItem(message, position, player);
			if (isSelf)
				pfile_update(true);
		}
	}

	return sizeof(message);
}

size_t OnSyncPutItem(const TCmdPItem &message, Player &player)
{
	if (gbBufferMsgs == 1)
		BufferMessage(player, &message, sizeof(message));
	else if (IsPItemValid(message, player)) {
		const int32_t dwSeed = Swap32LE(message.def.dwSeed);
		const uint16_t wCI = Swap16LE(message.def.wCI);
		const auto wIndx = static_cast<_item_indexes>(Swap16LE(message.def.wIndx));
		if (player.isOnActiveLevel()) {
			const int ii = SyncDropItem(message);
			if (ii != -1) {
				PutItemRecord(dwSeed, wCI, wIndx);
				DeltaPutItem(message, Items[ii].position, player);
				if (&player == MyPlayer)
					pfile_update(true);
			}
		} else {
			PutItemRecord(dwSeed, wCI, wIndx);
			DeltaPutItem(message, { message.x, message.y }, player);
			if (&player == MyPlayer)
				pfile_update(true);
		}
	}

	return sizeof(message);
}

size_t OnAttackTile(const TCmdLoc &message, Player &player)
{
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && leveltype != DTYPE_TOWN && InDungeonBounds(position)) {
		MakePlrPath(player, position, false);
		player.destAction = ACTION_ATTACK;
		player.destParam1 = position.x;
		player.destParam2 = position.y;
	}

	return sizeof(message);
}

size_t OnStandingAttackTile(const TCmdLoc &message, Player &player)
{
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && leveltype != DTYPE_TOWN && InDungeonBounds(position)) {
		ClrPlrPath(player);
		player.destAction = ACTION_ATTACK;
		player.destParam1 = position.x;
		player.destParam2 = position.y;
	}

	return sizeof(message);
}

size_t OnRangedAttackTile(const TCmdLoc &message, Player &player)
{
	const Point position { message.x, message.y };

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && leveltype != DTYPE_TOWN && InDungeonBounds(position)) {
		ClrPlrPath(player);
		player.destAction = ACTION_RATTACK;
		player.destParam1 = position.x;
		player.destParam2 = position.y;
	}

	return sizeof(message);
}

bool InitNewSpell(Player &player, uint16_t wParamSpellID, uint16_t wParamSpellType, uint16_t wParamSpellFrom)
{
	wParamSpellID = Swap16LE(wParamSpellID);
	wParamSpellType = Swap16LE(wParamSpellType);
	wParamSpellFrom = Swap16LE(wParamSpellFrom);

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

size_t OnSpellWall(const TCmdLocParam4 &message, Player &player)
{
	const Point position { message.x, message.y };

	if (gbBufferMsgs == 1)
		return sizeof(message);
	if (!player.isOnActiveLevel())
		return sizeof(message);
	if (!InDungeonBounds(position))
		return sizeof(message);
	const int16_t wParamDirection = Swap16LE(message.wParam3);
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

size_t OnSpellTile(const TCmdLocParam3 &message, Player &player)
{
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

size_t OnObjectTileAction(const TCmdLoc &message, Player &player, action_id action, bool pathToObject = true)
{
	const Point position { message.x, message.y };
	const Object *object = FindObjectAtPosition(position);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && object != nullptr) {
		if (pathToObject)
			MakePlrPath(player, position, !object->_oSolidFlag && !object->_oDoorFlag);

		player.destAction = action;
		player.destParam1 = static_cast<int>(object->GetId());
	}

	return sizeof(message);
}

size_t OnObjectTileAction(const TCmdLoc &message, Player &player)
{
	switch (message.bCmd) {
	case CMD_OPOBJXY:
		return OnObjectTileAction(message, player, ACTION_OPERATE);
	case CMD_DISARMXY:
		return OnObjectTileAction(message, player, ACTION_DISARM);
	case CMD_OPOBJT:
		return OnObjectTileAction(message, player, ACTION_OPERATETK, false);
	default:
		return sizeof(message);
	}
}

size_t OnAttackMonster(const TCmdParam1 &message, Player &player)
{
	const uint16_t monsterIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && leveltype != DTYPE_TOWN && monsterIdx < MaxMonsters) {
		const Point position = Monsters[monsterIdx].position.future;
		if (player.position.tile.WalkingDistance(position) > 1)
			MakePlrPath(player, position, false);
		player.destAction = ACTION_ATTACKMON;
		player.destParam1 = monsterIdx;
	}

	return sizeof(message);
}

size_t OnAttackPlayer(const TCmdParam1 &message, Player &player)
{
	const uint16_t playerIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && leveltype != DTYPE_TOWN && playerIdx < Players.size()) {
		MakePlrPath(player, Players[playerIdx].position.future, false);
		player.destAction = ACTION_ATTACKPLR;
		player.destParam1 = playerIdx;
	}

	return sizeof(message);
}

size_t OnRangedAttackMonster(const TCmdParam1 &message, Player &player)
{
	const uint16_t monsterIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && leveltype != DTYPE_TOWN && monsterIdx < MaxMonsters) {
		ClrPlrPath(player);
		player.destAction = ACTION_RATTACKMON;
		player.destParam1 = monsterIdx;
	}

	return sizeof(message);
}

size_t OnRangedAttackPlayer(const TCmdParam1 &message, Player &player)
{
	const uint16_t playerIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && leveltype != DTYPE_TOWN && playerIdx < Players.size()) {
		ClrPlrPath(player);
		player.destAction = ACTION_RATTACKPLR;
		player.destParam1 = playerIdx;
	}

	return sizeof(message);
}

size_t OnSpellMonster(const TCmdParam4 &message, Player &player)
{
	if (gbBufferMsgs == 1)
		return sizeof(message);
	if (!player.isOnActiveLevel())
		return sizeof(message);
	if (leveltype == DTYPE_TOWN)
		return sizeof(message);
	const uint16_t monsterIdx = Swap16LE(message.wParam1);
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

size_t OnSpellPlayer(const TCmdParam4 &message, Player &player)
{
	if (gbBufferMsgs == 1)
		return sizeof(message);
	if (!player.isOnActiveLevel())
		return sizeof(message);
	const uint16_t playerIdx = Swap16LE(message.wParam1);
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

size_t OnKnockback(const TCmdParam1 &message, Player &player)
{
	const uint16_t monsterIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && leveltype != DTYPE_TOWN && monsterIdx < MaxMonsters) {
		Monster &monster = Monsters[monsterIdx];
		M_GetKnockback(monster, player.position.tile);
		M_StartHit(monster, player, 0);
	}

	return sizeof(message);
}

size_t OnResurrect(const TCmdParam1 &message, Player &player)
{
	const uint16_t playerIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (playerIdx < Players.size()) {
		DoResurrect(player, Players[playerIdx]);
		if (&player == MyPlayer)
			pfile_update(true);
	}

	return sizeof(message);
}

size_t OnHealOther(const TCmdParam1 &message, const Player &caster)
{
	const uint16_t playerIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (caster.isOnActiveLevel() && playerIdx < Players.size()) {
			DoHealOther(caster, Players[playerIdx]);
		}
	}

	return sizeof(message);
}

size_t OnTalkXY(const TCmdLocParam1 &message, Player &player)
{
	const Point position { message.x, message.y };
	const uint16_t townerIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1 && player.isOnActiveLevel() && InDungeonBounds(position) && townerIdx < NUM_TOWNERS) {
		MakePlrPath(player, position, false);
		player.destAction = ACTION_TALK;
		player.destParam1 = townerIdx;
	}

	return sizeof(message);
}

size_t OnNewLevel(const TCmdParam2 &message, Player &player)
{
	const uint16_t eventIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (&player != MyPlayer) {
		if (eventIdx < WM_FIRST || eventIdx > WM_LAST)
			return sizeof(message);

		auto mode = static_cast<interface_mode>(eventIdx);

		const auto levelId = static_cast<uint8_t>(Swap16LE(message.wParam2));
		if (!IsValidLevel(levelId, mode == WM_DIABSETLVL)) {
			return sizeof(message);
		}

		StartNewLvl(player, mode, levelId);
	}

	return sizeof(message);
}

size_t OnWarp(const TCmdParam1 &message, Player &player)
{
	const uint16_t portalIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (portalIdx < MAXPORTAL) {
		StartWarpLvl(player, portalIdx);
	}

	return sizeof(message);
}

size_t OnMonstDeath(const TCmdLocParam1 &message, Player &player)
{
	const Point position { message.x, message.y };
	const uint16_t monsterIdx = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (&player != MyPlayer && player.plrlevel > 0 && InDungeonBounds(position) && monsterIdx < MaxMonsters) {
			Monster &monster = Monsters[monsterIdx];
			if (player.isOnActiveLevel())
				M_SyncStartKill(monster, position, player);
			delta_kill_monster(monster, position, player);
		}
	} else {
		BufferMessage(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnRequestSpawnGolem(const TCmdLocParam1 &message, const Player &player)
{
	if (gbBufferMsgs == 1)
		return sizeof(message);

	const WorldTilePosition position { message.x, message.y };

	if (player.plrlevel > 0 && player.isLevelOwnedByLocalClient() && InDungeonBounds(position))
		SpawnGolem(player, position, static_cast<uint8_t>(message.wParam1));

	return sizeof(message);
}

size_t OnMonstDamage(const TCmdMonDamage &message, Player &player)
{
	const uint16_t monsterIdx = Swap16LE(message.wMon);

	if (gbBufferMsgs != 1) {
		if (&player != MyPlayer) {
			if (player.isOnActiveLevel() && leveltype != DTYPE_TOWN && monsterIdx < MaxMonsters) {
				Monster &monster = Monsters[monsterIdx];
				monster.tag(player);
				if (monster.hitPoints > 0) {
					monster.hitPoints -= Swap32LE(message.dwDam);
					if ((monster.hitPoints >> 6) < 1)
						monster.hitPoints = 1 << 6;
					delta_monster_hp(monster, player);
				}
			}
		}
	} else {
		BufferMessage(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnPlayerDeath(const TCmdParam1 &message, Player &player)
{
	const auto deathReason = static_cast<DeathReason>(Swap16LE(message.wParam1));

	if (gbBufferMsgs != 1) {
		if (&player != MyPlayer)
			StartPlayerKill(player, deathReason);
		else
			pfile_update(true);
	} else {
		BufferMessage(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnPlayerDamage(const TCmdDamage &message, Player &player)
{
	const uint32_t damage = Swap32LE(message.dwDam);

	Player &target = Players[message.bPlr];
	if (&target == MyPlayer && leveltype != DTYPE_TOWN && gbBufferMsgs != 1) {
		if (player.isOnActiveLevel() && damage <= 192000 && target._pHitPoints >> 6 > 0) {
			ApplyPlrDamage(message.damageType, target, 0, 0, static_cast<int>(damage), DeathReason::Player);
		}
	}

	return sizeof(message);
}

size_t OnOperateObject(const TCmdLoc &message, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else {
		const WorldTilePosition position { message.x, message.y };
		assert(InDungeonBounds(position));
		if (player.isOnActiveLevel()) {
			Object *object = FindObjectAtPosition(position);
			if (object != nullptr)
				SyncOpObject(player, message.bCmd, *object);
		}
		if (player.plrlevel > 0) {
			DeltaSyncObject(position, message.bCmd, player);
		}
	}

	return sizeof(message);
}

size_t OnBreakObject(const TCmdLoc &message, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else {
		const WorldTilePosition position { message.x, message.y };
		assert(InDungeonBounds(position));
		if (player.isOnActiveLevel()) {
			Object *object = FindObjectAtPosition(position);
			if (object != nullptr)
				SyncBreakObj(player, *object);
		}
		if (player.plrlevel > 0) {
			DeltaSyncObject(position, CMD_BREAKOBJ, player);
		}
	}

	return sizeof(message);
}

size_t OnChangePlayerItems(const TCmdChItem &message, Player &player)
{
	if (message.bLoc >= NUM_INVLOC)
		return sizeof(message);

	auto bodyLocation = static_cast<inv_body_loc>(message.bLoc);

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (&player != MyPlayer && IsItemAvailable(static_cast<_item_indexes>(Swap16LE(message.def.wIndx)))) {
		Item &item = player.InvBody[message.bLoc];
		item = {};
		RecreateItem(player, message, item);
		CheckInvSwap(player, bodyLocation);
	}

	player.ReadySpellFromEquipment(bodyLocation, message.forceSpell);

	return sizeof(message);
}

size_t OnDeletePlayerItems(const TCmdDelItem &message, Player &player)
{
	if (gbBufferMsgs != 1) {
		if (&player != MyPlayer && message.bLoc < NUM_INVLOC)
			inv_update_rem_item(player, static_cast<inv_body_loc>(message.bLoc));
	} else {
		BufferMessage(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnChangeInventoryItems(const TCmdChItem &message, Player &player)
{
	if (message.bLoc >= InventoryGridCells)
		return sizeof(message);

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (&player != MyPlayer && IsItemAvailable(static_cast<_item_indexes>(Swap16LE(message.def.wIndx)))) {
		Item item {};
		RecreateItem(player, message, item);
		CheckInvSwap(player, item, message.bLoc);
	}

	return sizeof(message);
}

size_t OnDeleteInventoryItems(const TCmdParam1 &message, Player &player)
{
	const uint16_t invGridIndex = Swap16LE(message.wParam1);

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (&player != MyPlayer && invGridIndex < InventoryGridCells) {
		CheckInvRemove(player, invGridIndex);
	}

	return sizeof(message);
}

size_t OnChangeBeltItems(const TCmdChItem &message, Player &player)
{
	if (message.bLoc >= MaxBeltItems)
		return sizeof(message);

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (&player != MyPlayer && IsItemAvailable(static_cast<_item_indexes>(Swap16LE(message.def.wIndx)))) {
		Item &item = player.SpdList[message.bLoc];
		item = {};
		RecreateItem(player, message, item);
	}

	return sizeof(message);
}

size_t OnDeleteBeltItems(const TCmdParam1 &message, Player &player)
{
	const uint16_t spdBarIndex = Swap16LE(message.wParam1);

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (&player != MyPlayer && spdBarIndex < MaxBeltItems) {
		player.RemoveSpdBarItem(spdBarIndex);
	}

	return sizeof(message);
}

size_t OnPlayerLevel(const TCmdParam1 &message, Player &player)
{
	const uint16_t playerLevel = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (playerLevel <= player.getMaxCharacterLevel() && &player != MyPlayer)
			player.setCharacterLevel(static_cast<uint8_t>(playerLevel));
	} else {
		BufferMessage(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnDropItem(const TCmdPItem &message, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (IsPItemValid(message, player)) {
		DeltaPutItem(message, { message.x, message.y }, player);
	}

	return sizeof(message);
}

size_t OnSpawnItem(const TCmdPItem &message, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else if (IsPItemValid(message, player)) {
		if (player.isOnActiveLevel() && &player != MyPlayer) {
			SyncDropItem(message);
		}
		PutItemRecord(Swap32LE(message.def.dwSeed), Swap16LE(message.def.wCI), static_cast<_item_indexes>(Swap16LE(message.def.wIndx)));
		DeltaPutItem(message, { message.x, message.y }, player);
	}

	return sizeof(message);
}

size_t OnSendPlayerInfo(const TCmdPlrInfoHdr &header, size_t maxCmdSize, Player &player)
{
	const uint16_t wBytes = Swap16LE(header.wBytes);

	if (!ValidateCmdSize(wBytes + sizeof(header), maxCmdSize, player.getId()))
		return maxCmdSize;

	if (gbBufferMsgs == 1)
		BufferMessage(player, &header, wBytes + sizeof(header));
	else
		recv_plrinfo(player, header, header.bCmd == CMD_ACK_PLRINFO);

	return wBytes + sizeof(header);
}

size_t OnPlayerJoinLevel(const TCmdLocParam2 &message, Player &player)
{
	const Point position { message.x, message.y };

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
		return sizeof(message);
	}

	const auto playerLevel = static_cast<uint8_t>(Swap16LE(message.wParam1));
	const bool isSetLevel = message.wParam2 != 0;
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
		if (player.isOnActiveLevel()) {
			RemoveEnemyReferences(player);
			RemovePlrMissiles(player);
			FixPlrWalkTags(player);
		}
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
				player.AnimInfo.currentFrame = static_cast<int8_t>(player.AnimInfo.numberOfFrames - 2);
				dFlags[player.position.tile.x][player.position.tile.y] |= DungeonFlag::DeadPlayer;
			}

			ActivateVision(player.position.tile, player._pLightRad, player.getId());
		}
	}

	return sizeof(message);
}

size_t OnActivatePortal(const TCmdLocParam3 &message, Player &player)
{
	const Point position { message.x, message.y };
	const auto level = static_cast<uint8_t>(Swap16LE(message.wParam1));
	const uint16_t dungeonTypeIdx = Swap16LE(message.wParam2);
	const bool isSetLevel = message.wParam3 != 0;

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
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

size_t OnDeactivatePortal(const TCmd &cmd, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &cmd, sizeof(cmd));
	} else {
		if (PortalOnLevel(player))
			RemovePortalMissile(player);
		DeactivatePortal(player);
		delta_close_portal(player);
	}

	return sizeof(cmd);
}

size_t OnRestartTown(const TCmd &cmd, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &cmd, sizeof(cmd));
	} else {
		if (&player == MyPlayer) {
			MyPlayerIsDead = false;
			gamemenu_off();
		}
		RestartTownLvl(player);
	}

	return sizeof(cmd);
}

size_t OnSetStrength(const TCmdParam1 &message, Player &player)
{
	const uint16_t value = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (value <= 750 && &player != MyPlayer)
			SetPlrStr(player, value);
	} else {
		BufferMessage(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnSetDexterity(const TCmdParam1 &message, Player &player)
{
	const uint16_t value = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (value <= 750 && &player != MyPlayer)
			SetPlrDex(player, value);
	} else {
		BufferMessage(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnSetMagic(const TCmdParam1 &message, Player &player)
{
	const uint16_t value = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (value <= 750 && &player != MyPlayer)
			SetPlrMag(player, value);
	} else {
		BufferMessage(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnSetVitality(const TCmdParam1 &message, Player &player)
{
	const uint16_t value = Swap16LE(message.wParam1);

	if (gbBufferMsgs != 1) {
		if (value <= 750 && &player != MyPlayer)
			SetPlrVit(player, value);
	} else {
		BufferMessage(player, &message, sizeof(message));
	}

	return sizeof(message);
}

size_t OnString(const TCmd &cmd, size_t maxCmdSize, Player &player)
{
	const auto &message = reinterpret_cast<const TCmdString &>(cmd);
	const size_t headerSize = sizeof(message) - sizeof(message.str);
	const size_t maxLength = std::min<size_t>(MAX_SEND_STR_LEN, maxCmdSize - headerSize);
	const std::string_view str { message.str, maxLength };
	const auto tokens = SplitByChar(str, '\0');
	const std::string_view playerMessage = *tokens.begin();

	if (gbBufferMsgs == 0)
		SendPlrMsg(player, playerMessage);

	const size_t nullSize = str.size() != playerMessage.size() ? 1 : 0;
	return headerSize + playerMessage.size() + nullSize;
}

size_t OnFriendlyMode(const TCmd &cmd, Player &player) // NOLINT(misc-unused-parameters)
{
	player.friendlyMode = !player.friendlyMode;
	RedrawEverything();
	return sizeof(cmd);
}

size_t OnSyncQuest(const TCmdQuest &message, Player &player)
{
	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else {
		if (&player != MyPlayer && message.q < MAXQUESTS && message.qstate <= QUEST_HIVE_DONE)
			SetMultiQuest(message.q, message.qstate, message.qlog != 0, message.qvar1, message.qvar2, Swap16LE(message.qmsg));
	}

	return sizeof(message);
}

size_t OnCheatExperience(const TCmd &cmd, Player &player) // NOLINT(misc-unused-parameters)
{
#ifdef _DEBUG
	if (gbBufferMsgs == 1)
		BufferMessage(player, &cmd, sizeof(cmd));
	else if (!player.isMaxCharacterLevel()) {
		player._pExperience = player.getNextExperienceThreshold();
		if (*GetOptions().Gameplay.experienceBar) {
			RedrawEverything();
		}
		NextPlrLevel(player);
	}
#endif
	return sizeof(cmd);
}

size_t OnChangeSpellLevel(const TCmdParam2 &message, Player &player) // NOLINT(misc-unused-parameters)
{
	const auto spellID = static_cast<SpellID>(Swap16LE(message.wParam1));
	const uint8_t spellLevel = std::min(static_cast<uint8_t>(Swap16LE(message.wParam2)), MaxSpellLevel);

	if (gbBufferMsgs == 1) {
		BufferMessage(player, &message, sizeof(message));
	} else {
		player._pMemSpells |= GetSpellBitmask(spellID);
		player._pSplLvl[static_cast<size_t>(spellID)] = spellLevel;
	}

	return sizeof(message);
}

size_t OnDebug(const TCmd &pCmd)
{
	return sizeof(pCmd);
}

size_t OnSetShield(const TCmd &cmd, Player &player)
{
	if (gbBufferMsgs != 1)
		player.pManaShield = true;

	return sizeof(cmd);
}

size_t OnRemoveShield(const TCmd &cmd, Player &player)
{
	if (gbBufferMsgs != 1)
		player.pManaShield = false;

	return sizeof(cmd);
}

size_t OnSetReflect(const TCmdParam1 &message, Player &player)
{
	if (gbBufferMsgs != 1)
		player.wReflections = Swap16LE(message.wParam1);

	return sizeof(message);
}

size_t OnNakrul(const TCmd &cmd)
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
	return sizeof(cmd);
}

size_t OnOpenHive(const TCmd &cmd, Player &player)
{
	if (gbBufferMsgs != 1) {
		AddMissile({ 0, 0 }, { 0, 0 }, Direction::South, MissileID::OpenNest, TARGET_MONSTERS, player, 0, 0);
		TownOpenHive();
		InitTownTriggers();
	}

	return sizeof(cmd);
}

size_t OnOpenGrave(const TCmd &cmd)
{
	if (gbBufferMsgs != 1) {
		TownOpenGrave();
		InitTownTriggers();
		if (leveltype == DTYPE_TOWN)
			PlaySFX(SfxID::Sarcophagus);
	}
	return sizeof(cmd);
}

size_t OnSpawnMonster(const TCmdSpawnMonster &message, const Player &player)
{
	if (gbBufferMsgs == 1)
		return sizeof(message);
	if (player.plrlevel == 0)
		return sizeof(message);

	const WorldTilePosition position { message.x, message.y };

	auto typeIndex = static_cast<size_t>(Swap16LE(message.typeIndex));
	auto monsterId = static_cast<size_t>(Swap16LE(message.monsterId));
	const uint8_t golemOwnerPlayerId = message.golemOwnerPlayerId;
	if (golemOwnerPlayerId >= Players.size()) {
		return sizeof(message);
	}
	const uint8_t golemSpellLevel = std::min(message.golemSpellLevel, static_cast<uint8_t>(MaxSpellLevel + Players[golemOwnerPlayerId]._pISplLvlAdd));

	DLevel &deltaLevel = GetDeltaLevel(player);

	deltaLevel.spawnedMonsters[monsterId] = { typeIndex, message.seed, golemOwnerPlayerId, golemSpellLevel };
	// Override old monster delta information
	auto &deltaMonster = deltaLevel.monster[monsterId];
	deltaMonster.position = position;
	deltaMonster.hitPoints = -1;
	deltaMonster.menemy = 0;
	deltaMonster.mactive = 0;

	if (player.isOnActiveLevel() && &player != MyPlayer)
		InitializeSpawnedMonster(position, message.dir, typeIndex, monsterId, message.seed, golemOwnerPlayerId, golemSpellLevel);
	return sizeof(message);
}

template <typename TCmdImpl>
size_t HandleCmd(size_t (*handler)(const TCmdImpl &, size_t, Player &), Player &player, const TCmd *pCmd, size_t maxCmdSize)
{
	if (!ValidateCmdSize(sizeof(TCmdImpl), maxCmdSize, player.getId()))
		return maxCmdSize;

	const auto *message = reinterpret_cast<const TCmdImpl *>(pCmd);
	return handler(*message, maxCmdSize, player);
}

template <typename TCmdImpl>
size_t HandleCmd(size_t (*handler)(const TCmdImpl &, size_t, const Player &), const Player &player, const TCmd *pCmd, size_t maxCmdSize)
{
	if (!ValidateCmdSize(sizeof(TCmdImpl), maxCmdSize, player.getId()))
		return maxCmdSize;

	const auto *message = reinterpret_cast<const TCmdImpl *>(pCmd);
	return handler(*message, maxCmdSize, player);
}

template <typename TCmdImpl>
size_t HandleCmd(size_t (*handler)(const TCmdImpl &, Player &), Player &player, const TCmd *pCmd, size_t maxCmdSize)
{
	if (!ValidateCmdSize(sizeof(TCmdImpl), maxCmdSize, player.getId()))
		return maxCmdSize;

	const auto *message = reinterpret_cast<const TCmdImpl *>(pCmd);
	return handler(*message, player);
}

template <typename TCmdImpl>
size_t HandleCmd(size_t (*handler)(const TCmdImpl &, const Player &), const Player &player, const TCmd *pCmd, size_t maxCmdSize)
{
	if (!ValidateCmdSize(sizeof(TCmdImpl), maxCmdSize, player.getId()))
		return maxCmdSize;

	const auto *message = reinterpret_cast<const TCmdImpl *>(pCmd);
	return handler(*message, player);
}

} // namespace

void PrepareItemForNetwork(const Item &item, TItem &messageItem)
{
	messageItem.bId = item._iIdentified ? 1 : 0;
	messageItem.bDur = item._iDurability;
	messageItem.bMDur = item._iMaxDur;
	messageItem.bCh = item._iCharges;
	messageItem.bMCh = item._iMaxCharges;
	messageItem.wValue = Swap16LE(item._ivalue);
	messageItem.wToHit = Swap16LE(item._iPLToHit);
	messageItem.wMaxDam = Swap16LE(item._iMaxDam);
	messageItem.dwBuff = Swap32LE(item.dwBuff);
}

void PrepareEarForNetwork(const Item &item, TEar &ear)
{
	ear.bCursval = item._ivalue | ((item._iCurs - ICURS_EAR_SORCERER) << 6);
	CopyUtf8(ear.heroname, item._iIName, sizeof(ear.heroname));
}

void RecreateItem(const Player &player, const TItem &messageItem, Item &item)
{
	const uint32_t dwBuff = Swap32LE(messageItem.dwBuff);
	RecreateItem(player, item,
	    static_cast<_item_indexes>(Swap16LE(messageItem.wIndx)), Swap16LE(messageItem.wCI),
	    Swap32LE(messageItem.dwSeed), Swap16LE(messageItem.wValue), dwBuff);
	if (messageItem.bId != 0)
		item._iIdentified = true;
	item._iMaxDur = messageItem.bMDur;
	item._iDurability = ClampDurability(item, messageItem.bDur);
	item._iMaxCharges = std::clamp<int>(messageItem.bMCh, 0, item._iMaxCharges);
	item._iCharges = std::clamp<int>(messageItem.bCh, 0, item._iMaxCharges);
	if (gbIsHellfire) {
		item._iPLToHit = ClampToHit(item, static_cast<uint8_t>(Swap16LE(messageItem.wToHit)));
		item._iMaxDam = ClampMaxDam(item, static_cast<uint8_t>(Swap16LE(messageItem.wMaxDam)));
	}
}

void ClearLastSentPlayerCmd()
{
	lastSentPlayerCmd = {};
}

void msg_send_drop_pkt(uint8_t pnum, int reason)
{
	TFakeDropPlr cmd;

	cmd.dwReason = Swap32LE(reason);
	cmd.bCmd = FAKE_CMD_DROPID;
	cmd.bPlr = pnum;
	BufferMessage(pnum, &cmd, sizeof(cmd));
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

	if (sgbDeltaChunks != MaxChunks) {
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
		const size_t bufferSize = 1U                                                            /* marker byte, always 0 */
		    + sizeof(uint8_t)                                                                   /* level id */
		    + sizeof(deltaLevel.item)                                                           /* items spawned during dungeon generation which have been picked up, and items dropped by a player during a game */
		    + sizeof(uint8_t)                                                                   /* count of object interactions which caused a state change since dungeon generation */
		    + (sizeof(WorldTilePosition) + sizeof(DObjectStr)) * deltaLevel.object.size()       /* location/action pairs for the object interactions */
		    + sizeof(deltaLevel.monster)                                                        /* latest monster state */
		    + sizeof(uint16_t)                                                                  /* spawned monster count */
		    + (sizeof(uint16_t) + sizeof(DSpawnedMonster)) * deltaLevel.spawnedMonsters.size(); /* spawned monsters */
		const std::unique_ptr<std::byte[]> dst { new std::byte[bufferSize] };

		std::byte *dstEnd = &dst.get()[1];
		*dstEnd = static_cast<std::byte>(levelNum);
		dstEnd += sizeof(uint8_t);
		dstEnd = DeltaExportItem(dstEnd, deltaLevel.item);
		dstEnd = DeltaExportObject(dstEnd, deltaLevel.object);
		dstEnd = DeltaExportMonster(dstEnd, deltaLevel.monster);
		dstEnd = DeltaExportSpawnedMonsters(dstEnd, deltaLevel.spawnedMonsters);
		const uint32_t size = CompressData(dst.get(), dstEnd);
		multi_send_zero_packet(pnum, CMD_DLEVEL, dst.get(), size);
	}

	std::byte dst[sizeof(DJunk) + 1];
	std::byte *dstEnd = &dst[1];
	dstEnd = DeltaExportJunk(dstEnd);
	const uint32_t size = CompressData(dst, dstEnd);
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
	if (SwapSigned32LE(pD->hitPoints) > monster.hitPoints)
		pD->hitPoints = SwapSigned32LE(monster.hitPoints);
}

void delta_sync_monster(const TSyncMonster &monsterSync, uint8_t level)
{
	if (!gbIsMultiplayer)
		return;

	assert(level <= MaxMultiplayerLevels);

	DMonsterStr &monster = GetDeltaLevel(level).monster[monsterSync._mndx];
	if (monster.hitPoints == 0)
		return;

	monster.position.x = monsterSync._mx;
	monster.position.y = monsterSync._my;
	monster.mactive = UINT8_MAX;
	monster.menemy = monsterSync._menemy;
	monster.hitPoints = monsterSync._mhitpoints;
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
			    sgJunk.portal[i].setlvl != 0);
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
			quest._qmsg = static_cast<_speech_id>(Swap16LE(sgJunk.quests[q].qmsg));
		}
		q++;
	}
}

void DeltaAddItem(int ii)
{
	if (!gbIsMultiplayer)
		return;

	const uint8_t localLevel = GetLevelForMultiplayer(*MyPlayer);
	DLevel &deltaLevel = GetDeltaLevel(localLevel);

	for (const TCmdPItem &item : deltaLevel.item) {
		if (item.bCmd != CMD_INVALID
		    && static_cast<_item_indexes>(Swap16LE(item.def.wIndx)) == Items[ii].IDidx
		    && Swap16LE(item.def.wCI) == Items[ii]._iCreateInfo
		    && static_cast<uint32_t>(Swap32LE(item.def.dwSeed)) == Items[ii]._iSeed
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
	return level <= MaxMultiplayerLevels;
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

	const uint8_t localLevel = GetLevelForMultiplayer(*MyPlayer);
	DLevel &deltaLevel = GetDeltaLevel(localLevel);
	if (leveltype != DTYPE_TOWN) {
		DeltaLoadSpawnedMonsters(deltaLevel);
		DeltaLoadMonsters(deltaLevel);

		auto localLevelIt = LocalLevels.find(localLevel);
		if (localLevelIt != LocalLevels.end())
			memcpy(AutomapView, &localLevelIt->second, sizeof(AutomapView));
		else
			memset(AutomapView, 0, sizeof(AutomapView));

		DeltaLoadObjects(deltaLevel);
	}

	DeltaLoadItems(deltaLevel);
}

void NetSendCmd(bool bHiPri, _cmd_id bCmd)
{
	TCmd cmd;

	cmd.bCmd = bCmd;
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

void NetSendCmdSpawnMonster(Point position, Direction dir, uint16_t typeIndex, uint16_t monsterId, uint32_t seed, uint8_t golemOwnerPlayerId, uint8_t golemSpellLevel)
{
	TCmdSpawnMonster cmd;

	cmd.bCmd = CMD_SPAWNMONSTER;
	cmd.x = position.x;
	cmd.y = position.y;
	cmd.dir = dir;
	cmd.typeIndex = Swap16LE(typeIndex);
	cmd.monsterId = Swap16LE(monsterId);
	cmd.seed = Swap32LE(seed);
	cmd.golemOwnerPlayerId = golemOwnerPlayerId;
	cmd.golemSpellLevel = golemSpellLevel;
	NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
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
		NetSendHiPri(playerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(playerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));

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
	cmd.wParam1 = Swap16LE(wParam1);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));

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
	cmd.wParam1 = Swap16LE(wParam1);
	cmd.wParam2 = Swap16LE(wParam2);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));

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
	cmd.wParam1 = Swap16LE(wParam1);
	cmd.wParam2 = Swap16LE(wParam2);
	cmd.wParam3 = Swap16LE(wParam3);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));

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
	cmd.wParam1 = Swap16LE(wParam1);
	cmd.wParam2 = Swap16LE(wParam2);
	cmd.wParam3 = Swap16LE(wParam3);
	cmd.wParam4 = Swap16LE(wParam4);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));

	MyPlayer->UpdatePreviewCelSprite(bCmd, position, wParam1, wParam3);
}

void NetSendCmdParam1(bool bHiPri, _cmd_id bCmd, uint16_t wParam1)
{
	if (WasPlayerCmdAlreadyRequested(bCmd, {}, wParam1))
		return;

	TCmdParam1 cmd;

	cmd.bCmd = bCmd;
	cmd.wParam1 = Swap16LE(wParam1);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));

	MyPlayer->UpdatePreviewCelSprite(bCmd, {}, wParam1, 0);
}

void NetSendCmdParam2(bool bHiPri, _cmd_id bCmd, uint16_t wParam1, uint16_t wParam2)
{
	TCmdParam2 cmd;

	cmd.bCmd = bCmd;
	cmd.wParam1 = Swap16LE(wParam1);
	cmd.wParam2 = Swap16LE(wParam2);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

void NetSendCmdParam4(bool bHiPri, _cmd_id bCmd, uint16_t wParam1, uint16_t wParam2, uint16_t wParam3, uint16_t wParam4)
{
	if (WasPlayerCmdAlreadyRequested(bCmd, {}, wParam1, wParam2, wParam3, wParam4))
		return;

	TCmdParam4 cmd;

	cmd.bCmd = bCmd;
	cmd.wParam1 = Swap16LE(wParam1);
	cmd.wParam2 = Swap16LE(wParam2);
	cmd.wParam3 = Swap16LE(wParam3);
	cmd.wParam4 = Swap16LE(wParam4);
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));

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
	cmd.qmsg = Swap16LE(quest._qmsg);

	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

void NetSendCmdGItem(bool bHiPri, _cmd_id bCmd, const Player &player, uint8_t ii)
{
	const uint8_t pnum = player.getId();

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
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
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
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

void NetSendCmdChItem(bool bHiPri, uint8_t bLoc, bool forceSpellChange)
{
	TCmdChItem cmd {};

	const Item &item = MyPlayer->InvBody[bLoc];

	cmd.bCmd = CMD_CHANGEPLRITEMS;
	cmd.bLoc = bLoc;
	cmd.forceSpell = forceSpellChange;
	PrepareItemForNetwork(item, cmd);

	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

void NetSendCmdDelItem(bool bHiPri, uint8_t bLoc)
{
	TCmdDelItem cmd;

	cmd.bLoc = bLoc;
	cmd.bCmd = CMD_DELPLRITEMS;
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
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

	const int invListIndex = std::abs(MyPlayer->InvGrid[invGridIndex]) - 1;
	const Item &item = MyPlayer->InvList[invListIndex];

	cmd.bCmd = CMD_CHANGEINVITEMS;
	cmd.bLoc = invGridIndex;
	PrepareItemForNetwork(item, cmd);

	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

void NetSendCmdChBeltItem(bool bHiPri, int beltIndex)
{
	TCmdChItem cmd {};

	const Item &item = MyPlayer->SpdList[beltIndex];

	cmd.bCmd = CMD_CHANGEBELTITEMS;
	cmd.bLoc = beltIndex;
	PrepareItemForNetwork(item, cmd);

	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

void NetSendCmdDamage(bool bHiPri, const Player &player, uint32_t dwDam, DamageType damageType)
{
	TCmdDamage cmd;

	cmd.bCmd = CMD_PLRDAMAGE;
	cmd.bPlr = player.getId();
	cmd.dwDam = dwDam;
	cmd.damageType = damageType;
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

void NetSendCmdMonDmg(bool bHiPri, uint16_t wMon, uint32_t dwDam)
{
	TCmdMonDamage cmd;

	cmd.bCmd = CMD_MONSTDAMAGE;
	cmd.wMon = wMon;
	cmd.dwDam = dwDam;
	if (bHiPri)
		NetSendHiPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
	else
		NetSendLoPri(MyPlayerId, reinterpret_cast<std::byte *>(&cmd), sizeof(cmd));
}

void NetSendCmdString(uint32_t pmask, const char *pszStr)
{
	TCmdString cmd;

	cmd.bCmd = CMD_STRING;
	CopyUtf8(cmd.str, pszStr, sizeof(cmd.str));
	multi_send_msg_packet(pmask, reinterpret_cast<std::byte *>(&cmd), strlen(cmd.str) + 2);
}

void delta_close_portal(const Player &player)
{
	memset(&sgJunk.portal[player.getId()], 0xFF, sizeof(sgJunk.portal[player.getId()]));
}

bool ValidateCmdSize(size_t requiredCmdSize, size_t maxCmdSize, size_t playerId)
{
	if (requiredCmdSize <= maxCmdSize)
		return true;

	Log("Suspiciously small packet size, dropping player {}", playerId);
	SNetDropPlayer(static_cast<uint8_t>(playerId), LEAVE_DROP);
	return false;
}

size_t ParseCmd(uint8_t pnum, const TCmd *pCmd, size_t maxCmdSize)
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
		return HandleCmd(OnSyncData, player, pCmd, maxCmdSize);
	case CMD_WALKXY:
		return HandleCmd(OnWalk, player, pCmd, maxCmdSize);
	case CMD_ADDSTR:
		return HandleCmd(OnAddStrength, player, pCmd, maxCmdSize);
	case CMD_ADDDEX:
		return HandleCmd(OnAddDexterity, player, pCmd, maxCmdSize);
	case CMD_ADDMAG:
		return HandleCmd(OnAddMagic, player, pCmd, maxCmdSize);
	case CMD_ADDVIT:
		return HandleCmd(OnAddVitality, player, pCmd, maxCmdSize);
	case CMD_GOTOGETITEM:
		return HandleCmd(OnGotoGetItem, player, pCmd, maxCmdSize);
	case CMD_REQUESTGITEM:
		return HandleCmd(OnRequestGetItem, player, pCmd, maxCmdSize);
	case CMD_GETITEM:
		return HandleCmd(OnGetItem, player, pCmd, maxCmdSize);
	case CMD_GOTOAGETITEM:
		return HandleCmd(OnGotoAutoGetItem, player, pCmd, maxCmdSize);
	case CMD_REQUESTAGITEM:
		return HandleCmd(OnRequestAutoGetItem, player, pCmd, maxCmdSize);
	case CMD_AGETITEM:
		return HandleCmd(OnAutoGetItem, player, pCmd, maxCmdSize);
	case CMD_ITEMEXTRA:
		return HandleCmd(OnItemExtra, player, pCmd, maxCmdSize);
	case CMD_PUTITEM:
		return HandleCmd(OnPutItem, player, pCmd, maxCmdSize);
	case CMD_SYNCPUTITEM:
		return HandleCmd(OnSyncPutItem, player, pCmd, maxCmdSize);
	case CMD_SPAWNITEM:
		return HandleCmd(OnSpawnItem, player, pCmd, maxCmdSize);
	case CMD_ATTACKXY:
		return HandleCmd(OnAttackTile, player, pCmd, maxCmdSize);
	case CMD_SATTACKXY:
		return HandleCmd(OnStandingAttackTile, player, pCmd, maxCmdSize);
	case CMD_RATTACKXY:
		return HandleCmd(OnRangedAttackTile, player, pCmd, maxCmdSize);
	case CMD_SPELLXYD:
		return HandleCmd(OnSpellWall, player, pCmd, maxCmdSize);
	case CMD_SPELLXY:
		return HandleCmd(OnSpellTile, player, pCmd, maxCmdSize);
	case CMD_OPOBJXY:
	case CMD_DISARMXY:
	case CMD_OPOBJT:
		return HandleCmd(OnObjectTileAction, player, pCmd, maxCmdSize);
	case CMD_ATTACKID:
		return HandleCmd(OnAttackMonster, player, pCmd, maxCmdSize);
	case CMD_ATTACKPID:
		return HandleCmd(OnAttackPlayer, player, pCmd, maxCmdSize);
	case CMD_RATTACKID:
		return HandleCmd(OnRangedAttackMonster, player, pCmd, maxCmdSize);
	case CMD_RATTACKPID:
		return HandleCmd(OnRangedAttackPlayer, player, pCmd, maxCmdSize);
	case CMD_SPELLID:
		return HandleCmd(OnSpellMonster, player, pCmd, maxCmdSize);
	case CMD_SPELLPID:
		return HandleCmd(OnSpellPlayer, player, pCmd, maxCmdSize);
	case CMD_KNOCKBACK:
		return HandleCmd(OnKnockback, player, pCmd, maxCmdSize);
	case CMD_RESURRECT:
		return HandleCmd(OnResurrect, player, pCmd, maxCmdSize);
	case CMD_HEALOTHER:
		return HandleCmd(OnHealOther, player, pCmd, maxCmdSize);
	case CMD_TALKXY:
		return HandleCmd(OnTalkXY, player, pCmd, maxCmdSize);
	case CMD_DEBUG:
		return OnDebug(*pCmd);
	case CMD_NEWLVL:
		return HandleCmd(OnNewLevel, player, pCmd, maxCmdSize);
	case CMD_WARP:
		return HandleCmd(OnWarp, player, pCmd, maxCmdSize);
	case CMD_MONSTDEATH:
		return HandleCmd(OnMonstDeath, player, pCmd, maxCmdSize);
	case CMD_REQUESTSPAWNGOLEM:
		return HandleCmd(OnRequestSpawnGolem, player, pCmd, maxCmdSize);
	case CMD_MONSTDAMAGE:
		return HandleCmd(OnMonstDamage, player, pCmd, maxCmdSize);
	case CMD_PLRDEAD:
		return HandleCmd(OnPlayerDeath, player, pCmd, maxCmdSize);
	case CMD_PLRDAMAGE:
		return HandleCmd(OnPlayerDamage, player, pCmd, maxCmdSize);
	case CMD_OPENDOOR:
	case CMD_CLOSEDOOR:
	case CMD_OPERATEOBJ:
		return HandleCmd(OnOperateObject, player, pCmd, maxCmdSize);
	case CMD_BREAKOBJ:
		return HandleCmd(OnBreakObject, player, pCmd, maxCmdSize);
	case CMD_CHANGEPLRITEMS:
		return HandleCmd(OnChangePlayerItems, player, pCmd, maxCmdSize);
	case CMD_DELPLRITEMS:
		return HandleCmd(OnDeletePlayerItems, player, pCmd, maxCmdSize);
	case CMD_CHANGEINVITEMS:
		return HandleCmd(OnChangeInventoryItems, player, pCmd, maxCmdSize);
	case CMD_DELINVITEMS:
		return HandleCmd(OnDeleteInventoryItems, player, pCmd, maxCmdSize);
	case CMD_CHANGEBELTITEMS:
		return HandleCmd(OnChangeBeltItems, player, pCmd, maxCmdSize);
	case CMD_DELBELTITEMS:
		return HandleCmd(OnDeleteBeltItems, player, pCmd, maxCmdSize);
	case CMD_PLRLEVEL:
		return HandleCmd(OnPlayerLevel, player, pCmd, maxCmdSize);
	case CMD_DROPITEM:
		return HandleCmd(OnDropItem, player, pCmd, maxCmdSize);
	case CMD_ACK_PLRINFO:
	case CMD_SEND_PLRINFO:
		return HandleCmd(OnSendPlayerInfo, player, pCmd, maxCmdSize);
	case CMD_PLAYER_JOINLEVEL:
		return HandleCmd(OnPlayerJoinLevel, player, pCmd, maxCmdSize);
	case CMD_ACTIVATEPORTAL:
		return HandleCmd(OnActivatePortal, player, pCmd, maxCmdSize);
	case CMD_DEACTIVATEPORTAL:
		return OnDeactivatePortal(*pCmd, player);
	case CMD_RETOWN:
		return OnRestartTown(*pCmd, player);
	case CMD_SETSTR:
		return HandleCmd(OnSetStrength, player, pCmd, maxCmdSize);
	case CMD_SETMAG:
		return HandleCmd(OnSetMagic, player, pCmd, maxCmdSize);
	case CMD_SETDEX:
		return HandleCmd(OnSetDexterity, player, pCmd, maxCmdSize);
	case CMD_SETVIT:
		return HandleCmd(OnSetVitality, player, pCmd, maxCmdSize);
	case CMD_STRING:
		return OnString(*pCmd, maxCmdSize, player);
	case CMD_FRIENDLYMODE:
		return OnFriendlyMode(*pCmd, player);
	case CMD_SYNCQUEST:
		return HandleCmd(OnSyncQuest, player, pCmd, maxCmdSize);
	case CMD_CHEAT_EXPERIENCE:
		return OnCheatExperience(*pCmd, player);
	case CMD_CHANGE_SPELL_LEVEL:
		return HandleCmd(OnChangeSpellLevel, player, pCmd, maxCmdSize);
	case CMD_SETSHIELD:
		return OnSetShield(*pCmd, player);
	case CMD_REMSHIELD:
		return OnRemoveShield(*pCmd, player);
	case CMD_SETREFLECT:
		return HandleCmd(OnSetReflect, player, pCmd, maxCmdSize);
	case CMD_NAKRUL:
		return OnNakrul(*pCmd);
	case CMD_OPENHIVE:
		return OnOpenHive(*pCmd, player);
	case CMD_OPENGRAVE:
		return OnOpenGrave(*pCmd);
	case CMD_SPAWNMONSTER:
		return HandleCmd(OnSpawnMonster, player, pCmd, maxCmdSize);
	default:
		break;
	}

	if (pCmd->bCmd < CMD_DLEVEL || pCmd->bCmd > CMD_DLEVEL_END) {
		Log("Unrecognized network message {}, dropping player {}", static_cast<uint8_t>(pCmd->bCmd), pnum);
		SNetDropPlayer(pnum, LEAVE_DROP);
		return 0;
	}

	return HandleCmd(OnLevelData, player, pCmd, maxCmdSize);
}

} // namespace devilution
