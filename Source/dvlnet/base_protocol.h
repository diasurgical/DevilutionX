#pragma once

#include <memory>
#include <set>
#include <string>
#include <string_view>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include <ankerl/unordered_dense.h>

#include "dvlnet/base.h"
#include "dvlnet/packet.h"
#include "player.h"
#include "utils/algorithm/container.hpp"
#include "utils/is_of.hpp"
#include "utils/log.hpp"

namespace devilution::net {

template <class P>
class base_protocol : public base {
public:
	int create(std::string_view addrstr) override;
	int join(std::string_view addrstr) override;
	tl::expected<void, PacketError> poll() override;
	tl::expected<void, PacketError> send(packet &pkt) override;
	void DisconnectNet(plr_t plr) override;

	bool SNetLeaveGame(int type) override;

	std::string make_default_gamename() override;
	bool send_info_request() override;
	void clear_gamelist() override;
	std::vector<GameInfo> get_gamelist() override;
	DvlNetLatencies get_latencies(uint8_t playerid) override;

	~base_protocol() override = default;

protected:
	bool IsGameHost() override;

private:
	P proto;
	typedef typename P::endpoint endpoint_t;

	struct Peer {
		endpoint_t endpoint;
		std::unique_ptr<std::deque<packet>> sendQueue;
	};

	endpoint_t firstpeer;
	std::string gamename;

	struct GameListValue {
		GameData data;
		std::vector<std::string> playerNames;
		endpoint_t peer;
	};
	ankerl::unordered_dense::map</*name*/ std::string, GameListValue> game_list;
	std::array<Peer, MAX_PLRS> peers;
	bool isGameHost_;

	plr_t get_master();
	tl::expected<void, PacketError> InitiateHandshake(plr_t player);
	tl::expected<void, PacketError> SendTo(plr_t player, packet &pkt);
	void DrainSendQueue(plr_t player);
	void recv();
	tl::expected<void, PacketError> handle_join_request(packet &pkt, endpoint_t sender);
	tl::expected<void, PacketError> recv_decrypted(packet &pkt, endpoint_t sender);
	tl::expected<void, PacketError> recv_ingame(packet &pkt, endpoint_t sender);
	bool is_recognized(endpoint_t sender);

	tl::expected<void, PacketError> wait_network();
	tl::expected<void, PacketError> wait_firstpeer();
	tl::expected<void, PacketError> wait_join();
};

template <class P>
plr_t base_protocol<P>::get_master()
{
	plr_t ret = plr_self;
	for (plr_t i = 0; i < Players.size(); ++i)
		if (peers[i].endpoint)
			ret = std::min(ret, i);
	return ret;
}

template <class P>
tl::expected<void, PacketError> base_protocol<P>::wait_network()
{
	// wait for ZeroTier for 5 seconds
	for (auto i = 0; i < 500; ++i) {
		tl::expected<bool, PacketError> status = proto.network_online();
		if (!status.has_value())
			return tl::make_unexpected(std::move(status).error());
		if (*status)
			return {};
		SDL_Delay(10);
	}
	return tl::make_unexpected("Timeout waiting for ZeroTier network initialization");
}

template <class P>
void base_protocol<P>::DisconnectNet(plr_t plr)
{
	Peer &peer = peers[plr];
	proto.disconnect(peer.endpoint);
	peer = {};
}

template <class P>
tl::expected<void, PacketError> base_protocol<P>::wait_firstpeer()
{
	firstpeer = {};
	// wait for peer for 5 seconds
	for (auto i = 0; i < 500; ++i) {
		auto it = game_list.find(gamename);
		if (it != game_list.end()) {
			firstpeer = it->second.peer;
			break;
		}
		send_info_request();
		recv();
		SDL_Delay(10);
	}
	if (!firstpeer)
		return tl::make_unexpected("Timeout waiting for response from game host");
	return {};
}

template <class P>
bool base_protocol<P>::send_info_request()
{
	tl::expected<bool, PacketError> status = proto.peers_ready();
	if (!status.has_value()) {
		LogError("peers_ready: {}", status.error().what());
		return false;
	}
	if (!*status)
		return false;
	tl::expected<std::unique_ptr<packet>, PacketError> pkt
	    = pktfty->make_packet<PT_INFO_REQUEST>(PLR_BROADCAST, PLR_MASTER);
	if (!pkt.has_value()) {
		LogError("make_packet: {}", pkt.error().what());
		return false;
	}
	proto.send_oob_mc((*pkt)->Data());
	return true;
}

template <class P>
tl::expected<void, PacketError> base_protocol<P>::wait_join()
{
	cookie_self = packet_out::GenerateCookie();
	tl::expected<std::unique_ptr<packet>, PacketError> pkt
	    = pktfty->make_packet<PT_JOIN_REQUEST>(PLR_BROADCAST, PLR_MASTER, cookie_self, game_init_info);
	if (!pkt.has_value()) {
		return tl::make_unexpected(pkt.error());
	}
	tl::expected<void, PacketError> result = proto.send(firstpeer, (*pkt)->Data());
	if (!result.has_value()) {
		return result;
	}
	for (auto i = 0; i < 500; ++i) {
		recv();
		if (plr_self != PLR_BROADCAST)
			return {}; // join successful
		SDL_Delay(10);
	}
	return tl::make_unexpected("Timeout waiting to join game");
}

template <class P>
int base_protocol<P>::create(std::string_view addrstr)
{
	gamename = addrstr;
	isGameHost_ = true;

	tl::expected<void, PacketError> isReady = wait_network();
	if (!isReady.has_value()) {
		const std::string_view message = isReady.error().what();
		SDL_SetError("%.*s", static_cast<int>(message.size()), message.data());
		return -1;
	}
	plr_self = 0;
	if (tl::expected<void, PacketError> result = Connect(plr_self);
	    !result.has_value()) {
		const std::string_view message = result.error().what();
		SDL_SetError("%.*s", static_cast<int>(message.size()), message.data());
		return -1;
	}
	return plr_self;
}

template <class P>
int base_protocol<P>::join(std::string_view addrstr)
{
	gamename = addrstr;
	isGameHost_ = false;

	tl::expected<void, PacketError> isReady = wait_network();
	if (!isReady.has_value()) {
		const std::string_view message = isReady.error().what();
		SDL_SetError("%.*s", static_cast<int>(message.size()), message.data());
		return -1;
	}
	tl::expected<void, PacketError> isPeerReady = wait_firstpeer();
	if (!isPeerReady.has_value()) {
		const std::string_view message = isPeerReady.error().what();
		SDL_SetError("%.*s", static_cast<int>(message.size()), message.data());
		return -1;
	}
	tl::expected<void, PacketError> isJoined = wait_join();
	if (!isJoined.has_value()) {
		const std::string_view message = isJoined.error().what();
		SDL_SetError("%.*s", static_cast<int>(message.size()), message.data());
		return -1;
	}
	return (plr_self == PLR_BROADCAST ? -1 : plr_self);
}

template <class P>
bool base_protocol<P>::IsGameHost()
{
	return isGameHost_;
}

template <class P>
tl::expected<void, PacketError> base_protocol<P>::poll()
{
	recv();
	return {};
}

template <class P>
tl::expected<void, PacketError> base_protocol<P>::InitiateHandshake(plr_t player)
{
	Peer &peer = peers[player];

	// The first packet sent will initiate the TCP connection over the ZeroTier network.
	// It will cause problems if both peers attempt to initiate the handshake simultaneously.
	// If the connection is already open, it should be safe to initiate from either end.
	// If not, only the player with the smaller player number should initiate the handshake.
	if (peer.endpoint && (plr_self < player || proto.is_peer_connected(peer.endpoint)))
		return SendEchoRequest(player);

	return {};
}

template <class P>
tl::expected<void, PacketError> base_protocol<P>::send(packet &pkt)
{
	plr_t destination = pkt.Destination();
	if (destination == PLR_BROADCAST) {
		for (plr_t player = 0; player < Players.size(); player++) {
			tl::expected<void, PacketError> result = SendTo(player, pkt);
			if (!result.has_value())
				LogError("Failed to send packet {} to player {}: {}", static_cast<uint8_t>(pkt.Type()), player, result.error().what());
		}
		return {};
	}
	if (destination >= MAX_PLRS)
		return tl::make_unexpected("Invalid player ID");
	if (destination == plr_self)
		return {};
	return SendTo(destination, pkt);
}

template <class P>
tl::expected<void, PacketError> base_protocol<P>::SendTo(plr_t player, packet &pkt)
{
	Peer &peer = peers[player];
	if (!peer.endpoint)
		return {};

	// The handshake uses echo packets so clients know
	// when they can safely drain their send queues
	if (peer.sendQueue && !IsAnyOf(pkt.Type(), PT_ECHO_REQUEST, PT_ECHO_REPLY)) {
		peer.sendQueue->push_back(pkt);
		return {};
	}

	return proto.send(peer.endpoint, pkt.Data());
}

template <class P>
void base_protocol<P>::recv()
{
	buffer_t pkt_buf;
	endpoint_t sender;
	while (proto.recv(sender, pkt_buf)) { // read until kernel buffer is empty?
		tl::expected<void, PacketError> result
		    = pktfty->make_packet(pkt_buf)
		          .and_then([&](std::unique_ptr<packet> &&pkt) {
			          return recv_decrypted(*pkt, sender);
		          });
		if (!result.has_value()) {
			// drop packet
			proto.disconnect(sender);
			Log("{}", result.error().what());
		}
	}
	while (proto.get_disconnected(sender)) {
		for (plr_t i = 0; i < Players.size(); ++i) {
			if (peers[i].endpoint == sender) {
				DisconnectNet(i);
				break;
			}
		}
	}
}

template <class P>
tl::expected<void, PacketError> base_protocol<P>::handle_join_request(packet &inPkt, endpoint_t sender)
{
	plr_t i;
	for (i = 0; i < Players.size(); ++i) {
		Peer &peer = peers[i];
		if (i != plr_self && !peer.endpoint) {
			peer.endpoint = sender;
			peer.sendQueue = std::make_unique<std::deque<packet>>();
			if (tl::expected<void, PacketError> result = Connect(i);
			    !result.has_value()) {
				return result;
			}
			break;
		}
	}
	if (i >= MAX_PLRS) {
		// already full
		return {};
	}

	auto senderinfo = sender.serialize();
	for (plr_t j = 0; j < Players.size(); ++j) {
		endpoint_t peer = peers[j].endpoint;
		if ((j != plr_self) && (j != i) && peer) {
			tl::expected<void, PacketError> result
			    = pktfty->make_packet<PT_CONNECT>(PLR_MASTER, PLR_BROADCAST, i, senderinfo)
			          .and_then([&](std::unique_ptr<packet> &&pkt) { return proto.send(peer, pkt->Data()); })
			          .and_then([&]() { return pktfty->make_packet<PT_CONNECT>(PLR_MASTER, PLR_BROADCAST, j, peer.serialize()); })
			          .and_then([&](std::unique_ptr<packet> &&pkt) { return proto.send(sender, pkt->Data()); });
			if (!result.has_value())
				return result;
		}
	}

	// PT_JOIN_ACCEPT must be sent after all PT_CONNECT packets so the new player does
	// not resume game logic until after having been notified of all existing players
	tl::expected<cookie_t, PacketError> cookie = inPkt.Cookie();
	if (!cookie.has_value())
		return tl::make_unexpected(cookie.error());
	tl::expected<std::unique_ptr<packet>, PacketError> pkt
	    = pktfty->make_packet<PT_JOIN_ACCEPT>(plr_self, PLR_BROADCAST, *cookie, i, game_init_info);
	if (!pkt.has_value())
		return tl::make_unexpected(pkt.error());
	tl::expected<void, PacketError> result = proto.send(sender, (*pkt)->Data());
	if (!result.has_value())
		return result;
	DrainSendQueue(i);
	return {};
}

template <class P>
tl::expected<void, PacketError> base_protocol<P>::recv_decrypted(packet &pkt, endpoint_t sender)
{
	if (pkt.Source() == PLR_BROADCAST && pkt.Destination() == PLR_MASTER && pkt.Type() == PT_INFO_REPLY) {
		size_t neededSize = sizeof(GameData) + (PlayerNameLength * MAX_PLRS);
		const tl::expected<const buffer_t *, PacketError> pktInfo = pkt.Info();
		if (!pktInfo.has_value())
			return tl::make_unexpected(pktInfo.error());
		const buffer_t &infoBuffer = **pktInfo;
		if (infoBuffer.size() < neededSize)
			return {};
		GameData gameData;
		std::memcpy(&gameData, infoBuffer.data(), sizeof(GameData));
		gameData.swapLE();
		if (gameData.size != sizeof(GameData))
			return {};
		std::vector<std::string> playerNames;
		for (size_t i = 0; i < Players.size(); i++) {
			std::string_view playerNameBuffer {
				reinterpret_cast<const char *>(infoBuffer.data() + sizeof(GameData) + (i * PlayerNameLength)),
				PlayerNameLength
			};
			if (const size_t nullPos = playerNameBuffer.find('\0'); nullPos != std::string_view::npos) {
				playerNameBuffer.remove_suffix(playerNameBuffer.size() - nullPos);
			}
			if (!playerNameBuffer.empty()) {
				playerNames.emplace_back(playerNameBuffer);
			}
		}
		std::string gameName;
		size_t gameNameSize = infoBuffer.size() - neededSize;
		gameName.resize(gameNameSize);
		std::memcpy(&gameName[0], infoBuffer.data() + neededSize, gameNameSize);
		game_list[gameName] = GameListValue { gameData, std::move(playerNames), sender };
		return {};
	}
	return recv_ingame(pkt, sender);
}

template <class P>
tl::expected<void, PacketError> base_protocol<P>::recv_ingame(packet &pkt, endpoint_t sender)
{
	if (pkt.Source() == PLR_BROADCAST && pkt.Destination() == PLR_MASTER) {
		if (pkt.Type() == PT_JOIN_REQUEST) {
			if (tl::expected<void, PacketError> result = handle_join_request(pkt, sender);
			    !result.has_value()) {
				return result;
			}
		} else if (pkt.Type() == PT_INFO_REQUEST) {
			if ((plr_self != PLR_BROADCAST) && (get_master() == plr_self)) {
				buffer_t buf;
				buf.resize(game_init_info.size() + (PlayerNameLength * MAX_PLRS) + gamename.size());
				std::memcpy(buf.data(), &game_init_info[0], game_init_info.size());
				for (size_t i = 0; i < Players.size(); i++) {
					if (Players[i].plractive) {
						std::memcpy(buf.data() + game_init_info.size() + (i * PlayerNameLength), &Players[i]._pName, PlayerNameLength);
					} else {
						std::memset(buf.data() + game_init_info.size() + (i * PlayerNameLength), '\0', PlayerNameLength);
					}
				}
				std::memcpy(buf.data() + game_init_info.size() + (PlayerNameLength * MAX_PLRS), &gamename[0], gamename.size());
				tl::expected<std::unique_ptr<packet>, PacketError> reply
				    = pktfty->make_packet<PT_INFO_REPLY>(PLR_BROADCAST, PLR_MASTER, buf);
				if (!reply.has_value()) {
					return tl::make_unexpected(reply.error());
				}
				proto.send_oob(sender, (*reply)->Data());
			}
		}
		return {};
	}
	if (pkt.Source() == PLR_MASTER && pkt.Type() == PT_CONNECT) {
		if (!is_recognized(sender)) {
			LogDebug("Invalid packet: PT_CONNECT received from unrecognized endpoint");
			return {};
		}

		// addrinfo packets
		tl::expected<plr_t, PacketError> newPlayer = pkt.NewPlayer();
		if (!newPlayer.has_value())
			return tl::make_unexpected(newPlayer.error());
		Peer &peer = peers[*newPlayer];
		tl::expected<const buffer_t *, PacketError> pktInfo = pkt.Info();
		if (!pktInfo.has_value())
			return tl::make_unexpected(pktInfo.error());
		if (tl::expected<void, PacketError> result = peer.endpoint.unserialize(**pktInfo);
		    !result.has_value()) {
			return result;
		}
		peer.sendQueue = std::make_unique<std::deque<packet>>();
		if (tl::expected<void, PacketError> result = Connect(*newPlayer);
		    !result.has_value()) {
			return result;
		}

		if (plr_self != PLR_BROADCAST)
			return InitiateHandshake(*newPlayer);
		return {};
	}
	if (pkt.Source() >= MAX_PLRS) {
		// normal packets
		LogDebug("Invalid packet: packet source ({}) >= MAX_PLRS", pkt.Source());
		return {};
	}
	if (sender == firstpeer && pkt.Type() == PT_JOIN_ACCEPT) {
		plr_t src = pkt.Source();
		peers[src].endpoint = sender;
		if (tl::expected<void, PacketError> result = Connect(src);
		    !result.has_value()) {
			return result;
		}
		firstpeer = {};
	} else if (sender != peers[pkt.Source()].endpoint) {
		LogDebug("Invalid packet: packet source ({}) received from unrecognized endpoint", pkt.Source());
		return {};
	}
	if (pkt.Destination() != plr_self && pkt.Destination() != PLR_BROADCAST)
		return {}; // packet not for us, drop

	bool wasBroadcast = plr_self == PLR_BROADCAST;
	if (tl::expected<void, PacketError> result = RecvLocal(pkt);
	    !result.has_value()) {
		return result;
	}

	if (plr_self != PLR_BROADCAST) {
		if (wasBroadcast) {
			// Send a handshake to everyone just after PT_JOIN_ACCEPT
			for (plr_t player = 0; player < Players.size(); player++) {
				if (tl::expected<void, PacketError> result = InitiateHandshake(player);
				    !result.has_value()) {
					return result;
				}
			}
		}

		DrainSendQueue(pkt.Source());
	}
	return {};
}

template <class P>
void base_protocol<P>::DrainSendQueue(plr_t player)
{
	Peer &srcPeer = peers[player];
	if (!srcPeer.sendQueue)
		return;

	std::deque<packet> &sendQueue = *srcPeer.sendQueue;
	while (!sendQueue.empty()) {
		packet &pkt = sendQueue.front();
		tl::expected<void, PacketError> result = proto.send(srcPeer.endpoint, pkt.Data());
		if (!result.has_value())
			LogError("DrainSendQueue failed to send packet: {}", result.error().what());
		sendQueue.pop_front();
	}

	srcPeer.sendQueue = nullptr;
}

template <class P>
bool base_protocol<P>::is_recognized(endpoint_t sender)
{
	if (!sender)
		return false;

	if (sender == firstpeer)
		return true;

	for (auto player = 0; player <= MAX_PLRS; player++) {
		if (sender == peers[player].endpoint)
			return true;
	}

	return false;
}

template <class P>
void base_protocol<P>::clear_gamelist()
{
	game_list.clear();
}

template <class P>
std::vector<GameInfo> base_protocol<P>::get_gamelist()
{
	recv();
	std::vector<GameInfo> ret;
	ret.reserve(game_list.size());
	for (const auto &[name, gameInfo] : game_list) {
		const auto &[gameData, players, endpoint] = gameInfo;
		std::optional<int> latency = proto.get_latency_to(endpoint);
		std::optional<bool> isRelayed = proto.is_peer_relayed(endpoint);
		ret.push_back(GameInfo { name, gameData, players, latency, isRelayed });
	}
	c_sort(ret, [](const GameInfo &a, const GameInfo &b) { return a.name < b.name; });
	return ret;
}

template <class P>
DvlNetLatencies base_protocol<P>::get_latencies(uint8_t playerid)
{
	DvlNetLatencies latencies = base::get_latencies(playerid);
	Peer &srcPeer = peers[playerid];
	latencies.providerLatency = proto.get_latency_to(srcPeer.endpoint);
	latencies.isRelayed = proto.is_peer_relayed(srcPeer.endpoint);
	return latencies;
}

template <class P>
bool base_protocol<P>::SNetLeaveGame(int type)
{
	auto ret = base::SNetLeaveGame(type);
	recv();
	return ret;
}

template <class P>
std::string base_protocol<P>::make_default_gamename()
{
	return proto.make_default_gamename();
}

} // namespace devilution::net
