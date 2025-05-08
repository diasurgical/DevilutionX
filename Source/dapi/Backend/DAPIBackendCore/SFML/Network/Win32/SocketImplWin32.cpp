#ifdef _WIN32
////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2018 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Network/Win32/SocketImpl.hpp>
#include <cstring>

decltype(recvfrom_original) *recvfrom;
decltype(setsockopt_original) *setsockopt;
decltype(socket_original) *socket;
decltype(select_original) *select;
decltype(WSAGetLastError_original) *WSAGetLastError;
decltype(ioctlsocket_original) *ioctlsocket;
decltype(htons_original) *htons;
decltype(htonl_original) *htonl;
decltype(WSAStartup_original) *WSAStartup;
decltype(closesocket_original) *closesocket;
decltype(WSACleanup_original) *WSACleanup;
decltype(ntohs_original) *ntohs;
decltype(getsockname_original) *getsockname;
decltype(listen_original) *listen;
decltype(bind_original) *bind;
decltype(accept_original) *accept;
decltype(ntohl_original) *ntohl;
decltype(recv_original) *recv;
decltype(connect_original) *connect;
decltype(send_original) *send;
decltype(getpeername_original) *getpeername;
decltype(freeaddrinfo_original) *freeaddrinfo;
decltype(inet_ntoa_original) *inet_ntoa;
decltype(inet_addr_original) *inet_addr;
decltype(getaddrinfo_original) *getaddrinfo;
decltype(sendto_original) *sendto;
decltype(__WSAFDIsSet_original) *__WSAFDIsSet;

namespace sf {
namespace priv {

////////////////////////////////////////////////////////////
// Windows needs some initialization and cleanup to get
// sockets working properly... so let's create a class that will
// do it automatically
////////////////////////////////////////////////////////////
struct WSock32 {
	WSock32()
	{
		ws2_32 = LoadLibrary(TEXT("ws2_32.dll"));

		(FARPROC &)recvfrom = GetProcAddress(ws2_32, "recvfrom");
		(FARPROC &)setsockopt = GetProcAddress(ws2_32, "setsockopt");
		(FARPROC &)socket = GetProcAddress(ws2_32, "socket");
		(FARPROC &)select = GetProcAddress(ws2_32, "select");
		(FARPROC &)WSAGetLastError = GetProcAddress(ws2_32, "WSAGetLastError");
		(FARPROC &)ioctlsocket = GetProcAddress(ws2_32, "ioctlsocket");
		(FARPROC &)htons = GetProcAddress(ws2_32, "htons");
		(FARPROC &)htonl = GetProcAddress(ws2_32, "htonl");
		(FARPROC &)WSAStartup = GetProcAddress(ws2_32, "WSAStartup");
		(FARPROC &)closesocket = GetProcAddress(ws2_32, "closesocket");
		(FARPROC &)WSACleanup = GetProcAddress(ws2_32, "WSACleanup");
		(FARPROC &)ntohs = GetProcAddress(ws2_32, "ntohs");
		(FARPROC &)getsockname = GetProcAddress(ws2_32, "getsockname");
		(FARPROC &)listen = GetProcAddress(ws2_32, "listen");
		(FARPROC &)bind = GetProcAddress(ws2_32, "bind");
		(FARPROC &)accept = GetProcAddress(ws2_32, "accept");
		(FARPROC &)ntohl = GetProcAddress(ws2_32, "ntohl");
		(FARPROC &)recv = GetProcAddress(ws2_32, "recv");
		(FARPROC &)connect = GetProcAddress(ws2_32, "connect");
		(FARPROC &)send = GetProcAddress(ws2_32, "send");
		(FARPROC &)getpeername = GetProcAddress(ws2_32, "getpeername");
		(FARPROC &)freeaddrinfo = GetProcAddress(ws2_32, "freeaddrinfo");
		(FARPROC &)inet_ntoa = GetProcAddress(ws2_32, "inet_ntoa");
		(FARPROC &)inet_addr = GetProcAddress(ws2_32, "inet_addr");
		(FARPROC &)getaddrinfo = GetProcAddress(ws2_32, "getaddrinfo");
		(FARPROC &)sendto = GetProcAddress(ws2_32, "sendto");
		(FARPROC &)__WSAFDIsSet = GetProcAddress(ws2_32, "__WSAFDIsSet");

		WSADATA init;
		WSAStartup(MAKEWORD(2, 2), &init);
	}

	~WSock32()
	{
		WSACleanup();

		FreeLibrary(ws2_32);
	}

	HMODULE ws2_32;
};

WSock32 wsock32;

////////////////////////////////////////////////////////////
sockaddr_in SocketImpl::createAddress(Uint32 address, unsigned short port)
{
	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = htonl(address);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	return addr;
}

////////////////////////////////////////////////////////////
SocketHandle SocketImpl::invalidSocket()
{
	return INVALID_SOCKET;
}

////////////////////////////////////////////////////////////
void SocketImpl::close(SocketHandle sock)
{
	closesocket(sock);
}

////////////////////////////////////////////////////////////
void SocketImpl::setBlocking(SocketHandle sock, bool block)
{
	u_long blocking = block ? 0 : 1;
	ioctlsocket(sock, FIONBIO, &blocking);
}

////////////////////////////////////////////////////////////
Socket::Status SocketImpl::getErrorStatus()
{
	switch (WSAGetLastError()) {
	case WSAEWOULDBLOCK: return Socket::NotReady;
	case WSAEALREADY: return Socket::NotReady;
	case WSAECONNABORTED: return Socket::Disconnected;
	case WSAECONNRESET: return Socket::Disconnected;
	case WSAETIMEDOUT: return Socket::Disconnected;
	case WSAENETRESET: return Socket::Disconnected;
	case WSAENOTCONN: return Socket::Disconnected;
	case WSAEISCONN: return Socket::Done; // when connecting a non-blocking socket
	default: return Socket::Error;
	}
}

} // namespace priv

} // namespace sf
#endif
