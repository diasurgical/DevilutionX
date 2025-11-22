#include "dvlnet/base.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include <expected.hpp>

#include "player.h"

namespace devilution {
namespace net {

void base::setup_gameinfo(buffer_t info)
{
	game_init_info = std::move(info);
}

void base::setup_password(std::string pw)
{
	pktfty = std::make_unique<packet_factory>(pw);
}

void base::clear_password()
{
	pktfty = std::make_unique<packet_factory>();
}

DvlNetLatencies base::get_latencies(uint8_t playerid)
{
	DvlNetLatencies latencies = abstract_net::get_latencies(playerid);
	const PlayerState &playerState = playerStateTable_[playerid];
	latencies.echoLatency = playerState.roundTripLatency;
	return latencies;
}

void base::RunEventHandler(_SNETEVENT &ev)
{
	auto f = registered_handlers[static_cast<event_type>(ev.eventid)];
	if (f != nullptr) {
		f(&ev);
	}
}

void base::DisconnectNet(plr_t plr)
{
}

tl::expected<void, PacketError> base::SendEchoRequest(plr_t player)
{
	if (plr_self == PLR_BROADCAST)
		return {};
	if (player == plr_self)
		return {};

	const timestamp_t now = SDL_GetTicks();
	tl::expected<std::unique_ptr<packet>, PacketError> pkt
	    = pktfty->make_packet<PT_ECHO_REQUEST>(plr_self, player, now);
	if (!pkt.has_value()) {
		return tl::make_unexpected(pkt.error());
	}
	return send(**pkt);
}

tl::expected<void, PacketError> base::HandleAccept(packet &pkt)
{
	if (plr_self != PLR_BROADCAST) {
		return {}; // already have player id
	}
	if (pkt.Cookie() == cookie_self) {
		tl::expected<plr_t, PacketError> newPlayerPkt = pkt.NewPlayer();
		if (!newPlayerPkt.has_value())
			return tl::make_unexpected(newPlayerPkt.error());
		plr_self = *std::move(newPlayerPkt);
		Connect(plr_self);
	}
	tl::expected<const buffer_t *, PacketError> infoPkt = pkt.Info();
	if (!infoPkt.has_value())
		return tl::make_unexpected(infoPkt.error());
	const buffer_t &info = **infoPkt;
	if (game_init_info != info) {
		if (info.size() != sizeof(GameData)) {
			ABORT();
		}
		// we joined and did not create
		game_init_info = info;
		_SNETEVENT ev;
		ev.eventid = EVENT_TYPE_PLAYER_CREATE_GAME;
		ev.playerid = plr_self;
		ev.data = const_cast<unsigned char *>(info.data());
		ev.databytes = info.size();
		RunEventHandler(ev);
	}
	return {};
}

tl::expected<void, PacketError> base::HandleConnect(packet &pkt)
{
	return pkt.NewPlayer().transform([this](plr_t &&newPlayer) {
		Connect(newPlayer);
	});
}

tl::expected<void, PacketError> base::HandleTurn(packet &pkt)
{
	const plr_t src = pkt.Source();
	PlayerState &playerState = playerStateTable_[src];
	std::deque<turn_t> &turnQueue = playerState.turnQueue;
	return pkt.Turn().transform([&](turn_t &&turn) {
		turnQueue.push_back(turn);
		MakeReady(turn.SequenceNumber);
	});
}

tl::expected<void, PacketError> base::HandleDisconnect(packet &pkt)
{
	tl::expected<plr_t, PacketError> newPlayer = pkt.NewPlayer();
	if (!newPlayer.has_value())
		return tl::make_unexpected(newPlayer.error());
	if (*newPlayer == plr_self)
		return tl::make_unexpected("We were dropped by the owner?");
	if (IsConnected(*newPlayer)) {
		tl::expected<leaveinfo_t, PacketError> leaveinfo = pkt.LeaveInfo();
		if (!leaveinfo.has_value())
			return tl::make_unexpected(leaveinfo.error());
		_SNETEVENT ev;
		ev.eventid = EVENT_TYPE_PLAYER_LEAVE_GAME;
		ev.playerid = *newPlayer;
		ev.data = reinterpret_cast<unsigned char *>(&*leaveinfo);
		ev.databytes = sizeof(leaveinfo_t);
		RunEventHandler(ev);
		DisconnectNet(*newPlayer);
		ClearMsg(*newPlayer);
		PlayerState &playerState = playerStateTable_[*newPlayer];
		playerState.isConnected = false;
		playerState.turnQueue.clear();
	}
	return {};
}

tl::expected<void, PacketError> base::HandleEchoRequest(packet &pkt)
{
	return pkt.Time()
	    .and_then([&](cookie_t &&pktTime) {
		    return pktfty->make_packet<PT_ECHO_REPLY>(plr_self, pkt.Source(), pktTime);
	    })
	    .and_then([&](std::unique_ptr<packet> &&pkt) {
		    return send(*pkt);
	    });
}

tl::expected<void, PacketError> base::HandleEchoReply(packet &pkt)
{
	const uint32_t now = SDL_GetTicks();
	plr_t src = pkt.Source();
	return pkt.Time().transform([&](cookie_t &&pktTime) {
		PlayerState &playerState = playerStateTable_[src];
		playerState.roundTripLatency = now - pktTime;
	});
}

void base::ClearMsg(plr_t plr)
{
	message_queue.erase(std::remove_if(message_queue.begin(),
	                        message_queue.end(),
	                        [&](message_t &msg) {
		                        return msg.sender == plr;
	                        }),
	    message_queue.end());
}

tl::expected<void, PacketError> base::Connect(plr_t player)
{
	PlayerState &playerState = playerStateTable_[player];
	const bool wasConnected = playerState.isConnected;
	playerState.isConnected = true;

	if (!wasConnected)
		return SendFirstTurnIfReady(player);
	return {};
}

bool base::IsConnected(plr_t player) const
{
	const PlayerState &playerState = playerStateTable_[player];
	return playerState.isConnected;
}

tl::expected<void, PacketError> base::RecvLocal(packet &pkt)
{
	if (pkt.Source() < MAX_PLRS) {
		if (tl::expected<void, PacketError> result = Connect(pkt.Source());
		    !result.has_value()) {
			return result;
		}
	}
	switch (pkt.Type()) {
	case PT_MESSAGE:
		return pkt.Message().transform([&](const buffer_t *message) {
			message_queue.emplace_back(pkt.Source(), *message);
		});
	case PT_TURN:
		return HandleTurn(pkt);
	case PT_JOIN_ACCEPT:
		return HandleAccept(pkt);
	case PT_CONNECT:
		return HandleConnect(pkt);
	case PT_DISCONNECT:
		return HandleDisconnect(pkt);
	case PT_ECHO_REQUEST:
		return HandleEchoRequest(pkt);
	case PT_ECHO_REPLY:
		return HandleEchoReply(pkt);
	default:
		return {};
		// otherwise drop
	}
}

bool base::SNetReceiveMessage(uint8_t *sender, void **data, size_t *size)
{
	uint32_t now = SDL_GetTicks();
	if (now == 0) now++;
	if (lastEchoTime == 0 || now - lastEchoTime > 5000) {
		for (plr_t i = 0; i < Players.size(); i++)
			SendEchoRequest(i);
		lastEchoTime = now;
	}
	poll();
	if (message_queue.empty())
		return false;
	message_last = message_queue.front();
	message_queue.pop_front();
	*sender = message_last.sender;
	*size = message_last.payload.size();
	*data = message_last.payload.data();
	return true;
}

bool base::SNetSendMessage(uint8_t playerId, void *data, size_t size)
{
	if (playerId != SNPLAYER_OTHERS && playerId >= MAX_PLRS)
		abort();
	auto *rawMessage = reinterpret_cast<unsigned char *>(data);
	const buffer_t message(rawMessage, rawMessage + size);
	if (playerId == plr_self)
		message_queue.emplace_back(plr_self, message);
	plr_t dest;
	if (playerId == SNPLAYER_OTHERS)
		dest = PLR_BROADCAST;
	else
		dest = playerId;
	if (dest != plr_self) {
		tl::expected<std::unique_ptr<packet>, PacketError> pkt
		    = pktfty->make_packet<PT_MESSAGE>(plr_self, dest, message);
		if (!pkt.has_value()) {
			LogError("make_packet: {}", pkt.error().what());
			return false;
		}
		tl::expected<void, PacketError> result = send(**pkt);
		if (!result.has_value()) {
			LogError("send: {}", result.error().what());
			return false;
		}
	}
	return true;
}

bool base::AllTurnsArrived()
{
	for (size_t i = 0; i < Players.size(); ++i) {
		const PlayerState &playerState = playerStateTable_[i];
		if (!playerState.isConnected)
			continue;

		const std::deque<turn_t> &turnQueue = playerState.turnQueue;
		if (turnQueue.empty()) {
			LogDebug("Turn missing from player {}", i);
			return false;
		}
	}

	return true;
}

bool base::SNetReceiveTurns(char **data, size_t *size, uint32_t *status)
{
	poll();

	for (size_t i = 0; i < Players.size(); ++i) {
		status[i] = 0;

		PlayerState &playerState = playerStateTable_[i];
		if (!playerState.isConnected)
			continue;

		status[i] |= PS_CONNECTED;

		std::deque<turn_t> &turnQueue = playerState.turnQueue;
		while (!turnQueue.empty()) {
			const turn_t &turn = turnQueue.front();
			const seq_t diff = turn.SequenceNumber - current_turn;
			if (diff <= 0x7F)
				break;
			turnQueue.pop_front();
		}
	}

	if (AllTurnsArrived()) {
		for (size_t i = 0; i < Players.size(); ++i) {
			PlayerState &playerState = playerStateTable_[i];
			if (!playerState.isConnected)
				continue;

			std::deque<turn_t> &turnQueue = playerState.turnQueue;
			if (turnQueue.empty())
				continue;

			const turn_t &turn = turnQueue.front();
			if (turn.SequenceNumber != current_turn)
				continue;

			playerState.lastTurnValue = turn.Value;
			turnQueue.pop_front();

			status[i] |= PS_ACTIVE;
			status[i] |= PS_TURN_ARRIVED;
			size[i] = sizeof(int32_t);
			data[i] = reinterpret_cast<char *>(&playerState.lastTurnValue);
		}

		current_turn++;

		return true;
	}

	for (size_t i = 0; i < Players.size(); ++i) {
		const PlayerState &playerState = playerStateTable_[i];
		if (!playerState.isConnected)
			continue;

		const std::deque<turn_t> &turnQueue = playerState.turnQueue;
		if (turnQueue.empty())
			continue;

		status[i] |= PS_ACTIVE;
	}

	return false;
}

bool base::SNetSendTurn(char *data, size_t size)
{
	if (size != sizeof(int32_t))
		ABORT();

	turn_t turn;
	turn.SequenceNumber = next_turn;
	std::memcpy(&turn.Value, data, size);
	next_turn++;

	PlayerState &playerState = playerStateTable_[plr_self];
	std::deque<turn_t> &turnQueue = playerState.turnQueue;
	turnQueue.push_back(turn);
	SendTurnIfReady(turn);
	return true;
}

tl::expected<void, PacketError> base::SendTurnIfReady(turn_t turn)
{
	if (awaitingSequenceNumber_)
		awaitingSequenceNumber_ = !IsGameHost();

	if (!awaitingSequenceNumber_) {
		tl::expected<std::unique_ptr<packet>, PacketError> pkt
		    = pktfty->make_packet<PT_TURN>(plr_self, PLR_BROADCAST, turn);
		if (!pkt.has_value()) {
			return tl::make_unexpected(pkt.error());
		}
		return send(**pkt);
	}
	return {};
}

tl::expected<void, PacketError> base::SendFirstTurnIfReady(plr_t player)
{
	if (awaitingSequenceNumber_)
		return {};

	const PlayerState &playerState = playerStateTable_[plr_self];
	const std::deque<turn_t> &turnQueue = playerState.turnQueue;
	if (turnQueue.empty())
		return {};

	for (const turn_t turn : turnQueue) {
		tl::expected<std::unique_ptr<packet>, PacketError> pkt
		    = pktfty->make_packet<PT_TURN>(plr_self, player, turn);
		if (!pkt.has_value()) {
			return tl::make_unexpected(pkt.error());
		}
		tl::expected<void, PacketError> result = send(**pkt);
		if (!result.has_value()) {
			return result;
		}
	}
	return {};
}

tl::expected<void, PacketError> base::MakeReady(seq_t sequenceNumber)
{
	if (!awaitingSequenceNumber_)
		return {};

	current_turn = sequenceNumber;
	next_turn = sequenceNumber;
	awaitingSequenceNumber_ = false;

	PlayerState &playerState = playerStateTable_[plr_self];
	std::deque<turn_t> &turnQueue = playerState.turnQueue;
	for (turn_t &turn : turnQueue) {
		turn.SequenceNumber = next_turn;
		next_turn++;
		if (tl::expected<void, PacketError> result = SendTurnIfReady(turn);
		    !result.has_value()) {
			return result;
		}
	}
	return {};
}

void base::SNetGetProviderCaps(struct _SNETCAPS *caps)
{
	caps->size = 0;                  // engine writes only ?!?
	caps->flags = 0;                 // unused
	caps->maxmessagesize = 512;      // capped to 512; underflow if < 24
	caps->maxqueuesize = 0;          // unused
	caps->maxplayers = MAX_PLRS;     // capped to 4
	caps->bytessec = 1000000;        // ?
	caps->latencyms = 0;             // unused
	caps->defaultturnssec = 10;      // ?
	caps->defaultturnsintransit = 2; // maximum acceptable number
	                                 // of turns in queue?
}

bool base::SNetUnregisterEventHandler(event_type evtype)
{
	registered_handlers.erase(evtype);
	return true;
}

bool base::SNetRegisterEventHandler(event_type evtype, SEVTHANDLER func)
{
	/*
  engine registers handler for:
  EVENT_TYPE_PLAYER_LEAVE_GAME
  EVENT_TYPE_PLAYER_CREATE_GAME (should be raised during SNetCreateGame
  for non-creating player)
  EVENT_TYPE_PLAYER_MESSAGE (for bnet? not implemented)
  (engine uses same function for all three)
*/
	registered_handlers[evtype] = func;
	return true;
}

bool base::SNetLeaveGame(int type)
{
	tl::expected<std::unique_ptr<packet>, PacketError> pkt
	    = pktfty->make_packet<PT_DISCONNECT>(
	        plr_self, PLR_BROADCAST, plr_self, static_cast<leaveinfo_t>(type));
	if (!pkt.has_value()) {
		LogError("make_packet: {}", pkt.error().what());
		return false;
	}
	tl::expected<void, PacketError> result = send(**pkt);
	if (!result.has_value()) {
		LogError("send: {}", result.error().what());
		return false;
	}
	plr_self = PLR_BROADCAST;
	return true;
}

bool base::SNetDropPlayer(int playerid, uint32_t flags)
{
	const plr_t plr = static_cast<plr_t>(playerid);
	tl::expected<std::unique_ptr<packet>, PacketError> pkt
	    = pktfty->make_packet<PT_DISCONNECT>(
	        plr_self,
	        PLR_BROADCAST,
	        plr,
	        static_cast<leaveinfo_t>(flags));
	if (!pkt.has_value()) {
		LogError("make_packet: {}", pkt.error().what());
		return false;
	}
	// Disconnect at the network layer first so we
	// don't send players their own disconnect packet
	DisconnectNet(plr);
	tl::expected<void, PacketError> sendResult = send(**pkt);
	if (!sendResult.has_value()) {
		LogError("send: {}", sendResult.error().what());
		return false;
	}
	tl::expected<void, PacketError> receiveResult = RecvLocal(**pkt);
	if (!receiveResult.has_value()) {
		LogError("SNetDropPlayer: {}", receiveResult.error().what());
		return false;
	}
	return true;
}

plr_t base::GetOwner()
{
	for (plr_t i = 0; i < Players.size(); ++i) {
		if (IsConnected(i)) {
			return i;
		}
	}
	return PLR_BROADCAST; // should be unreachable
}

bool base::SNetGetOwnerTurnsWaiting(uint32_t *turns)
{
	poll();

	const plr_t owner = GetOwner();
	const PlayerState &playerState = playerStateTable_[owner];
	const std::deque<turn_t> &turnQueue = playerState.turnQueue;
	*turns = static_cast<uint32_t>(turnQueue.size());

	return true;
}

bool base::SNetGetTurnsInTransit(uint32_t *turns)
{
	const PlayerState &playerState = playerStateTable_[plr_self];
	const std::deque<turn_t> &turnQueue = playerState.turnQueue;
	*turns = static_cast<uint32_t>(turnQueue.size());
	return true;
}

} // namespace net
} // namespace devilution
