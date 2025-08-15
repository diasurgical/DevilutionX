#include "dvlnet/loopback.h"

#include <cstdint>

#include "multi.h"
#include "player.h"
#include "utils/language.h"
#include "utils/stubs.h"

namespace devilution::net {

int loopback::create(std::string_view /*addrstr*/)
{
	IsLoopback = true;
	return plr_single;
}

int loopback::join(std::string_view /*addrstr*/)
{
	ABORT();
}

bool loopback::SNetReceiveMessage(uint8_t *sender, void **data, size_t *size)
{
	if (message_queue.empty())
		return false;
	message_last = message_queue.front();
	message_queue.pop();
	*sender = plr_single;
	*size = message_last.size();
	*data = message_last.data();
	return true;
}

bool loopback::SNetSendMessage(uint8_t dest, void *data, size_t size)
{
	if (dest == plr_single) {
		auto *rawMessage = reinterpret_cast<unsigned char *>(data);
		const buffer_t message(rawMessage, rawMessage + size);
		message_queue.push(message);
	}
	return true;
}

bool loopback::SNetReceiveTurns(char **data, size_t *size, uint32_t * /*status*/)
{
	for (size_t i = 0; i < Players.size(); ++i) {
		size[i] = 0;
		data[i] = nullptr;
	}
	return true;
}

bool loopback::SNetSendTurn(char * /*data*/, size_t /*size*/)
{
	return true;
}

void loopback::SNetGetProviderCaps(struct _SNETCAPS *caps)
{
	caps->size = 0;                  // engine writes only ?!?
	caps->flags = 0;                 // unused
	caps->maxmessagesize = 512;      // capped to 512; underflow if < 24
	caps->maxqueuesize = 0;          // unused
	caps->maxplayers = MAX_PLRS;     // capped to 4
	caps->bytessec = 1000000;        // ?
	caps->latencyms = 0;             // unused
	caps->defaultturnssec = 10;      // ?
	caps->defaultturnsintransit = 1; // maximum acceptable number
	                                 // of turns in queue?
}

bool loopback::SNetRegisterEventHandler(event_type /*evtype*/,
    SEVTHANDLER /*func*/)
{
	// not called in real singleplayer mode
	// not needed in pseudo multiplayer mode (?)
	return true;
}

bool loopback::SNetUnregisterEventHandler(event_type /*evtype*/)
{
	// not called in real singleplayer mode
	// not needed in pseudo multiplayer mode (?)
	return true;
}

bool loopback::SNetLeaveGame(int /*type*/)
{
	IsLoopback = false;
	return true;
}

bool loopback::SNetDropPlayer(int /*playerid*/, uint32_t /*flags*/)
{
	return true;
}

void loopback::setup_gameinfo(buffer_t info)
{
}

bool loopback::SNetGetOwnerTurnsWaiting(uint32_t *turns)
{
	*turns = 0;
	return true;
}

bool loopback::SNetGetTurnsInTransit(uint32_t *turns)
{
	*turns = 0;
	return true;
}

std::string loopback::make_default_gamename()
{
	return std::string(_("loopback"));
}

} // namespace devilution::net
