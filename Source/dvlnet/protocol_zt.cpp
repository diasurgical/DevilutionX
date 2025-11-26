#include "dvlnet/protocol_zt.h"

#include <optional>
#include <random>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#else
#include "utils/sdl2_backports.h"
#endif
#endif

#include <lwip/igmp.h>
#include <lwip/mld6.h>
#include <lwip/nd6.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>

#include "dvlnet/zerotier_native.h"
#include "utils/log.hpp"

namespace devilution {
namespace net {

namespace {

bool GetMAC(const protocol_zt::endpoint &peer, uint64_t &mac)
{
	ip6_addr_t address = {};
	IP6_ADDR_PART(&address, 0, peer.addr[0], peer.addr[1], peer.addr[2], peer.addr[3]);
	IP6_ADDR_PART(&address, 1, peer.addr[4], peer.addr[5], peer.addr[6], peer.addr[7]);
	IP6_ADDR_PART(&address, 2, peer.addr[8], peer.addr[9], peer.addr[10], peer.addr[11]);
	IP6_ADDR_PART(&address, 3, peer.addr[12], peer.addr[13], peer.addr[14], peer.addr[15]);

	const u8_t *hwaddr;
	if (nd6_get_next_hop_addr_or_queue(netif_default, nullptr, &address, &hwaddr) != ERR_OK)
		return false;

	mac = hwaddr[0];
	mac = (mac << 8) | hwaddr[1];
	mac = (mac << 8) | hwaddr[2];
	mac = (mac << 8) | hwaddr[3];
	mac = (mac << 8) | hwaddr[4];
	mac = (mac << 8) | hwaddr[5];
	return true;
}

} // namespace

protocol_zt::protocol_zt()
{
	zerotier_network_start();
}

void protocol_zt::set_nonblock(int fd)
{
	static_assert(O_NONBLOCK == 1, "O_NONBLOCK == 1 not satisfied");
	auto mode = lwip_fcntl(fd, F_GETFL, 0);
	mode |= O_NONBLOCK;
	lwip_fcntl(fd, F_SETFL, mode);
}

void protocol_zt::set_nodelay(int fd)
{
	const int yes = 1;
	lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&yes, sizeof(yes));
}

void protocol_zt::set_reuseaddr(int fd)
{
	const int yes = 1;
	lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(yes));
}

tl::expected<bool, PacketError> protocol_zt::network_online()
{
	if (!zerotier_network_ready())
		return false;

	struct sockaddr_in6 in6 {
	};
	in6.sin6_port = htons(default_port);
	in6.sin6_family = AF_INET6;
	in6.sin6_addr = in6addr_any;

	if (fd_udp == -1) {
		fd_udp = lwip_socket(AF_INET6, SOCK_DGRAM, 0);
		set_reuseaddr(fd_udp);
		auto ret = lwip_bind(fd_udp, (struct sockaddr *)&in6, sizeof(in6));
		if (ret < 0) {
			std::string_view format = "Error binding to ZeroTier UDP socket: {}";
			PacketError error = ProtocolError(format, strerror(errno));
			return tl::make_unexpected(std::move(error));
		}
		set_nonblock(fd_udp);
	}
	if (fd_tcp == -1) {
		fd_tcp = lwip_socket(AF_INET6, SOCK_STREAM, 0);
		set_reuseaddr(fd_tcp);
		auto r1 = lwip_bind(fd_tcp, (struct sockaddr *)&in6, sizeof(in6));
		if (r1 < 0) {
			std::string_view format = "Error binding to ZeroTier TCP socket: {}";
			PacketError error = ProtocolError(format, strerror(errno));
			return tl::make_unexpected(std::move(error));
		}
		auto r2 = lwip_listen(fd_tcp, 10);
		if (r2 < 0) {
			std::string_view format = "Error listening on ZeroTier TCP socket: {}";
			PacketError error = ProtocolError(format, strerror(errno));
			return tl::make_unexpected(std::move(error));
		}
		set_nonblock(fd_tcp);
		set_nodelay(fd_tcp);
	}
	return true;
}

tl::expected<bool, PacketError> protocol_zt::peers_ready()
{
	return network_online()
	    .map([&](bool isOnline) { return isOnline && zerotier_peers_ready(); });
}

tl::expected<void, PacketError> protocol_zt::send(const endpoint &peer, const buffer_t &data)
{
	tl::expected<buffer_t, PacketError> frame = frame_queue::MakeFrame(data);
	if (!frame.has_value())
		return tl::make_unexpected(frame.error());
	peer_list[peer].send_queue.push_back(*frame);
	return {};
}

bool protocol_zt::send_oob(const endpoint &peer, const buffer_t &data) const
{
	struct sockaddr_in6 in6 {
	};
	in6.sin6_port = htons(default_port);
	in6.sin6_family = AF_INET6;
	std::copy(peer.addr.begin(), peer.addr.end(), in6.sin6_addr.s6_addr);
	lwip_sendto(fd_udp, data.data(), data.size(), 0, (const struct sockaddr *)&in6, sizeof(in6));
	return true;
}

bool protocol_zt::send_oob_mc(const buffer_t &data) const
{
	endpoint mc;
	std::copy(dvl_multicast_addr, dvl_multicast_addr + 16, mc.addr.begin());
	return send_oob(mc, data);
}

tl::expected<bool, PacketError> protocol_zt::send_queued_peer(const endpoint &peer)
{
	peer_state &state = peer_list[peer];
	if (state.fd == -1) {
		state.fd = lwip_socket(AF_INET6, SOCK_STREAM, 0);
		set_nodelay(state.fd);
		set_nonblock(state.fd);
		struct sockaddr_in6 in6 {
		};
		in6.sin6_port = htons(default_port);
		in6.sin6_family = AF_INET6;
		std::copy(peer.addr.begin(), peer.addr.end(), in6.sin6_addr.s6_addr);
		lwip_connect(state.fd, (const struct sockaddr *)&in6, sizeof(in6));
	}
	while (!state.send_queue.empty()) {
		auto len = state.send_queue.front().size();
		auto r = lwip_send(state.fd, state.send_queue.front().data(), len, 0);
		if (r < 0) {
			// handle error
			return false;
		}
		if (decltype(len)(r) < len) {
			// partial send
			auto it = state.send_queue.front().begin();
			state.send_queue.front().erase(it, it + r);
			return true;
		}
		if (decltype(len)(r) == len) {
			state.send_queue.pop_front();
		} else {
			std::string_view format = "Impossible number of bytes sent: {} available, {} sent";
			PacketError error = ProtocolError(format, len, decltype(len)(r));
			return tl::make_unexpected(std::move(error));
		}
	}
	return true;
}

bool protocol_zt::recv_peer(const endpoint &peer)
{
	unsigned char buf[PKTBUF_LEN];
	peer_state &state = peer_list[peer];
	while (true) {
		auto len = lwip_recv(state.fd, buf, sizeof(buf), 0);
		if (len >= 0) {
			state.recv_queue.Write(buffer_t(buf, buf + len));
		} else {
			return errno == EAGAIN || errno == EWOULDBLOCK;
		}
	}
}

bool protocol_zt::send_queued_all()
{
	for (const auto &[endpoint, _] : peer_list) {
		tl::expected<bool, PacketError> result = send_queued_peer(endpoint);
		if (!result.has_value()) {
			LogError("send_queued_peer: {}", result.error().what());
			continue;
		}
		if (!*result) {
			// handle error?
		}
	}
	return true;
}

bool protocol_zt::recv_from_peers()
{
	for (const auto &[endpoint, state] : peer_list) {
		if (state.fd != -1) {
			if (!recv_peer(endpoint)) {
				disconnect_queue.push_back(endpoint);
			}
		}
	}
	return true;
}

bool protocol_zt::recv_from_udp()
{
	unsigned char buf[PKTBUF_LEN];
	struct sockaddr_in6 in6 {
	};
	socklen_t addrlen = sizeof(in6);
	auto len = lwip_recvfrom(fd_udp, buf, sizeof(buf), 0, (struct sockaddr *)&in6, &addrlen);
	if (len < 0)
		return false;
	buffer_t data(buf, buf + len);
	endpoint ep;
	std::copy(in6.sin6_addr.s6_addr, in6.sin6_addr.s6_addr + 16, ep.addr.begin());
	oob_recv_queue.emplace_back(ep, std::move(data));
	return true;
}

bool protocol_zt::accept_all()
{
	struct sockaddr_in6 in6 {
	};
	socklen_t addrlen = sizeof(in6);
	while (true) {
		auto newfd = lwip_accept(fd_tcp, (struct sockaddr *)&in6, &addrlen);
		if (newfd < 0)
			break;
		endpoint ep;
		std::copy(in6.sin6_addr.s6_addr, in6.sin6_addr.s6_addr + 16, ep.addr.begin());
		peer_state &state = peer_list[ep];
		if (state.fd != -1) {
			Log("protocol_zt::accept_all: WARNING: overwriting connection");
			SDL_SetError("protocol_zt::accept_all: WARNING: overwriting connection");
			lwip_close(state.fd);
		}
		set_nonblock(newfd);
		set_nodelay(newfd);
		state.fd = newfd;
	}
	return true;
}

bool protocol_zt::recv(endpoint &peer, buffer_t &data)
{
	accept_all();
	send_queued_all();
	recv_from_peers();
	recv_from_udp();

	if (!oob_recv_queue.empty()) {
		peer = oob_recv_queue.front().first;
		data = oob_recv_queue.front().second;
		oob_recv_queue.pop_front();
		return true;
	}

	for (auto &p : peer_list) {
		tl::expected<bool, PacketError> ready = p.second.recv_queue.PacketReady();
		if (!ready.has_value()) {
			LogError("PacketReady: {}", ready.error().what());
			continue;
		}
		if (!*ready)
			continue;
		tl::expected<buffer_t, PacketError> packet = p.second.recv_queue.ReadPacket();
		if (!packet.has_value()) {
			LogError("Failed reading packet data from peer: {}", packet.error().what());
			continue;
		}
		peer = p.first;
		data = *packet;
		return true;
	}
	return false;
}

bool protocol_zt::get_disconnected(endpoint &peer)
{
	if (!disconnect_queue.empty()) {
		peer = disconnect_queue.front();
		disconnect_queue.pop_front();
		return true;
	}
	return false;
}

void protocol_zt::disconnect(const endpoint &peer)
{
	const auto it = peer_list.find(peer);
	if (it != peer_list.end()) {
		if (it->second.fd != -1) {
			if (lwip_close(it->second.fd) < 0) {
				Log("lwip_close: {}", strerror(errno));
				SDL_SetError("lwip_close: %s", strerror(errno));
			}
		}
		peer_list.erase(it);
	}
}

void protocol_zt::close_all()
{
	if (fd_tcp != -1) {
		lwip_close(fd_tcp);
		fd_tcp = -1;
	}
	if (fd_udp != -1) {
		lwip_close(fd_udp);
		fd_udp = -1;
	}
	for (auto &[_, state] : peer_list) {
		if (state.fd != -1)
			lwip_close(state.fd);
	}
	peer_list.clear();
}

protocol_zt::~protocol_zt()
{
	close_all();
}

void protocol_zt::endpoint::from_string(const std::string &str)
{
	ip_addr_t a;
	if (ipaddr_aton(str.c_str(), &a) == 0)
		return;
	if (!IP_IS_V6_VAL(a))
		return;
	const auto *r = reinterpret_cast<const unsigned char *>(a.u_addr.ip6.addr);
	std::copy(r, r + 16, addr.begin());
}

uint64_t protocol_zt::current_ms()
{
	return 0;
}

bool protocol_zt::is_peer_connected(endpoint &peer)
{
	const auto it = peer_list.find(peer);
	return it != peer_list.end() && it->second.fd != -1;
}

std::optional<bool> protocol_zt::is_peer_relayed(const endpoint &peer) const
{
	uint64_t mac;
	if (!GetMAC(peer, mac))
		return std::nullopt;
	return zerotier_is_relayed(mac);
}

std::optional<int> protocol_zt::get_latency_to(const endpoint &peer) const
{
	uint64_t mac;
	if (!GetMAC(peer, mac))
		return std::nullopt;
	return zerotier_latency(mac);
}

std::string protocol_zt::make_default_gamename()
{
	std::string ret;
	std::string allowedChars = "abcdefghkopqrstuvwxyz";
	std::random_device rd;
	std::uniform_int_distribution<size_t> dist(0, allowedChars.size() - 1);
	for (size_t i = 0; i < 5; ++i) {
		ret += allowedChars.at(dist(rd));
	}
	return ret;
}

} // namespace net
} // namespace devilution
