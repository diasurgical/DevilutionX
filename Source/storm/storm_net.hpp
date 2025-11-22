#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "multi.h"

namespace devilution {

enum conn_type : uint8_t {
	SELCONN_ZT,
	SELCONN_TCP,
	SELCONN_LOOPBACK,
};

enum event_type : uint8_t {
	EVENT_TYPE_PLAYER_CREATE_GAME,
	EVENT_TYPE_PLAYER_LEAVE_GAME,
	EVENT_TYPE_PLAYER_MESSAGE,
};

extern const char *ConnectionNames[];

struct _SNETCAPS {
	uint32_t size;
	uint32_t flags;
	uint32_t maxmessagesize;
	uint32_t maxqueuesize;
	uint32_t maxplayers;
	uint32_t bytessec;
	uint32_t latencyms;
	uint32_t defaultturnssec;
	uint32_t defaultturnsintransit;
};

struct _SNETEVENT {
	uint32_t eventid;  // native-endian
	uint32_t playerid; // native-endian
	void *data;        // little-endian
	size_t databytes;  // native-endian
};

struct DvlNetLatencies {
	uint32_t echoLatency;
	std::optional<int> providerLatency;
	std::optional<bool> isRelayed;
};

#define PS_CONNECTED 0x10000
#define PS_TURN_ARRIVED 0x20000
#define PS_ACTIVE 0x40000

#define LEAVE_ENDING 0x40000004
#define LEAVE_DROP 0x40000006

bool SNetCreateGame(const char *pszGameName, const char *pszGamePassword, char *GameTemplateData, int GameTemplateSize, int *playerID);
bool SNetDestroy();

/*  SNetDropPlayer @ 106
 *
 *  Drops a player from the current game.
 *
 *  playerid:     The player ID for the player to be dropped.
 *  flags:
 *
 *  Returns true if the function was called successfully and false otherwise.
 */
bool SNetDropPlayer(uint8_t playerid, uint32_t flags);

/*  SNetGetTurnsInTransit @ 115
 *
 *  Retrieves the number of turns (buffers) that have been queued
 *  before sending them over the network.
 *
 *  turns: A pointer to an integer that will receive the value.
 *
 *  Returns true if the function was called successfully and false otherwise.
 */
bool SNetGetTurnsInTransit(uint32_t *turns);

bool SNetJoinGame(char *gameName, char *gamePassword, int *playerid);

/*  SNetLeaveGame @ 119
 *
 *  Notifies Storm that the player has left the game. Storm will
 *  notify all connected peers through the network provider.
 *
 *  type: The leave type. It doesn't appear to be important, no documentation available.
 *
 *  Returns true if the function was called successfully and false otherwise.
 */
bool SNetLeaveGame(int type);

bool SNetReceiveMessage(uint8_t *senderplayerid, void **data, size_t *databytes);
bool SNetReceiveTurns(int arraysize, char **arraydata, size_t *arraydatabytes, uint32_t *arrayplayerstatus);

typedef void (*SEVTHANDLER)(struct _SNETEVENT *);

/*  SNetSendMessage @ 127
 *
 *  Sends a message to a player given their player ID. Network message
 *  is sent using class 01 and is retrieved by the other client using
 *  SNetReceiveMessage().
 *
 *  playerID:   The player index of the player to receive the data.
 *              Conversely, this field can be one of the following constants:
 *                  SNPLAYER_ALL      | Sends the message to all players, including oneself.
 *                  SNPLAYER_OTHERS   | Sends the message to all players, except for oneself.
 *  data:       A pointer to the data.
 *  databytes:  The amount of bytes that the data pointer contains.
 *
 *  Returns true if the function was called successfully and false otherwise.
 */
bool SNetSendMessage(uint8_t playerID, void *data, size_t databytes);

// Macro values to target specific players
constexpr uint8_t SNPLAYER_OTHERS = 0xFF;

/*  SNetSendTurn @ 128
 *
 *  Sends a turn (data packet) to all players in the game. Network data
 *  is sent using class 02 and is retrieved by the other client using
 *  SNetReceiveTurns().
 *
 *  data:       A pointer to the data.
 *  databytes:  The amount of bytes that the data pointer contains.
 *
 *  Returns true if the function was called successfully and false otherwise.
 */
bool SNetSendTurn(char *data, size_t databytes);

bool SNetGetOwnerTurnsWaiting(uint32_t *);
bool SNetUnregisterEventHandler(event_type);
bool SNetRegisterEventHandler(event_type, SEVTHANDLER);
bool SNetSetBasePlayer(int);
bool SNetInitializeProvider(uint32_t provider, struct GameData *gameData);
void SNetGetProviderCaps(struct _SNETCAPS *);

bool DvlNet_SendInfoRequest();
void DvlNet_ClearGamelist();
std::vector<GameInfo> DvlNet_GetGamelist();
void DvlNet_SetPassword(std::string pw);
void DvlNet_ClearPassword();
bool DvlNet_IsPublicGame();
DvlNetLatencies DvlNet_GetLatencies(uint8_t playerId);

} // namespace devilution
