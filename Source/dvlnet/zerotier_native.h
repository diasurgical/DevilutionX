#pragma once

#include <cstdint>

namespace devilution {
namespace net {

bool zerotier_network_ready();
bool zerotier_peers_ready();
void zerotier_network_start();
bool zerotier_is_relayed(uint64_t mac);
int zerotier_latency(uint64_t mac);

// NOTE: We have patched our libzt to have the corresponding multicast
// MAC hardcoded, since libzt is still missing the proper handling.
const unsigned char dvl_multicast_addr[16] = {
	0xff, 0x0e, 0xa8, 0xa9, 0xb6, 0x11, 0x60, 0xce,
	0x04, 0x12, 0xfd, 0x73, 0x37, 0x86, 0x6f, 0xb7
};

} // namespace net
} // namespace devilution
