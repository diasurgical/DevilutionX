#pragma once

#include_next <netinet/in.h>

struct ipv6_mreq {
	struct in6_addr ipv6mr_multiaddr;
	unsigned ipv6mr_interface;
};
