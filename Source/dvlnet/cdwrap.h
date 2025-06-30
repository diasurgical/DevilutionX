#pragma once

#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <ankerl/unordered_dense.h>
#include <function_ref.hpp>

#include "dvlnet/abstract_net.h"
#include "storm/storm_net.hpp"

namespace devilution::net {

class cdwrap : public abstract_net {
private:
	std::unique_ptr<abstract_net> dvlnet_wrap;
	ankerl::unordered_dense::map<event_type, SEVTHANDLER> registered_handlers;
	buffer_t game_init_info;
	std::optional<std::string> game_pw;
	tl::function_ref<std::unique_ptr<abstract_net>()> make_net_fn_;

	void reset();

public:
	explicit cdwrap(tl::function_ref<std::unique_ptr<abstract_net>()> makeNetFn)
	    : make_net_fn_(makeNetFn)
	{
		reset();
	}

	int create(std::string_view addrstr) override;
	int join(std::string_view addrstr) override;
	bool SNetReceiveMessage(uint8_t *sender, void **data, size_t *size) override;
	bool SNetSendMessage(uint8_t dest, void *data, size_t size) override;
	bool SNetReceiveTurns(char **data, size_t *size, uint32_t *status) override;
	bool SNetSendTurn(char *data, size_t size) override;
	void SNetGetProviderCaps(struct _SNETCAPS *caps) override;
	bool SNetRegisterEventHandler(event_type evtype, SEVTHANDLER func) override;
	bool SNetUnregisterEventHandler(event_type evtype) override;
	bool SNetLeaveGame(int type) override;
	bool SNetDropPlayer(int playerid, uint32_t flags) override;
	bool SNetGetOwnerTurnsWaiting(uint32_t *turns) override;
	bool SNetGetTurnsInTransit(uint32_t *turns) override;
	void setup_gameinfo(buffer_t info) override;
	std::string make_default_gamename() override;
	bool send_info_request() override;
	void clear_gamelist() override;
	std::vector<GameInfo> get_gamelist() override;
	void setup_password(std::string pw) override;
	void clear_password() override;
	DvlNetLatencies get_latencies(uint8_t playerid) override;

	virtual ~cdwrap() = default;
};

} // namespace devilution::net
