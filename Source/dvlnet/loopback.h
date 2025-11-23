#pragma once

#include <cstdint>
#include <queue>
#include <string>

#include "dvlnet/abstract_net.h"

namespace devilution::net {

class loopback : public abstract_net {
private:
	std::queue<buffer_t> message_queue;
	buffer_t message_last;
	uint8_t plr_single = 0;

public:
	loopback() = default;

	int create(std::string_view addrstr) override;
	int join(std::string_view addrstr) override;
	bool SNetReceiveMessage(uint8_t *sender, void **data, size_t *size) override;
	bool SNetSendMessage(uint8_t dest, void *data, size_t size) override;
	bool SNetReceiveTurns(char **data, size_t *size, uint32_t *status) override;
	bool SNetSendTurn(char *data, size_t size) override;
	void SNetGetProviderCaps(struct _SNETCAPS *caps) override;
	bool SNetRegisterEventHandler(event_type evtype, SEVTHANDLER func) override;
	bool SNetUnregisterEventHandler(event_type evtype) override;
	bool SNetLeaveGame(net::leaveinfo_t type) override;
	bool SNetDropPlayer(int playerid, net::leaveinfo_t flags) override;
	bool SNetGetOwnerTurnsWaiting(uint32_t *turns) override;
	bool SNetGetTurnsInTransit(uint32_t *turns) override;
	void setup_gameinfo(buffer_t info) override;
	std::string make_default_gamename() override;
};

} // namespace devilution::net
