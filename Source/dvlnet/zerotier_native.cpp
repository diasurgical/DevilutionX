#include "dvlnet/zerotier_native.h"

#include <atomic>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>

#ifdef USE_SDL1
#include "utils/sdl2_to_1_2_backports.h"
#else
#include "utils/sdl2_backports.h"
#endif
#endif

#include <ankerl/unordered_dense.h>

#if defined(_WIN32) && !defined(DEVILUTIONX_WINDOWS_NO_WCHAR)
#include "utils/stdcompat/filesystem.hpp"
#ifdef DVL_HAS_FILESYSTEM
#define DVL_ZT_SYMLINK
#endif
#endif

#ifdef DVL_ZT_SYMLINK
#include <shlobj.h>
#include <sodium.h>

#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"
#endif

#include <ZeroTierSockets.h>
#include <cstdlib>

#include "utils/algorithm/container.hpp"
#include "utils/log.hpp"
#include "utils/paths.h"

#include "dvlnet/zerotier_lwip.h"

namespace devilution {
namespace net {

namespace {

// static constexpr uint64_t zt_earth = 0x8056c2e21c000001;
constexpr uint64_t ZtNetwork = 0xa84ac5c10a7ebb5f;

std::atomic_bool zt_network_ready(false);
std::atomic_bool zt_node_online(false);
std::atomic_bool zt_joined(false);
std::atomic_uint zt_peers_ready(0);

ankerl::unordered_dense::map<uint64_t, zts_event_t> ztPeerEvents;

#ifdef DVL_ZT_SYMLINK
bool HasMultiByteChars(std::string_view path)
{
	return c_any_of(path, IsTrailUtf8CodeUnit);
}

std::string ComputeAlternateFolderName(std::string_view path)
{
	const size_t hashSize = crypto_generichash_BYTES;
	unsigned char hash[hashSize];

	const int status = crypto_generichash(hash, hashSize,
	    reinterpret_cast<const unsigned char *>(path.data()), path.size(),
	    nullptr, 0);

	if (status != 0)
		return {};

	char buf[hashSize * 2];
	for (size_t i = 0; i < hashSize; ++i) {
		BufCopy(&buf[i * 2], AsHexPad2(hash[i]));
	}
	return std::string(buf, hashSize * 2);
}

std::string ToZTCompliantPath(std::string_view configPath)
{
	if (!HasMultiByteChars(configPath))
		return std::string(configPath);

	char commonAppDataPath[MAX_PATH];
	if (!SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, commonAppDataPath))) {
		LogVerbose("Failed to retrieve common application data path");
		return std::string(configPath);
	}

	std::error_code err;
	std::string alternateConfigPath = StrCat(commonAppDataPath, "\\diasurgical\\devilution");
	std::filesystem::create_directories(alternateConfigPath, err);
	if (err) {
		LogVerbose("Failed to create directories in ZT-compliant config path");
		return std::string(configPath);
	}

	std::string alternateFolderName = ComputeAlternateFolderName(configPath);
	if (alternateFolderName == "") {
		LogVerbose("Failed to hash config path for ZT");
		return std::string(configPath);
	}

	std::string symlinkPath = StrCat(alternateConfigPath, "\\", alternateFolderName);
	bool symlinkExists = std::filesystem::exists(
	    std::u8string_view(reinterpret_cast<const char8_t *>(symlinkPath.data()), symlinkPath.size()), err);
	if (err) {
		LogVerbose("Failed to determine if symlink for ZT-compliant config path exists");
		return std::string(configPath);
	}

	if (!symlinkExists) {
		std::filesystem::create_directory_symlink(
		    std::u8string_view(reinterpret_cast<const char8_t *>(configPath.data()), configPath.size()),
		    std::u8string_view(reinterpret_cast<const char8_t *>(symlinkPath.data()), symlinkPath.size()),
		    err);

		if (err) {
			LogVerbose("Failed to create symlink for ZT-compliant config path");
			return std::string(configPath);
		}
	}

	return StrCat(symlinkPath, "\\");
}
#endif

void Callback(void *ptr)
{
	zts_event_msg_t *msg = reinterpret_cast<zts_event_msg_t *>(ptr);

	switch (msg->event_code) {
	case ZTS_EVENT_NODE_ONLINE:
		Log("ZeroTier: ZTS_EVENT_NODE_ONLINE, nodeId={:x}", (unsigned long long)msg->node->node_id);
		zt_node_online = true;
		if (!zt_joined) {
			zts_net_join(ZtNetwork);
			zt_joined = true;
		}
		break;

	case ZTS_EVENT_NODE_OFFLINE:
		Log("ZeroTier: ZTS_EVENT_NODE_OFFLINE");
		zt_node_online = false;
		break;

	case ZTS_EVENT_NETWORK_READY_IP6:
		Log("ZeroTier: ZTS_EVENT_NETWORK_READY_IP6, networkId={:x}", (unsigned long long)msg->network->net_id);
		zt_ip6setup();
		zt_network_ready = true;
		zt_peers_ready = SDL_GetTicks();
		break;

	case ZTS_EVENT_ADDR_ADDED_IP6:
		print_ip6_addr(&(msg->addr->addr));
		break;

	case ZTS_EVENT_PEER_DIRECT:
	case ZTS_EVENT_PEER_RELAY:
		ztPeerEvents[msg->peer->peer_id] = static_cast<zts_event_t>(msg->event_code);
		if (!zerotier_peers_ready())
			zt_peers_ready = SDL_GetTicks();
		break;

	case ZTS_EVENT_PEER_PATH_DEAD:
		ztPeerEvents.erase(msg->peer->peer_id);
		break;
	}
}

} // namespace

bool zerotier_network_ready()
{
	return zt_network_ready && zt_node_online;
}

bool zerotier_peers_ready()
{
	return SDL_GetTicks() - zt_peers_ready >= 5000;
}

void zerotier_network_start()
{
	std::string configPath = paths::ConfigPath();
#ifdef DVL_ZT_SYMLINK
	configPath = ToZTCompliantPath(configPath);
#endif
	std::string ztpath = configPath + "zerotier";
	zts_init_from_storage(ztpath.c_str());
	zts_init_set_event_handler(&Callback);
	zts_node_start();
}

bool zerotier_is_relayed(uint64_t mac)
{
	bool isRelayed = true;
	if (zts_core_lock_obtain() != ZTS_ERR_OK)
		return isRelayed;
	zts_peer_info_t peerInfo;
	if (zts_core_query_peer_info(ZtNetwork, mac, &peerInfo) == ZTS_ERR_OK) {
		auto peerEvent = ztPeerEvents.find(peerInfo.peer_id);
		if (peerEvent != ztPeerEvents.end())
			isRelayed = (peerEvent->second == ZTS_EVENT_PEER_RELAY);
	}
	zts_core_lock_release();
	return isRelayed;
}

int zerotier_latency(uint64_t mac)
{
	int latency = -1;
	if (zts_core_lock_obtain() != ZTS_ERR_OK)
		return latency;
	zts_peer_info_t peerInfo;
	if (zts_core_query_peer_info(ZtNetwork, mac, &peerInfo) == ZTS_ERR_OK)
		latency = peerInfo.latency;
	zts_core_lock_release();
	return latency;
}

} // namespace net
} // namespace devilution
