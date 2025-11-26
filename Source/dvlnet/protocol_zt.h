#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <exception>
#include <optional>
#include <string>

#include <ankerl/unordered_dense.h>

#include "dvlnet/frame_queue.h"
#include "dvlnet/packet.h"
#include "utils/log.hpp"

namespace devilution {
namespace net {

template <typename... Args>
PacketError ProtocolError(std::string_view fmt, Args &&...args)
{
	auto str = detail::format(fmt, std::forward<Args>(args)...);
	return PacketError(str);
}

class protocol_zt {
public:
	class endpoint {
	public:
		std::array<unsigned char, 16> addr = {};

		explicit operator bool() const
		{
			auto empty = std::array<unsigned char, 16> {};
			return (addr != empty);
		}

		bool operator==(const endpoint &rhs) const
		{
			return addr == rhs.addr;
		}

		bool operator!=(const endpoint &rhs) const
		{
			return !(*this == rhs);
		}

		bool operator<(const endpoint &rhs) const
		{
			return addr < rhs.addr;
		}

		buffer_t serialize() const
		{
			return buffer_t(addr.begin(), addr.end());
		}

		tl::expected<void, PacketError> unserialize(const buffer_t &buf)
		{
			if (buf.size() != 16) {
				std::string_view format = "Endpoint deserialization expected 16 bytes, got {}";
				PacketError error = ProtocolError(format, buf.size());
				return tl::make_unexpected(std::move(error));
			}
			std::copy(buf.begin(), buf.end(), addr.begin());
			return {};
		}

		void from_string(const std::string &str);
	};
	struct EndpointHash {
		using is_avalanching = void;

		[[nodiscard]] uint64_t operator()(const endpoint &e) const noexcept
		{
			return ankerl::unordered_dense::hash<std::string_view> {}(
			    std::string_view { reinterpret_cast<const char *>(e.addr.data()), e.addr.size() });
		}
	};

	protocol_zt();
	~protocol_zt();
	void disconnect(const endpoint &peer);
	tl::expected<void, PacketError> send(const endpoint &peer, const buffer_t &data);
	bool send_oob(const endpoint &peer, const buffer_t &data) const;
	bool send_oob_mc(const buffer_t &data) const;
	bool recv(endpoint &peer, buffer_t &data);
	bool get_disconnected(endpoint &peer);
	tl::expected<bool, PacketError> network_online();
	tl::expected<bool, PacketError> peers_ready();
	bool is_peer_connected(endpoint &peer);
	std::optional<bool> is_peer_relayed(const endpoint &peer) const;
	std::optional<int> get_latency_to(const endpoint &peer) const;
	static std::string make_default_gamename();

private:
	static constexpr uint32_t PKTBUF_LEN = 65536;
	static constexpr uint16_t default_port = 6112;

	struct peer_state {
		int fd = -1;
		std::deque<buffer_t> send_queue;
		frame_queue recv_queue;
	};

	std::deque<std::pair<endpoint, buffer_t>> oob_recv_queue;
	std::deque<endpoint> disconnect_queue;

	ankerl::unordered_dense::map<endpoint, peer_state, EndpointHash> peer_list;
	int fd_tcp = -1;
	int fd_udp = -1;

	static uint64_t current_ms();
	void close_all();

	static void set_nonblock(int fd);
	static void set_nodelay(int fd);
	static void set_reuseaddr(int fd);

	tl::expected<bool, PacketError> send_queued_peer(const endpoint &peer);
	bool recv_peer(const endpoint &peer);
	bool send_queued_all();
	bool recv_from_peers();
	bool recv_from_udp();
	bool accept_all();
};

} // namespace net
} // namespace devilution
