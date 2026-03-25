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
	BELT8 = 54,
	STASH1 = 55,
	STASH2 = 56,
	STASH3 = 57,
	STASH4 = 58,
	STASH5 = 59,
	STASH6 = 60,
	STASH7 = 61,
	STASH8 = 62,
	STASH9 = 63,
	STASH10 = 64,
	STASH11 = 65,
	STASH12 = 66,
	STASH13 = 67,
	STASH14 = 68,
	STASH15 = 69,
	STASH16 = 70,
	STASH17 = 71,
	STASH18 = 72,
	STASH19 = 73,
	STASH20 = 74,
	STASH21 = 75,
	STASH22 = 76,
	STASH23 = 77,
	STASH24 = 78,
	STASH25 = 79,
	STASH26 = 80,
	STASH27 = 81,
	STASH28 = 82,
	STASH29 = 83,
	STASH30 = 84,
	STASH31 = 85,
	STASH32 = 86,
	STASH33 = 87,
	STASH34 = 88,
	STASH35 = 89,
	STASH36 = 90,
	STASH37 = 91,
	STASH38 = 92,
	STASH39 = 93,
	STASH40 = 94,
	STASH41 = 95,
	STASH42 = 96,
	STASH43 = 97,
	STASH44 = 98,
	STASH45 = 99,
	STASH46 = 100,
	STASH47 = 101,
	STASH48 = 102,
	STASH49 = 103,
	STASH50 = 104,
	STASH51 = 105,
	STASH52 = 106,
	STASH53 = 107,
	STASH54 = 108,
	STASH55 = 109,
	STASH56 = 110,
	STASH57 = 111,
	STASH58 = 112,
	STASH59 = 113,
	STASH60 = 114,
	STASH61 = 115,
	STASH62 = 116,
	STASH63 = 117,
	STASH64 = 118,
	STASH65 = 119,
	STASH66 = 120,
	STASH67 = 121,
	STASH68 = 122,
	STASH69 = 123,
	STASH70 = 124,
	STASH71 = 125,
	STASH72 = 126,
	STASH73 = 127,
	STASH74 = 128,
	STASH75 = 129,
	STASH76 = 130,
	STASH77 = 131,
	STASH78 = 132,
	STASH79 = 133,
	STASH80 = 134,
	STASH81 = 135,
	STASH82 = 136,
	STASH83 = 137,
	STASH84 = 138,
	STASH85 = 139,
	STASH86 = 140,
	STASH87 = 141,
	STASH88 = 142,
	STASH89 = 143,
	STASH90 = 144,
	STASH91 = 145,
	STASH92 = 146,
	STASH93 = 147,
	STASH94 = 148,
	STASH95 = 149,
	STASH96 = 150,
	STASH97 = 151,
	STASH98 = 152,
	STASH99 = 153,
	STASH100 = 154
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
