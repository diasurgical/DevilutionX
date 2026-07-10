#pragma once

#include <cstdint>
#include_next <netinet/in.h>

struct in6_addr {
	uint8_t s6_addr[16];
};

struct ipv6_mreq {
	struct in6_addr ipv6mr_multiaddr;
	unsigned ipv6mr_interface;
};

struct sockaddr_in6 {
	sa_family_t sin6_family;
	in_port_t sin6_port;
	uint32_t sin6_flowinfo;
	struct in6_addr sin6_addr;
	uint32_t sin6_scope_id;
};

#define IPPROTO_IPV6 -1
#define IP_MULTICAST_IF -1
#define IPV6_JOIN_GROUP -1
#define IPV6_LEAVE_GROUP -1
#define IPV6_UNICAST_HOPS -1
#define IPV6_MULTICAST_HOPS -1
#define IPV6_MULTICAST_IF -1
#define IPV6_MULTICAST_LOOP -1
