#pragma once

#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "multi.h"
#include "storm/storm_net.hpp"

namespace devilution::net {

using buffer_t = std::vector<unsigned char>;
using provider_t = unsigned long;

class abstract_net {
public:
	virtual int create(std::string_view addrstr) = 0;
	virtual int join(std::string_view addrstr) = 0;
	virtual bool SNetReceiveMessage(uint8_t *sender, void **data, size_t *size) = 0;
	virtual bool SNetSendMessage(uint8_t dest, void *data, size_t size) = 0;
	virtual bool SNetReceiveTurns(char **data, size_t *size, uint32_t *status) = 0;
	virtual bool SNetSendTurn(char *data, size_t size) = 0;
	virtual void SNetGetProviderCaps(struct _SNETCAPS *caps) = 0;
	virtual bool SNetRegisterEventHandler(event_type evtype, SEVTHANDLER func) = 0;
	virtual bool SNetUnregisterEventHandler(event_type evtype) = 0;
	virtual bool SNetLeaveGame(int type) = 0;
	virtual bool SNetDropPlayer(int playerid, uint32_t flags) = 0;
	virtual bool SNetGetOwnerTurnsWaiting(uint32_t *turns) = 0;
	virtual bool SNetGetTurnsInTransit(uint32_t *turns) = 0;
	virtual void setup_gameinfo(buffer_t info) = 0;
	virtual ~abstract_net() = default;

	virtual std::string make_default_gamename() = 0;

	virtual void setup_password(std::string passwd)
	{
	}

	virtual void clear_password()
	{
	}

	virtual bool send_info_request()
	{
		return true;
	}

	virtual void clear_gamelist()
	{
	}

	virtual std::vector<GameInfo> get_gamelist()
	{
		return std::vector<GameInfo>();
	}

	virtual DvlNetLatencies get_latencies(uint8_t playerid)
	{
		return {};
	}

	static std::unique_ptr<abstract_net> MakeNet(provider_t provider);
};

} // namespace devilution::net
