#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOGDICAPMASKS
#define NOGDICAPMASKS
#endif

#ifndef NOVIRTUALKEYCODES
#define NOVIRTUALKEYCODES
#endif

#ifndef NOWINMESSAGES
#define NOWINMESSAGES
#endif

#ifndef NOWINSTYLES
#define NOWINSTYLES
#endif

#ifndef NOSYSMETRICS
#define NOSYSMETRICS
#endif

#ifndef NOMENUS
#define NOMENUS
#endif

#ifndef NOICONS
#define NOICONS
#endif

#ifndef NOKEYSTATES
#define NOKEYSTATES
#endif

#ifndef NOSYSCOMMANDS
#define NOSYSCOMMANDS
#endif

#ifndef NORASTEROPS
#define NORASTEROPS
#endif

#ifndef NOSHOWWINDOW
#define NOSHOWWINDOW
#endif

#ifndef NOATOM
#define NOATOM
#endif

#ifndef NOCLIPBOARD
#define NOCLIPBOARD
#endif

#ifndef NOCOLOR
#define NOCOLOR
#endif

#ifndef NOCTLMGR
#define NOCTLMGR
#endif

#ifndef NODRAWTEXT
#define NODRAWTEXT
#endif

#ifndef NOGDI
#define NOGDI
#endif

#ifndef NOKERNEL
#define NOKERNEL
#endif

#ifndef NOUSER
#define NOUSER
#endif

#ifndef NONLS
#define NONLS
#endif

#ifndef NOMB
#define NOMB
#endif

#ifndef NOMEMMGR
#define NOMEMMGR
#endif

#ifndef NOMETAFILE
#define NOMETAFILE
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef NOMSG
#define NOMSG
#endif

#ifndef NOOPENFILE
#define NOOPENFILE
#endif

#ifndef NOSCROLL
#define NOSCROLL
#endif

#ifndef NOSERVICE
#define NOSERVICE
#endif

#ifndef NOSOUND
#define NOSOUND
#endif

#ifndef NOTEXTMETRIC
#define NOTEXTMETRIC
#endif

#ifndef NOWH
#define NOWH
#endif

#ifndef NOWINOFFSETS
#define NOWINOFFSETS
#endif

#ifndef NOCOMM
#define NOCOMM
#endif

#ifndef NOKANJI
#define NOKANJI
#endif

#ifndef NOHELP
#define NOHELP
#endif

#ifndef NOPROFILER
#define NOPROFILER
#endif

#ifndef NODEFERWINDOWPOS
#define NODEFERWINDOWPOS
#endif

#ifndef NOMCX
#define NOMCX
#endif

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#define recvfrom recvfrom_original
#define setsockopt setsockopt_original
#define socket socket_original
#define select select_original
#define WSAGetLastError WSAGetLastError_original
#define ioctlsocket ioctlsocket_original
#define htons htons_original
#define htonl htonl_original
#define WSAStartup WSAStartup_original
#define closesocket closesocket_original
#define WSACleanup WSACleanup_original
#define ntohs ntohs_original
#define getsockname getsockname_original
#define listen listen_original
#define bind bind_original
#define accept accept_original
#define ntohl ntohl_original
#define recv recv_original
#define connect connect_original
#define send send_original
#define getpeername getpeername_original
#define freeaddrinfo freeaddrinfo_original
#define inet_ntoa inet_ntoa_original
#define inet_addr inet_addr_original
#define getaddrinfo getaddrinfo_original
#define sendto sendto_original
#define __WSAFDIsSet __WSAFDIsSet_original

#include <winsock2.h>
#include <ws2tcpip.h>

#undef recvfrom
#undef setsockopt
#undef socket
#undef select
#undef WSAGetLastError
#undef ioctlsocket
#undef htons
#undef htonl
#undef WSAStartup
#undef closesocket
#undef WSACleanup
#undef ntohs
#undef getsockname
#undef listen
#undef bind
#undef accept
#undef ntohl
#undef recv
#undef connect
#undef send
#undef getpeername
#undef freeaddrinfo
#undef inet_ntoa
#undef inet_addr
#undef getaddrinfo
#undef sendto
#undef __WSAFDIsSet

extern decltype(recvfrom_original) *recvfrom;
extern decltype(setsockopt_original) *setsockopt;
extern decltype(socket_original) *socket;
extern decltype(select_original) *select;
extern decltype(WSAGetLastError_original) *WSAGetLastError;
extern decltype(ioctlsocket_original) *ioctlsocket;
extern decltype(htons_original) *htons;
extern decltype(htonl_original) *htonl;
extern decltype(WSAStartup_original) *WSAStartup;
extern decltype(closesocket_original) *closesocket;
extern decltype(WSACleanup_original) *WSACleanup;
extern decltype(ntohs_original) *ntohs;
extern decltype(getsockname_original) *getsockname;
extern decltype(listen_original) *listen;
extern decltype(bind_original) *bind;
extern decltype(accept_original) *accept;
extern decltype(ntohl_original) *ntohl;
extern decltype(recv_original) *recv;
extern decltype(connect_original) *connect;
extern decltype(send_original) *send;
extern decltype(getpeername_original) *getpeername;
extern decltype(freeaddrinfo_original) *freeaddrinfo;
extern decltype(inet_ntoa_original) *inet_ntoa;
extern decltype(inet_addr_original) *inet_addr;
extern decltype(getaddrinfo_original) *getaddrinfo;
extern decltype(sendto_original) *sendto;
extern decltype(__WSAFDIsSet_original) *__WSAFDIsSet;

#ifdef FD_ISSET
#undef FD_ISSET
#endif

#define FD_ISSET(fd, set) __WSAFDIsSet((SOCKET)(fd), (fd_set FAR *)(set))
