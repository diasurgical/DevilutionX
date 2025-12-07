#pragma once

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

#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>
#include <asio/ts/io_context.hpp>
#include <asio/ts/net.hpp>
#include <asio_handle_exception.hpp>

#include "dvlnet/base.h"
#include "dvlnet/frame_queue.h"
#include "dvlnet/packet.h"
#include "dvlnet/tcp_server.h"

namespace devilution::net {

class tcp_client : public base {
public:
	int create(std::string_view addrstr) override;
	int join(std::string_view addrstr) override;

	tl::expected<void, PacketError> poll() override;
	tl::expected<void, PacketError> send(packet &pkt) override;
	void DisconnectNet(plr_t plr) override;

	bool SNetLeaveGame(net::leaveinfo_t type) override;

	~tcp_client() override;

	std::string make_default_gamename() override;

protected:
	bool IsGameHost() override;

private:
	frame_queue recv_queue;
	buffer_t recv_buffer = buffer_t(frame_queue::max_frame_size);

	asio::io_context ioc;
	asio::ip::tcp::resolver resolver = asio::ip::tcp::resolver(ioc);
	asio::ip::tcp::socket sock = asio::ip::tcp::socket(ioc);
	std::unique_ptr<tcp_server> local_server; // must be declared *after* ioc

	std::optional<PacketError> ioHandlerResult;

	void HandleReceive(const asio::error_code &error, size_t bytesRead);
	void StartReceive();
	void HandleSend(const asio::error_code &error, size_t bytesSent);
	void HandleTcpErrorCode();

	void RaiseIoHandlerError(const PacketError &error);
};

} // namespace devilution::net
