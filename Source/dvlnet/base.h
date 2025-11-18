#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include <ankerl/unordered_dense.h>

#include "dvlnet/abstract_net.h"
#include "dvlnet/packet.h"
#include "multi.h"
#include "storm/storm_net.hpp"

namespace devilution {
namespace net {

class base : public abstract_net {
public:
	bool SNetReceiveMessage(uint8_t *sender, void **data, size_t *size) override;
	bool SNetSendMessage(uint8_t playerId, void *data, size_t size) override;
	bool SNetReceiveTurns(char **data, size_t *size, uint32_t *status) override;
	bool SNetSendTurn(char *data, size_t size) override;
	void SNetGetProviderCaps(struct _SNETCAPS *caps) override;
	bool SNetRegisterEventHandler(event_type evtype, SEVTHANDLER func) override;
	bool SNetUnregisterEventHandler(event_type evtype) override;
	bool SNetLeaveGame(int type) override;
	bool SNetDropPlayer(int playerid, uint32_t flags) override;
	bool SNetGetOwnerTurnsWaiting(uint32_t *turns) override;
	bool SNetGetTurnsInTransit(uint32_t *turns) override;

	virtual tl::expected<void, PacketError> poll() = 0;
	virtual tl::expected<void, PacketError> send(packet &pkt) = 0;
	virtual void DisconnectNet(plr_t plr);

	void setup_gameinfo(buffer_t info) override;

	void setup_password(std::string pw) override;
	void clear_password() override;

	DvlNetLatencies get_latencies(uint8_t playerid) override;

	~base() override = default;

protected:
	ankerl::unordered_dense::map<event_type, SEVTHANDLER> registered_handlers;
	buffer_t game_init_info;

	struct message_t {
		uint8_t sender;
		buffer_t payload;
		message_t()
		    : sender(-1)
		    , payload({})
		{
		}
		message_t(int s, buffer_t p)
		    : sender(s)
		    , payload(p)
		{
		}
	};

	struct PlayerState {
		bool isConnected = {};
		std::deque<turn_t> turnQueue;
		int32_t lastTurnValue = {};
		uint32_t roundTripLatency = {};
	};

	seq_t current_turn = 0;
	seq_t next_turn = 0;
	message_t message_last;
	std::deque<message_t> message_queue;

	plr_t plr_self = PLR_BROADCAST;
	cookie_t cookie_self = 0;

	std::unique_ptr<packet_factory> pktfty;

	tl::expected<void, PacketError> Connect(plr_t player);
	tl::expected<void, PacketError> RecvLocal(packet &pkt);
	void RunEventHandler(_SNETEVENT &ev);
	tl::expected<void, PacketError> SendEchoRequest(plr_t player);

	[[nodiscard]] bool IsConnected(plr_t player) const;
	virtual bool IsGameHost() = 0;

private:
	std::array<PlayerState, MAX_PLRS> playerStateTable_;
	bool awaitingSequenceNumber_ = true;
	uint32_t lastEchoTime = 0;

	plr_t GetOwner();
	bool AllTurnsArrived();
	tl::expected<void, PacketError> MakeReady(seq_t sequenceNumber);
	tl::expected<void, PacketError> SendTurnIfReady(turn_t turn);
	tl::expected<void, PacketError> SendFirstTurnIfReady(plr_t player);
	void ClearMsg(plr_t plr);

	tl::expected<void, PacketError> HandleAccept(packet &pkt);
	tl::expected<void, PacketError> HandleConnect(packet &pkt);
	tl::expected<void, PacketError> HandleTurn(packet &pkt);
	tl::expected<void, PacketError> HandleDisconnect(packet &pkt);
	tl::expected<void, PacketError> HandleEchoRequest(packet &pkt);
	tl::expected<void, PacketError> HandleEchoReply(packet &pkt);
};

} // namespace net
} // namespace devilution
