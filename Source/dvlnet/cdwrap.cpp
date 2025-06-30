#include "dvlnet/cdwrap.h"

namespace devilution::net {

void cdwrap::reset()
{
	dvlnet_wrap = make_net_fn_();
	dvlnet_wrap->setup_gameinfo(game_init_info);

	if (game_pw != std::nullopt) {
		dvlnet_wrap->setup_password(*game_pw);
	} else {
		dvlnet_wrap->clear_password();
	}

	for (const auto &[eventType, eventHandler] : registered_handlers)
		dvlnet_wrap->SNetRegisterEventHandler(eventType, eventHandler);
}

int cdwrap::create(std::string_view addrstr)
{
	reset();
	return dvlnet_wrap->create(addrstr);
}

int cdwrap::join(std::string_view addrstr)
{
	game_init_info = buffer_t();
	reset();
	return dvlnet_wrap->join(addrstr);
}

void cdwrap::setup_gameinfo(buffer_t info)
{
	game_init_info = std::move(info);
	if (dvlnet_wrap)
		dvlnet_wrap->setup_gameinfo(game_init_info);
}

bool cdwrap::SNetReceiveMessage(uint8_t *sender, void **data, size_t *size)
{
	return dvlnet_wrap->SNetReceiveMessage(sender, data, size);
}

bool cdwrap::SNetSendMessage(uint8_t playerID, void *data, size_t size)
{
	return dvlnet_wrap->SNetSendMessage(playerID, data, size);
}

bool cdwrap::SNetReceiveTurns(char **data, size_t *size, uint32_t *status)
{
	return dvlnet_wrap->SNetReceiveTurns(data, size, status);
}

bool cdwrap::SNetSendTurn(char *data, size_t size)
{
	return dvlnet_wrap->SNetSendTurn(data, size);
}

void cdwrap::SNetGetProviderCaps(struct _SNETCAPS *caps)
{
	dvlnet_wrap->SNetGetProviderCaps(caps);
}

bool cdwrap::SNetUnregisterEventHandler(event_type evtype)
{
	registered_handlers.erase(evtype);
	if (dvlnet_wrap)
		return dvlnet_wrap->SNetUnregisterEventHandler(evtype);
	return true;
}

bool cdwrap::SNetRegisterEventHandler(event_type evtype, SEVTHANDLER func)
{
	registered_handlers[evtype] = func;
	if (dvlnet_wrap)
		return dvlnet_wrap->SNetRegisterEventHandler(evtype, func);
	return true;
}

bool cdwrap::SNetLeaveGame(int type)
{
	return dvlnet_wrap->SNetLeaveGame(type);
}

bool cdwrap::SNetDropPlayer(int playerid, uint32_t flags)
{
	return dvlnet_wrap->SNetDropPlayer(playerid, flags);
}

bool cdwrap::SNetGetOwnerTurnsWaiting(uint32_t *turns)
{
	return dvlnet_wrap->SNetGetOwnerTurnsWaiting(turns);
}

bool cdwrap::SNetGetTurnsInTransit(uint32_t *turns)
{
	return dvlnet_wrap->SNetGetTurnsInTransit(turns);
}

std::string cdwrap::make_default_gamename()
{
	return dvlnet_wrap->make_default_gamename();
}

bool cdwrap::send_info_request()
{
	return dvlnet_wrap->send_info_request();
}

void cdwrap::clear_gamelist()
{
	dvlnet_wrap->clear_gamelist();
}

std::vector<GameInfo> cdwrap::get_gamelist()
{
	return dvlnet_wrap->get_gamelist();
}

void cdwrap::setup_password(std::string pw)
{
	game_pw = pw;
	return dvlnet_wrap->setup_password(pw);
}

void cdwrap::clear_password()
{
	game_pw = std::nullopt;
	return dvlnet_wrap->clear_password();
}

DvlNetLatencies cdwrap::get_latencies(uint8_t playerid)
{
	return dvlnet_wrap->get_latencies(playerid);
}

} // namespace devilution::net
