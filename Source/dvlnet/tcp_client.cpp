#include "dvlnet/tcp_client.h"

#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <system_error>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include <asio/connect.hpp>
#include <expected.hpp>

#include "options.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"

namespace devilution::net {

int tcp_client::create(std::string_view addrstr)
{
	auto port = *GetOptions().Network.port;
	local_server = std::make_unique<tcp_server>(ioc, std::string(addrstr), port, *pktfty);
	return join(local_server->LocalhostSelf());
}

int tcp_client::join(std::string_view addrstr)
{
	constexpr int MsSleep = 10;
	constexpr int NoSleep = 250;

	const char *defaultPort = "6112";
	std::string_view host;
	std::string_view port = defaultPort;
	if (!addrstr.empty() && addrstr[0] == '[') {
		// Assume IPv6 address in square brackets, followed by port
		// Example: [::1]:6113
		size_t pos = addrstr.find(']', 1);
		pos = pos != std::string::npos ? pos + 1 : addrstr.length();
		host = addrstr.substr(0, pos);

		if (pos != addrstr.length()) {
			if (addrstr[pos] != ':') {
				SDL_SetError("Invalid hostname: expected colon after square brackets");
				return -1;
			}
			if (++pos != addrstr.length())
				port = addrstr.substr(pos);
		}
	} else {
		// Assume "hostname:port"
		const SplitByChar splithost(addrstr, ':');
		auto it = splithost.begin();
		if (it != splithost.end()) host = *it++;
		if (it != splithost.end()) port = *it++;

		// If there is more than one colon, assume it's just a plain IPv6 address
		if (it != splithost.end()) {
			host = addrstr;
			port = defaultPort;
		}
	}

	asio::error_code errorCode;
	const asio::ip::basic_resolver_results<asio::ip::tcp> range = resolver.resolve(host, port, errorCode);
	if (errorCode) {
		SDL_SetError("%s", errorCode.message().c_str());
		return -1;
	}

	asio::connect(sock, range, errorCode);
	if (errorCode) {
		SDL_SetError("%s", errorCode.message().c_str());
		return -1;
	}

	const asio::ip::tcp::no_delay option(true);
	sock.set_option(option, errorCode);
	if (errorCode)
		LogError("Client error setting socket option: {}", errorCode.message());

	StartReceive();
	{
		cookie_self = packet_out::GenerateCookie();
		tl::expected<std::unique_ptr<packet>, PacketError> pkt
		    = pktfty->make_packet<PT_JOIN_REQUEST>(
		        PLR_BROADCAST, PLR_MASTER, cookie_self, game_init_info);
		if (!pkt.has_value()) {
			const std::string_view message = pkt.error().what();
			SDL_SetError("make_packet: %.*s", static_cast<int>(message.size()), message.data());
			return -1;
		}
		tl::expected<void, PacketError> sendResult = send(**pkt);
		if (!sendResult.has_value()) {
			const std::string_view message = sendResult.error().what();
			SDL_SetError("send: %.*s", static_cast<int>(message.size()), message.data());
			return -1;
		}
		for (auto i = 0; i < NoSleep; ++i) {
			tl::expected<void, PacketError> pollResult = poll();
			if (!pollResult.has_value()) {
				const std::string_view message = pollResult.error().what();
				SDL_SetError("%.*s", static_cast<int>(message.size()), message.data());
				return -1;
			}
			if (plr_self != PLR_BROADCAST)
				break; // join successful
			SDL_Delay(MsSleep);
		}
	}
	if (plr_self == PLR_BROADCAST) {
		const std::string_view message = _("Unable to connect");
		SDL_SetError("%.*s", static_cast<int>(message.size()), message.data());
		return -1;
	}

	return plr_self;
}

bool tcp_client::IsGameHost()
{
	return local_server != nullptr;
}

tl::expected<void, PacketError> tcp_client::poll()
{
	while (ioc.poll_one() > 0) {
		if (IsGameHost()) {
			tl::expected<void, PacketError> serverResult = local_server->CheckIoHandlerError();
			if (!serverResult.has_value())
				return serverResult;
		}
		if (ioHandlerResult == std::nullopt)
			continue;
		tl::expected<void, PacketError> packetError = tl::make_unexpected(*ioHandlerResult);
		ioHandlerResult = std::nullopt;
		return packetError;
	}
	return {};
}

void tcp_client::HandleReceive(const asio::error_code &error, size_t bytesRead)
{
	if (error) {
		const PacketError packetError = IoHandlerError(error.message());
		RaiseIoHandlerError(packetError);
		return;
	}
	if (bytesRead == 0) {
		const PacketError packetError(_("error: read 0 bytes from server"));
		RaiseIoHandlerError(packetError);
		return;
	}
	recv_buffer.resize(bytesRead);
	recv_queue.Write(std::move(recv_buffer));
	recv_buffer.resize(frame_queue::max_frame_size);
	while (true) {
		tl::expected<bool, PacketError> ready = recv_queue.PacketReady();
		if (!ready.has_value()) {
			RaiseIoHandlerError(ready.error());
			return;
		}
		if (!*ready)
			break;
		tl::expected<void, PacketError> result
		    = recv_queue.ReadPacket()
		          .and_then([this](buffer_t &&pktData) { return pktfty->make_packet(pktData); })
		          .and_then([this](std::unique_ptr<packet> &&pkt) { return RecvLocal(*pkt); });
		if (!result.has_value()) {
			RaiseIoHandlerError(result.error());
			return;
		}
	}
	StartReceive();
}

void tcp_client::StartReceive()
{
	sock.async_receive(
	    asio::buffer(recv_buffer),
	    std::bind(&tcp_client::HandleReceive, this, std::placeholders::_1, std::placeholders::_2));
}

void tcp_client::HandleSend(const asio::error_code &error, size_t bytesSent)
{
	if (error)
		RaiseIoHandlerError(error.message());
}

tl::expected<void, PacketError> tcp_client::send(packet &pkt)
{
	tl::expected<buffer_t, PacketError> frame = frame_queue::MakeFrame(pkt.Data());
	if (!frame.has_value())
		return tl::make_unexpected(frame.error());
	std::unique_ptr<buffer_t> framePtr = std::make_unique<buffer_t>(*frame);
	const asio::mutable_buffer buf = asio::buffer(*framePtr);
	asio::async_write(sock, buf, [this, frame = std::move(framePtr)](const asio::error_code &error, size_t bytesSent) {
		HandleSend(error, bytesSent);
	});
	return {};
}

void tcp_client::DisconnectNet(plr_t plr)
{
	if (local_server != nullptr)
		local_server->DisconnectNet(plr);
}

bool tcp_client::SNetLeaveGame(int type)
{
	auto ret = base::SNetLeaveGame(type);
	poll();
	if (local_server != nullptr)
		local_server->Close();
	sock.close();
	return ret;
}

std::string tcp_client::make_default_gamename()
{
	return std::string(GetOptions().Network.szBindAddress);
}

void tcp_client::RaiseIoHandlerError(const PacketError &error)
{
	ioHandlerResult.emplace(error);
}

tcp_client::~tcp_client()
    = default;

} // namespace devilution::net
