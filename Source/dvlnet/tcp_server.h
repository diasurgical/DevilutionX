#pragma once

#include <array>
#include <memory>
#include <string>

#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <asio/ts/io_context.hpp>
#include <asio/ts/net.hpp>

#include "dvlnet/abstract_net.h"
#include "dvlnet/frame_queue.h"
#include "dvlnet/packet.h"
#include "multi.h"

namespace devilution {
namespace net {

class server_exception : public dvlnet_exception {
public:
	const char *what() const throw() override
	{
		return "Invalid player ID";
	}
};

class tcp_server {
public:
	tcp_server(asio::io_context &ioc, const std::string &bindaddr,
	    unsigned short port, packet_factory &pktfty);
	std::string LocalhostSelf();
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
	std::array<scc, MAX_PLRS> connections;
	buffer_t game_init_info;

	scc MakeConnection();
	plr_t NextFree();
	bool Empty();
	void StartAccept();
	void HandleAccept(const scc &con, const asio::error_code &ec);
	void StartReceive(const scc &con);
	void HandleReceive(const scc &con, const asio::error_code &ec, size_t bytesRead);
	void HandleReceiveNewPlayer(const scc &con, packet &pkt);
	void HandleReceivePacket(packet &pkt);
	void SendPacket(packet &pkt);
	void StartSend(const scc &con, packet &pkt);
	void HandleSend(const scc &con, const asio::error_code &ec, size_t bytesSent);
	void StartTimeout(const scc &con);
	void HandleTimeout(const scc &con, const asio::error_code &ec);
	void DropConnection(const scc &con);
};

} // namespace net
} // namespace devilution
