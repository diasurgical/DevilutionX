#include "dvlnet/tcp_server.h"

#include <chrono>
#include <functional>
#include <memory>
#include <utility>

#include <expected.hpp>

#include "dvlnet/base.h"
#include "player.h"
#include "utils/log.hpp"

namespace devilution::net {

tcp_server::tcp_server(asio::io_context &ioc, const std::string &bindaddr,
    unsigned short port, packet_factory &pktfty)
    : ioc(ioc)
    , pktfty(pktfty)
{
	auto addr = asio::ip::address::from_string(bindaddr);
	auto ep = asio::ip::tcp::endpoint(addr, port);
	acceptor = std::make_unique<asio::ip::tcp::acceptor>(ioc, ep, true);
	StartAccept();
}

std::string tcp_server::LocalhostSelf()
{
	auto addr = acceptor->local_endpoint().address();
	if (addr.is_unspecified()) {
		if (addr.is_v4()) {
			return asio::ip::address_v4::loopback().to_string();
		}
		if (addr.is_v6()) {
			return asio::ip::address_v6::loopback().to_string();
		}
		ABORT();
	}
	return addr.to_string();
}

tcp_server::scc tcp_server::MakeConnection()
{
	return std::make_shared<client_connection>(ioc);
}

plr_t tcp_server::NextFree()
{
	for (plr_t i = 0; i < Players.size(); ++i)
		if (!connections[i])
			return i;
	return PLR_BROADCAST;
}

bool tcp_server::Empty()
{
	for (plr_t i = 0; i < Players.size(); ++i)
		if (connections[i])
			return false;
	return true;
}

void tcp_server::StartReceive(const scc &con)
{
	con->socket.async_receive(
	    asio::buffer(con->recv_buffer),
	    std::bind(&tcp_server::HandleReceive, this, con, std::placeholders::_1, std::placeholders::_2));
}

void tcp_server::HandleReceive(const scc &con, const asio::error_code &ec,
    size_t bytesRead)
{
	if (ec || bytesRead == 0) {
		DropConnection(con);
		return;
	}
	con->recv_buffer.resize(bytesRead);
	con->recv_queue.Write(std::move(con->recv_buffer));
	con->recv_buffer.resize(frame_queue::max_frame_size);
	while (true) {
		tl::expected<bool, PacketError> ready = con->recv_queue.PacketReady();
		if (!ready.has_value()) {
			Log("PacketReady: {}", ready.error().what());
			DropConnection(con);
			return;
		}
		if (!*ready)
			break;
		tl::expected<buffer_t, PacketError> pktData = con->recv_queue.ReadPacket();
		if (!pktData.has_value()) {
			Log("ReadPacket: {}", pktData.error().what());
			DropConnection(con);
			return;
		}
		tl::expected<std::unique_ptr<packet>, PacketError> pkt = pktfty.make_packet(*pktData);
		if (!pkt.has_value()) {
			Log("make_packet: {}", pkt.error().what());
			DropConnection(con);
			return;
		}
		if (con->plr == PLR_BROADCAST) {
			tl::expected<void, PacketError> result = HandleReceiveNewPlayer(con, **pkt);
			if (!result.has_value()) {
				Log("HandleReceiveNewPlayer: {}", result.error().what());
				DropConnection(con);
				return;
			}
		} else {
			con->timeout = timeout_active;
			tl::expected<void, PacketError> result = HandleReceivePacket(**pkt);
			if (!result.has_value()) {
				Log("Network error: {}", result.error().what());
				DropConnection(con);
				return;
			}
		}
	}
	StartReceive(con);
}

tl::expected<void, PacketError> tcp_server::HandleReceiveNewPlayer(const scc &con, packet &inPkt)
{
	auto newplr = NextFree();
	if (newplr == PLR_BROADCAST)
		return tl::make_unexpected(ServerError());

	if (Empty()) {
		tl::expected<const buffer_t *, PacketError> pktInfo = inPkt.Info();
		if (!pktInfo.has_value())
			return tl::make_unexpected(pktInfo.error());
		game_init_info = **pktInfo;
	}

	for (plr_t player = 0; player < Players.size(); player++) {
		if (connections[player]) {
			tl::expected<void, PacketError> result
			    = pktfty.make_packet<PT_CONNECT>(PLR_MASTER, PLR_BROADCAST, newplr)
			          .and_then([&](std::unique_ptr<packet> &&pkt) { return StartSend(connections[player], *pkt); })
			          .and_then([&]() { return pktfty.make_packet<PT_CONNECT>(PLR_MASTER, PLR_BROADCAST, player); })
			          .and_then([&](std::unique_ptr<packet> &&pkt) { return StartSend(con, *pkt); });
			if (!result.has_value())
				return result;
		}
	}

	tl::expected<void, PacketError> result
	    = inPkt.Cookie()
	          .and_then([&](cookie_t &&cookie) { return pktfty.make_packet<PT_JOIN_ACCEPT>(PLR_MASTER, PLR_BROADCAST, cookie, newplr, game_init_info); })
	          .and_then([&](std::unique_ptr<packet> &&pkt) { return StartSend(con, *pkt); });
	if (!result.has_value())
		return result;
	con->plr = newplr;
	connections[newplr] = con;
	con->timeout = timeout_active;
	return {};
}

tl::expected<void, PacketError> tcp_server::HandleReceivePacket(packet &pkt)
{
	return SendPacket(pkt);
}

tl::expected<void, PacketError> tcp_server::SendPacket(packet &pkt)
{
	if (pkt.Destination() == PLR_BROADCAST) {
		for (size_t i = 0; i < Players.size(); ++i) {
			if (i == pkt.Source() || !connections[i])
				continue;
			tl::expected<void, PacketError> result = StartSend(connections[i], pkt);
			if (!result.has_value())
				LogError("Failed to send packet {} to player {}: {}", static_cast<uint8_t>(pkt.Type()), i, result.error().what());
		}
		return {};
	}
	if (pkt.Destination() >= MAX_PLRS)
		return tl::make_unexpected(ServerError());
	if (pkt.Destination() == pkt.Source() || !connections[pkt.Destination()])
		return {};
	return StartSend(connections[pkt.Destination()], pkt);
}

tl::expected<void, PacketError> tcp_server::StartSend(const scc &con, packet &pkt)
{
	tl::expected<buffer_t, PacketError> frame = frame_queue::MakeFrame(pkt.Data());
	if (!frame.has_value())
		return tl::make_unexpected(frame.error());
	std::unique_ptr<buffer_t> framePtr = std::make_unique<buffer_t>(*frame);
	const asio::mutable_buffer buf = asio::buffer(*framePtr);
	asio::async_write(con->socket, buf,
	    [this, con, frame = std::move(framePtr)](const asio::error_code &ec, size_t bytesSent) {
		    HandleSend(con, ec, bytesSent);
	    });
	return {};
}

void tcp_server::HandleSend(const scc &con, const asio::error_code &ec,
    size_t bytesSent)
{
	if (ec) {
		Log("Network error: {}", ec.message());
		DropConnection(con);
	}
}

void tcp_server::StartAccept()
{
	auto nextcon = MakeConnection();
	acceptor->async_accept(
	    nextcon->socket,
	    std::bind(&tcp_server::HandleAccept, this, nextcon, std::placeholders::_1));
}

void tcp_server::HandleAccept(const scc &con, const asio::error_code &ec)
{
	if (ec) {
		const PacketError packetError = IoHandlerError(ec.message());
		RaiseIoHandlerError(packetError);
		return;
	}
	if (NextFree() == PLR_BROADCAST) {
		DropConnection(con);
	} else {
		asio::error_code errorCode;
		const asio::ip::tcp::no_delay option(true);
		con->socket.set_option(option, errorCode);
		if (errorCode)
			LogError("Server error setting socket option: {}", errorCode.message());
		con->timeout = timeout_connect;
		StartReceive(con);
		StartTimeout(con);
	}
	StartAccept();
}

void tcp_server::StartTimeout(const scc &con)
{
	con->timer.expires_after(std::chrono::seconds(1));
	con->timer.async_wait(
	    std::bind(&tcp_server::HandleTimeout, this, con, std::placeholders::_1));
}

void tcp_server::HandleTimeout(const scc &con, const asio::error_code &ec)
{
	if (ec) {
		DropConnection(con);
		return;
	}
	if (con->timeout > 0)
		con->timeout -= 1;
	if (con->timeout <= 0) {
		con->timeout = 0;
		DropConnection(con);
		return;
	}
	StartTimeout(con);
}

void tcp_server::DropConnection(const scc &con)
{
	const plr_t plr = con->plr;
	con->timer.cancel();
	con->socket.close();
	if (plr == PLR_BROADCAST) {
		return;
	}
	connections[plr] = nullptr;

	tl::expected<std::unique_ptr<packet>, PacketError> pkt
	    = pktfty.make_packet<PT_DISCONNECT>(PLR_MASTER, PLR_BROADCAST,
	        plr, static_cast<leaveinfo_t>(LEAVE_DROP));
	if (pkt.has_value()) {
		SendPacket(**pkt);
	} else {
		LogError("make_packet<PT_DISCONNECT>: {}", pkt.error().what());
	}
}

void tcp_server::RaiseIoHandlerError(const PacketError &error)
{
	ioHandlerResult.emplace(error);
}

tl::expected<void, PacketError> tcp_server::CheckIoHandlerError()
{
	if (ioHandlerResult == std::nullopt)
		return {};
	tl::expected<void, PacketError> packetError = tl::make_unexpected(*ioHandlerResult);
	ioHandlerResult = std::nullopt;
	return packetError;
}

void tcp_server::DisconnectNet(plr_t plr)
{
	scc &con = connections[plr];
	if (con == nullptr)
		return;
	con->timer.cancel();
	con->socket.close();
	con = nullptr;
}

void tcp_server::Close()
{
	acceptor->close();
}

tcp_server::~tcp_server()
    = default;

} // namespace devilution::net
