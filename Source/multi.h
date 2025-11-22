/**
 * @file multi.h
 *
 * Interface of functions for keeping multiplayer games in sync.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "msg.h"
#include "utils/attributes.h"

namespace devilution {

// Defined in player.h, forward declared here to allow for functions which operate in the context of a player.
struct Player;

// must be unsigned to generate unsigned comparisons with pnum
#define MAX_PLRS 4

struct GameData {
	int32_t size;
	uint8_t reserved[4];
	uint32_t programid;
	uint8_t versionMajor;
	uint8_t versionMinor;
	uint8_t versionPatch;
	_difficulty nDifficulty;
	uint8_t nTickRate;
	uint8_t bRunInTown;
	uint8_t bTheoQuest;
	uint8_t bCowQuest;
	uint8_t bFriendlyFire;
	uint8_t fullQuests;
	/** Used to initialise the seed table for dungeon levels so players in multiplayer games generate the same layout */
	uint32_t gameSeed[4];

	void swapLE();
};

/* @brief Contains info of running public game (for game list browsing) */
struct GameInfo {
	std::string name;
	GameData gameData;
	std::vector<std::string> players;
	std::optional<int> latency;
	std::optional<bool> peerIsRelayed;
};

extern bool gbSomebodyWonGameKludge;
extern uint16_t sgwPackPlrOffsetTbl[MAX_PLRS];
extern uint8_t gbActivePlayers;
extern bool gbGameDestroyed;
extern DVL_API_FOR_TEST GameData sgGameInitInfo;
extern bool gbSelectProvider;
extern DVL_API_FOR_TEST bool gbIsMultiplayer;
extern std::string GameName;
extern std::string GamePassword;
extern bool PublicGame;
extern uint8_t gbDeltaSender;
extern uint32_t player_state[MAX_PLRS];
extern bool IsLoopback;

void InitGameInfo();
void NetSendLoPri(uint8_t playerId, const std::byte *data, size_t size);
void NetSendHiPri(uint8_t playerId, const std::byte *data, size_t size);
void multi_send_msg_packet(uint32_t pmask, const std::byte *data, size_t size);
void multi_msg_countdown();
void multi_player_left(uint8_t pnum, int reason);
void multi_net_ping();

/**
 * @return Always true for singleplayer
 */
bool multi_handle_delta();
void multi_process_network_packets();
void multi_send_zero_packet(uint8_t pnum, _cmd_id bCmd, const std::byte *data, size_t size);
void NetClose();
bool NetInit(bool bSinglePlayer);
void recv_plrinfo(Player &player, const TCmdPlrInfoHdr &header, bool recv);

} // namespace devilution
