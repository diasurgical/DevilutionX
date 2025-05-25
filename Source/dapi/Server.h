#pragma once

#include <fstream>
#include <memory>
#include <sstream>

#include "Backend/DAPIBackendCore/DAPIProtoClient.h"
#include "GameData.h"

#include "control.h"
#include "cursor.h"
#include "diablo.h"
#include "engine/random.hpp"
#include "gamemenu.h"
#include "gmenu.h"
#include "inv.h"
#include "levels/gendung.h"
#include "minitext.h"
#include "missiles.h"
#include "msg.h"
#include "multi.h"
#include "objects.h"
#include "player.h"
#include "portal.h"
#include "qol/chatlog.h"
#include "spelldat.h"
#include "stores.h"
#include "towners.h"

namespace DAPI {
enum struct CommandType {
	STAND,
	WALKXY,
	ACK_PLRINFO,
	ADDSTR,
	ADDMAG,
	ADDDEX,
	ADDVIT,
	SBSPELL,
	GETITEM,
	AGETITEM,
	PUTITEM,
	RESPAWNITEM,
	ATTACKXY,
	RATTACKXY,
	SPELLXY,
	TSPELLXY,
	OPOBJXY,
	DISARMXY,
	ATTACKID,
	ATTACKPID,
	RATTACKID,
	RATTACKPID,
	SPELLID,
	SPELLPID,
	TSPELLID,
	TSPELLPID,
	RESURRECT,
	OPOBJT,
	KNOCKBACK,
	TALKXY,
	NEWLVL,
	WARP,
	CHEAT_EXPERIENCE,
	CHEAT_SPELL_LEVEL,
	DEBUG,
	SYNCDATA,
	MONSTDEATH,
	MONSTDAMAGE,
	PLRDEAD,
	REQUESTGITEM,
	REQUESTAGITEM,
	GOTOGETITEM,
	GOTOAGETITEM,
	OPENDOOR,
	CLOSEDOOR,
	OPERATEOBJ,
	PLROPOBJ,
	BREAKOBJ,
	CHANGEPLRITEMS,
	DELPLRITEMS,
	PLRDAMAGE,
	PLRLEVEL,
	DROPITEM,
	PLAYER_JOINLEVEL,
	SEND_PLRINFO,
	SATTACKXY,
	ACTIVATEPORTAL,
	DEACTIVATEPORTAL,
	DLEVEL_0,
	DLEVEL_1,
	DLEVEL_2,
	DLEVEL_3,
	DLEVEL_4,
	DLEVEL_5,
	DLEVEL_6,
	DLEVEL_7,
	DLEVEL_8,
	DLEVEL_9,
	DLEVEL_10,
	DLEVEL_11,
	DLEVEL_12,
	DLEVEL_13,
	DLEVEL_14,
	DLEVEL_15,
	DLEVEL_16,
	DLEVEL_JUNK,
	DLEVEL_END,
	HEALOTHER,
	STRING,
	SETSTR,
	SETMAG,
	SETDEX,
	SETVIT,
	RETOWN,
	SPELLXYD,
	ITEMEXTRA,
	SYNCPUTITEM,
	KILLGOLEM,
	SYNCQUEST,
	ENDSHIELD,
	AWAKEGOLEM,
	NOVA,
	SETSHIELD,
	REMSHIELD,
	FAKE_SETID,
	FAKE_DROPID,
	TOGGLECHARACTER,
	SETSPELL,
	TOGGLEINVENTORY,
	PUTINCURSOR,
	PUTCURSORITEM,
	IDENTIFYSTOREITEM,
	NUM_CMDS,
};

enum struct EquipSlot {
	HEAD = 0,
	RIGHTRING = 1,
	LEFTRING = 2,
	AMULET = 3,
	LEFTHAND = 4,
	RIGHTHAND = 5,
	BODY = 6,
	INV1 = 7,
	INV2 = 8,
	INV3 = 9,
	INV4 = 10,
	INV5 = 11,
	INV6 = 12,
	INV7 = 13,
	INV8 = 14,
	INV9 = 15,
	INV10 = 16,
	INV11 = 17,
	INV12 = 18,
	INV13 = 19,
	INV14 = 20,
	INV15 = 21,
	INV16 = 22,
	INV17 = 23,
	INV18 = 24,
	INV19 = 25,
	INV20 = 26,
	INV21 = 27,
	INV22 = 28,
	INV23 = 29,
	INV24 = 30,
	INV25 = 31,
	INV26 = 32,
	INV27 = 33,
	INV28 = 34,
	INV29 = 35,
	INV30 = 36,
	INV31 = 37,
	INV32 = 38,
	INV33 = 39,
	INV34 = 40,
	INV35 = 41,
	INV36 = 42,
	INV37 = 43,
	INV38 = 44,
	INV39 = 45,
	INV40 = 46,
	BELT1 = 47,
	BELT2 = 48,
	BELT3 = 49,
	BELT4 = 50,
	BELT5 = 51,
	BELT6 = 52,
	BELT7 = 53,
	BELT8 = 54
};

enum struct Backend {
	Vanilla109,
	DevilutionX
};

struct Server {
	Server();

	Server(const Server &other) = delete;
	Server(Server &&other) = delete;

	void update();
	bool isConnected() const;

	int FPS;
	std::ofstream output;

private:
	void processMessages();
	void checkForConnections();
	void updateGameData();
	bool isOnScreen(int x, int y);
	bool OKToAct();
	void move(int x, int y);
	void talk(int x, int y);
	void selectStoreOption(StoreOption option);
	void buyItem(int itemID);
	void sellItem(int itemID);
	void rechargeItem(int itemID);
	void repairItem(int itemID);
	void attackMonster(int index);
	void attackXY(int x, int y);
	void operateObject(int index);
	void useBeltItem(int slot);
	void toggleCharacterScreen();
	void increaseStat(CommandType commandType);
	void getItem(int itemID);
	void setSpell(int spellID, devilution::SpellType spellType);
	void castSpell(int index);
	void toggleInventory();
	void putInCursor(size_t itemID);
	void putCursorItem(int location);
	void dropCursorItem();
	void useItem(size_t itemID);
	void identifyStoreItem(int itemID);
	void castSpell(int x, int y);
	void cancelQText();
	void setFPS(int newFPS);
	void disarmTrap(int index);
	void skillRepair(int itemID);
	void skillRecharge(int itemID);
	void toggleMenu();
	void saveGame();
	void quit();
	void clearCursor();
	void identifyItem(int itemID);
	void sendChat(std::string message);

	bool listening = false;

	std::unique_ptr<GameData> data;

	std::map<std::pair<int, int>, bool> panelScreenCheck;

	DAPIProtoClient protoClient;
};
} // namespace DAPI
