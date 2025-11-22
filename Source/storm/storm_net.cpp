#include "storm/storm_net.hpp"

#include <cstdint>
#include <memory>

#ifndef NONET
#include <mutex>
#include <thread>
#include <utility>

#include "utils/sdl_mutex.h"
#endif

#include "dvlnet/abstract_net.h"
#include "engine/demomode.h"
#include "headless_mode.hpp"
#include "menu.h"
#include "multi.h"
#include "options.h"
#include "utils/stubs.h"
#include "utils/utf8.hpp"

namespace devilution {

namespace {
std::unique_ptr<net::abstract_net> dvlnet_inst;
bool GameIsPublic = {};

#ifndef NONET
SdlMutex storm_net_mutex;
#endif
} // namespace

bool SNetReceiveMessage(uint8_t *senderplayerid, void **data, size_t *databytes)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	return dvlnet_inst->SNetReceiveMessage(senderplayerid, data, databytes);
}

bool SNetSendMessage(uint8_t playerID, void *data, size_t databytes)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	return dvlnet_inst->SNetSendMessage(playerID, data, databytes);
}

bool SNetReceiveTurns(int arraysize, char **arraydata, size_t *arraydatabytes, uint32_t *arrayplayerstatus)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	if (arraysize != MAX_PLRS)
		UNIMPLEMENTED();
	return dvlnet_inst->SNetReceiveTurns(arraydata, arraydatabytes, arrayplayerstatus);
}

bool SNetSendTurn(char *data, size_t databytes)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	return dvlnet_inst->SNetSendTurn(data, databytes);
}

void SNetGetProviderCaps(struct _SNETCAPS *caps)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	dvlnet_inst->SNetGetProviderCaps(caps);
}

bool SNetUnregisterEventHandler(event_type evtype)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	if (dvlnet_inst == nullptr)
		return true;
	return dvlnet_inst->SNetUnregisterEventHandler(evtype);
}

bool SNetRegisterEventHandler(event_type evtype, SEVTHANDLER func)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	return dvlnet_inst->SNetRegisterEventHandler(evtype, func);
}

bool SNetDestroy()
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	dvlnet_inst = nullptr;
	return true;
}

bool SNetDropPlayer(uint8_t playerid, uint32_t flags)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	return dvlnet_inst->SNetDropPlayer(playerid, flags);
}

bool SNetLeaveGame(int type)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	if (dvlnet_inst == nullptr)
		return true;
	return dvlnet_inst->SNetLeaveGame(type);
}

/**
 * @brief Called by engine for single, called by ui for multi
 * @param provider BNET, IPXN, MODM, SCBL or UDPN
 * @param gameData The game data
 */
bool SNetInitializeProvider(uint32_t provider, struct GameData *gameData)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	dvlnet_inst = net::abstract_net::MakeNet(provider);
	return (HeadlessMode && !demo::IsRunning()) || mainmenu_select_hero_dialog(gameData);
}

/**
 * @brief Called by engine for single, called by ui for multi
 */
bool SNetCreateGame(const char *pszGameName, const char *pszGamePassword, char *gameTemplateData, int gameTemplateSize, int *playerID)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	if (gameTemplateSize != sizeof(GameData))
		ABORT();
	net::buffer_t gameInitInfo(gameTemplateData, gameTemplateData + gameTemplateSize);
	dvlnet_inst->setup_gameinfo(std::move(gameInitInfo));

	std::string defaultName;
	if (pszGameName == nullptr) {
		defaultName = dvlnet_inst->make_default_gamename();
		pszGameName = defaultName.c_str();
	}

	GameName = pszGameName;
	if (pszGamePassword != nullptr)
		DvlNet_SetPassword(pszGamePassword);
	else
		DvlNet_ClearPassword();
	*playerID = dvlnet_inst->create(pszGameName);
	return *playerID != -1;
}

bool SNetJoinGame(char *pszGameName, char *pszGamePassword, int *playerID)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	if (pszGameName != nullptr)
		GameName = pszGameName;
	if (pszGamePassword != nullptr)
		DvlNet_SetPassword(pszGamePassword);
	else
		DvlNet_ClearPassword();
	*playerID = dvlnet_inst->join(pszGameName);
	return *playerID != -1;
}

/**
 * @brief Is this the mirror image of SNetGetTurnsInTransit?
 */
bool SNetGetOwnerTurnsWaiting(uint32_t *turns)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	return dvlnet_inst->SNetGetOwnerTurnsWaiting(turns);
}

bool SNetGetTurnsInTransit(uint32_t *turns)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	return dvlnet_inst->SNetGetTurnsInTransit(turns);
}

/**
 * @brief engine calls this only once with argument 1
 */
bool SNetSetBasePlayer(int /*unused*/)
{
#ifndef NONET
	std::lock_guard<SdlMutex> lg(storm_net_mutex);
#endif
	return true;
}

bool DvlNet_SendInfoRequest()
{
	return dvlnet_inst->send_info_request();
}

void DvlNet_ClearGamelist()
{
	return dvlnet_inst->clear_gamelist();
}

std::vector<GameInfo> DvlNet_GetGamelist()
{
	return dvlnet_inst->get_gamelist();
}

void DvlNet_SetPassword(std::string pw)
{
	GameIsPublic = false;
	GamePassword = pw;
	dvlnet_inst->setup_password(std::move(pw));
}

void DvlNet_ClearPassword()
{
	GameIsPublic = true;
	GamePassword.clear();
	dvlnet_inst->clear_password();
}

bool DvlNet_IsPublicGame()
{
	return GameIsPublic;
}

DvlNetLatencies DvlNet_GetLatencies(uint8_t playerId)
{
	return dvlnet_inst->get_latencies(playerId);
}

} // namespace devilution
