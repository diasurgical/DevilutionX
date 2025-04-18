#pragma once

#include <array>
#include <memory>
#include <string>

// This header must be included before any 3DS code
// because 3DS SDK defines a macro with the same name
// as an fmt template parameter in some versions of fmt.
// See https://github.com/fmtlib/fmt/issues/3632
//
// 3DS uses some custom ASIO code that transitively includes
// the 3DS SDK.
#include <fmt/core.h>

#include <expected.hpp>

#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <asio/ts/io_context.hpp>
#include <asio/ts/net.hpp>
#include <asio_handle_exception.hpp>

#include "dvlnet/abstract_net.h"
#include "dvlnet/frame_queue.h"
#include "dvlnet/packet.h"
#include "multi.h"

namespace devilution::net {

inline PacketError ServerError()
{
	return PacketError("Invalid player ID");
}

class tcp_server {
public:
	tcp_server(asio::io_context &ioc, const std::string &bindaddr,
	    unsigned short port, packet_factory &pktfty);
	std::string LocalhostSelf();
	tl::expected<void, PacketError> CheckIoHandlerError();
	void DisconnectNet(plr_t plr);
	void Close();
	virtual ~tcp_server();

private:
	static constexpr int timeout_connect = 30;
	static constexpr int timeout_active = 60;

	struct client_connection {
		frame_queue recv_queue;
		buffer_t recv_buffer = buffer_t(frame_queue::max_frame_size);
		plr_t plr = PLR_BROADCAST;
		asio::ip::tcp::socket socket;
		asio::steady_timer timer;
		int timeout;
		client_connection(asio::io_context &ioc)
		    : socket(ioc)
		    , timer(ioc)
		{
		}
	};

	typedef std::shared_ptr<client_connection> scc;

	asio::io_context &ioc;
	packet_factory &pktfty;
	std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
	std::array<scc, MaxPlayers> connections;
	buffer_t game_init_info;

	std::optional<PacketError> ioHandlerResult;

	scc MakeConnection();
	plr_t NextFree();
	bool Empty();
	void StartAccept();
	void HandleAccept(const scc &con, const asio::error_code &ec);
	void StartReceive(const scc &con);
	void HandleReceive(const scc &con, const asio::error_code &ec, size_t bytesRead);
	tl::expected<void, PacketError> HandleReceiveNewPlayer(const scc &con, packet &pkt);
	tl::expected<void, PacketError> HandleReceivePacket(packet &pkt);
	tl::expected<void, PacketError> SendPacket(packet &pkt);
	tl::expected<void, PacketError> StartSend(const scc &con, packet &pkt);
	void HandleSend(const scc &con, const asio::error_code &ec, size_t bytesSent);
	void StartTimeout(const scc &con);
	void HandleTimeout(const scc &con, const asio::error_code &ec);
	void DropConnection(const scc &con);
	void RaiseIoHandlerError(const PacketError &error);
};

} // namespace devilution::net
