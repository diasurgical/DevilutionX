#pragma once

#include_next <sys/socket.h>

#define SO_DEBUG 0
#define SO_DONTROUTE 0
#define SO_KEEPALIVE 0
#define SOMAXCONN 10
#define MSG_EOR 0

struct msghdr {
	void *msg_name;
	socklen_t msg_namelen;
	struct iovec *msg_iov;
	int msg_iovlen;
	void *msg_control;
	socklen_t msg_controllen;
	int msg_flags;
};

#ifdef __cplusplus
extern "C" {
#endif

ssize_t recvmsg(int socket, struct msghdr *message, int flags);
ssize_t sendmsg(int socket, const struct msghdr *message, int flags);
int socketpair(int domain, int type, int protocol, int socket_vector[2]);

#ifdef __cplusplus
}
#endif
