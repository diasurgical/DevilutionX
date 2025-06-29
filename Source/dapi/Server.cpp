#include <thread>

#include "Server.h"
#include "qol/stash.h"

namespace DAPI {
Server::Server()
    : FPS(20)
{
	output.open("output.csv");
	data = std::make_unique<GameData>();
	for (int x = -8; x < 9; x++) {
		switch (x) {
		case 8:
			panelScreenCheck[std::make_pair(x, 3)] = true;
			break;
		case 7:
			for (int y = 2; y < 5; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case 6:
			for (int y = 1; y < 6; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case 5:
			for (int y = 0; y < 7; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case 4:
			for (int y = -1; y < 8; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case 3:
			for (int y = -2; y < 9; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case 2:
			for (int y = -3; y < 8; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case 1:
			for (int y = -4; y < 7; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case 0:
			for (int y = -5; y < 6; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case -1:
			for (int y = -6; y < 5; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case -2:
			for (int y = -7; y < 4; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case -3:
			for (int y = -8; y < 3; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case -4:
			for (int y = -7; y < 2; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case -5:
			for (int y = -6; y < 1; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case -6:
			for (int y = -5; y < 0; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case -7:
			for (int y = -4; y < -1; y++)
				panelScreenCheck[std::make_pair(x, y)] = true;
			break;
		case -8:
			panelScreenCheck[std::make_pair(x, -3)] = true;
			break;
		}
	}
}

void Server::update()
{
	if (isConnected()) {
		updateGameData();
		protoClient.transmitMessages();
		protoClient.receiveMessages();
		processMessages();
	} else {
		checkForConnections();
	}
}

bool Server::isConnected() const
{
	return protoClient.isConnected();
}

void Server::processMessages()
{
	bool issuedCommand = false;
	while (protoClient.messageQueueSize()) {
		auto message = protoClient.getNextMessage();
		if (message.get() == nullptr)
			return;
		if (message->has_endofqueue())
			return;

		if (message->has_command() && !issuedCommand) {
			auto command = message->command();
			if (command.has_move() && this->OKToAct()) {
				auto moveMessage = command.move();
				this->move(moveMessage.targetx(), moveMessage.targety());
			} else if (command.has_talk() && this->OKToAct()) {
				auto talkMessage = command.talk();
				this->talk(talkMessage.targetx(), talkMessage.targety());
			} else if (command.has_option() && devilution::ActiveStore != devilution::TalkID::None) {
				auto option = command.option();
				this->selectStoreOption(static_cast<StoreOption>(command.option().option()));
			} else if (command.has_buyitem()) {
				auto buyItem = command.buyitem();
				this->buyItem(buyItem.id());
			} else if (command.has_sellitem()) {
				auto sellItem = command.sellitem();
				this->sellItem(sellItem.id());
			} else if (command.has_rechargeitem()) {
				auto rechargeItem = command.rechargeitem();
				this->rechargeItem(rechargeItem.id());
			} else if (command.has_repairitem()) {
				auto repairItem = command.repairitem();
				this->repairItem(repairItem.id());
			} else if (command.has_attackmonster()) {
				auto attackMonster = command.attackmonster();
				this->attackMonster(attackMonster.index());
			} else if (command.has_attackxy()) {
				auto attackXY = command.attackxy();
				this->attackXY(attackXY.x(), attackXY.y());
			} else if (command.has_operateobject()) {
				auto operateObject = command.operateobject();
				this->operateObject(operateObject.index());
			} else if (command.has_usebeltitem()) {
				auto useBeltItem = command.usebeltitem();
				this->useBeltItem(useBeltItem.slot());
			} else if (command.has_togglecharactersheet()) {
				auto toggleCharacterSheet = command.togglecharactersheet();
				this->toggleCharacterScreen();
			} else if (command.has_increasestat()) {
				auto increaseStat = command.increasestat();
				this->increaseStat(static_cast<CommandType>(increaseStat.stat()));
			} else if (command.has_getitem()) {
				auto getItem = command.getitem();
				this->getItem(getItem.id());
			} else if (command.has_setspell()) {
				auto setSpell = command.setspell();
				this->setSpell(setSpell.spellid(), static_cast<devilution::SpellType>(setSpell.spelltype()));
			} else if (command.has_castmonster()) {
				auto castMonster = command.castmonster();
				this->castSpell(castMonster.index());
			} else if (command.has_toggleinventory()) {
				this->toggleInventory();
			} else if (command.has_putincursor()) {
				auto putInCursor = command.putincursor();
				this->putInCursor(putInCursor.id());
			} else if (command.has_putcursoritem()) {
				auto putCursorItem = command.putcursoritem();
				this->putCursorItem(putCursorItem.target());
			} else if (command.has_dropcursoritem()) {
				this->dropCursorItem();
			} else if (command.has_useitem()) {
				auto useItem = command.useitem();
				this->useItem(useItem.id());
			} else if (command.has_identifystoreitem()) {
				auto identifyStoreItem = command.identifystoreitem();
				this->identifyStoreItem(identifyStoreItem.id());
			} else if (command.has_castxy()) {
				auto castXY = command.castxy();
				this->castSpell(castXY.x(), castXY.y());
			} else if (command.has_cancelqtext()) {
				this->cancelQText();
			} else if (command.has_disarmtrap()) {
				auto disarmTrap = command.disarmtrap();
				this->disarmTrap(disarmTrap.index());
			} else if (command.has_skillrepair()) {
				auto skillRepair = command.skillrepair();
				this->skillRepair(skillRepair.id());
			} else if (command.has_skillrecharge()) {
				auto skillRecharge = command.skillrecharge();
				this->skillRecharge(skillRecharge.id());
			} else if (command.has_togglemenu()) {
				this->toggleMenu();
			} else if (command.has_savegame()) {
				this->saveGame();
			} else if (command.has_quit()) {
				this->quit();
			} else if (command.has_clearcursor()) {
				this->clearCursor();
			} else if (command.has_identifyitem()) {
				auto identifyItem = command.identifyitem();
				this->identifyItem(identifyItem.id());
			} else if (command.has_sendchat()) {
				auto sendChat = command.sendchat();
				this->sendChat(sendChat.message());
			}
			issuedCommand = true;
			if (command.has_setfps()) {
				auto setfps = command.setfps();
				this->setFPS(setfps.fps());
				issuedCommand = false;
			}
		}
	}
}

void Server::checkForConnections()
{
	if (isConnected())
		return;
	if (!listening) {
		protoClient.initListen();
		listening = false;
	}
	protoClient.checkForConnection();
	if (!protoClient.isConnected())
		return;
	protoClient.stopListen();
}

void Server::updateGameData()
{
	std::vector<int> itemsModified;

	auto fullFillItemInfo = [&](int itemID, devilution::Item *item) {
		itemsModified.push_back(itemID);

		data->itemList[itemID].ID = itemID;
		data->itemList[itemID]._iSeed = item->_iSeed;
		data->itemList[itemID]._iCreateInfo = item->_iCreateInfo;
		data->itemList[itemID]._itype = static_cast<int>(item->_itype);
		data->itemList[itemID]._ix = item->position.x;
		data->itemList[itemID]._iy = item->position.y;

		data->itemList[itemID]._iIdentified = item->_iIdentified;
		data->itemList[itemID]._iMagical = item->_iMagical;
		strcpy(data->itemList[itemID]._iName, item->_iName);
		if (data->itemList[itemID]._iIdentified) {
			strcpy(data->itemList[itemID]._iIName, item->_iIName);

			data->itemList[itemID]._iFlags = static_cast<int>(item->_iFlags);
			data->itemList[itemID]._iPrePower = item->_iPrePower;
			data->itemList[itemID]._iSufPower = item->_iSufPower;
			data->itemList[itemID]._iPLDam = item->_iPLDam;
			data->itemList[itemID]._iPLToHit = item->_iPLToHit;
			data->itemList[itemID]._iPLAC = item->_iPLAC;
			data->itemList[itemID]._iPLStr = item->_iPLStr;
			data->itemList[itemID]._iPLMag = item->_iPLMag;
			data->itemList[itemID]._iPLDex = item->_iPLDex;
			data->itemList[itemID]._iPLVit = item->_iPLVit;
			data->itemList[itemID]._iPLFR = item->_iPLFR;
			data->itemList[itemID]._iPLLR = item->_iPLLR;
			data->itemList[itemID]._iPLMR = item->_iPLMR;
			data->itemList[itemID]._iPLMana = item->_iPLMana;
			data->itemList[itemID]._iPLHP = item->_iPLHP;
			data->itemList[itemID]._iPLDamMod = item->_iPLDamMod;
			data->itemList[itemID]._iPLGetHit = item->_iPLGetHit;
			data->itemList[itemID]._iPLLight = item->_iPLLight;
			data->itemList[itemID]._iSplLvlAdd = item->_iSplLvlAdd;
			data->itemList[itemID]._iFMinDam = item->_iFMinDam;
			data->itemList[itemID]._iFMaxDam = item->_iFMaxDam;
			data->itemList[itemID]._iLMinDam = item->_iLMinDam;
			data->itemList[itemID]._iLMaxDam = item->_iLMaxDam;
		} else {
			strcpy(data->itemList[itemID]._iName, item->_iName);

			data->itemList[itemID]._iFlags = -1;
			data->itemList[itemID]._iPrePower = -1;
			data->itemList[itemID]._iSufPower = -1;
			data->itemList[itemID]._iPLDam = -1;
			data->itemList[itemID]._iPLToHit = -1;
			data->itemList[itemID]._iPLAC = -1;
			data->itemList[itemID]._iPLStr = -1;
			data->itemList[itemID]._iPLMag = -1;
			data->itemList[itemID]._iPLDex = -1;
			data->itemList[itemID]._iPLVit = -1;
			data->itemList[itemID]._iPLFR = -1;
			data->itemList[itemID]._iPLLR = -1;
			data->itemList[itemID]._iPLMR = -1;
			data->itemList[itemID]._iPLMana = -1;
			data->itemList[itemID]._iPLHP = -1;
			data->itemList[itemID]._iPLDamMod = -1;
			data->itemList[itemID]._iPLGetHit = -1;
			data->itemList[itemID]._iPLLight = -1;
			data->itemList[itemID]._iSplLvlAdd = -1;
			data->itemList[itemID]._iFMinDam = -1;
			data->itemList[itemID]._iFMaxDam = -1;
			data->itemList[itemID]._iLMinDam = -1;
			data->itemList[itemID]._iLMaxDam = -1;
		}
		data->itemList[itemID]._iClass = item->_iClass;
		data->itemList[itemID]._iCurs = item->_iCurs;
		if (item->_itype == devilution::ItemType::Gold)
			data->itemList[itemID]._ivalue = item->_ivalue;
		else
			data->itemList[itemID]._ivalue = -1;
		data->itemList[itemID]._iMinDam = item->_iMinDam;
		data->itemList[itemID]._iMaxDam = item->_iMaxDam;
		data->itemList[itemID]._iAC = item->_iAC;
		data->itemList[itemID]._iMiscId = item->_iMiscId;
		data->itemList[itemID]._iSpell = static_cast<int>(item->_iSpell);

		data->itemList[itemID]._iCharges = item->_iCharges;
		data->itemList[itemID]._iMaxCharges = item->_iMaxCharges;

		data->itemList[itemID]._iDurability = item->_iDurability;
		data->itemList[itemID]._iMaxDur = item->_iMaxDur;

		data->itemList[itemID]._iMinStr = item->_iMinStr;
		data->itemList[itemID]._iMinMag = item->_iMinMag;
		data->itemList[itemID]._iMinDex = item->_iMinDex;
		data->itemList[itemID]._iStatFlag = item->_iStatFlag;
		data->itemList[itemID].IDidx = item->IDidx;
	};

	auto partialFillItemInfo = [&](int itemID, devilution::Item *item) {
		itemsModified.push_back(itemID);

		data->itemList[itemID].ID = itemID;
		data->itemList[itemID]._iSeed = item->_iSeed;
		data->itemList[itemID]._iCreateInfo = item->_iCreateInfo;
		data->itemList[itemID]._itype = static_cast<int>(item->_itype);
		data->itemList[itemID]._ix = item->position.x;
		data->itemList[itemID]._iy = item->position.y;

		data->itemList[itemID]._iIdentified = item->_iIdentified;
		data->itemList[itemID]._iMagical = item->_iMagical;
		strcpy(data->itemList[itemID]._iName, item->_iName);
		if (data->itemList[itemID]._iIdentified)
			strcpy(data->itemList[itemID]._iIName, item->_iIName);
		else
			strcpy(data->itemList[itemID]._iName, item->_iName);
		data->itemList[itemID]._iFlags = -1;
		data->itemList[itemID]._iPrePower = -1;
		data->itemList[itemID]._iSufPower = -1;
		data->itemList[itemID]._iPLDam = -1;
		data->itemList[itemID]._iPLToHit = -1;
		data->itemList[itemID]._iPLAC = -1;
		data->itemList[itemID]._iPLStr = -1;
		data->itemList[itemID]._iPLMag = -1;
		data->itemList[itemID]._iPLDex = -1;
		data->itemList[itemID]._iPLVit = -1;
		data->itemList[itemID]._iPLFR = -1;
		data->itemList[itemID]._iPLLR = -1;
		data->itemList[itemID]._iPLMR = -1;
		data->itemList[itemID]._iPLMana = -1;
		data->itemList[itemID]._iPLHP = -1;
		data->itemList[itemID]._iPLDamMod = -1;
		data->itemList[itemID]._iPLGetHit = -1;
		data->itemList[itemID]._iPLLight = -1;
		data->itemList[itemID]._iSplLvlAdd = -1;
		data->itemList[itemID]._iFMinDam = -1;
		data->itemList[itemID]._iFMaxDam = -1;
		data->itemList[itemID]._iLMinDam = -1;
		data->itemList[itemID]._iLMaxDam = -1;
		data->itemList[itemID]._iClass = item->_iClass;
		data->itemList[itemID]._iCurs = item->_iCurs;
		if (item->_itype == devilution::ItemType::Gold)
			data->itemList[itemID]._ivalue = item->_ivalue;
		else
			data->itemList[itemID]._ivalue = -1;
		data->itemList[itemID]._iMinDam = -1;
		data->itemList[itemID]._iMaxDam = -1;
		data->itemList[itemID]._iAC = -1;
		data->itemList[itemID]._iMiscId = item->_iMiscId;
		data->itemList[itemID]._iSpell = static_cast<int>(item->_iSpell);

		data->itemList[itemID]._iCharges = -1;
		data->itemList[itemID]._iMaxCharges = -1;

		data->itemList[itemID]._iDurability = -1;
		data->itemList[itemID]._iMaxDur = -1;

		data->itemList[itemID]._iMinStr = -1;
		data->itemList[itemID]._iMinMag = -1;
		data->itemList[itemID]._iMinDex = -1;
		data->itemList[itemID]._iStatFlag = -1;
		data->itemList[itemID].IDidx = item->IDidx;
	};

	auto isOnScreen = [&](int x, int y) {
		int dx = devilution::Players[devilution::MyPlayerId].position.tile.x - x;
		int dy = devilution::Players[devilution::MyPlayerId].position.tile.y - y;
		if (!devilution::CharFlag && !devilution::invflag) {
			if (dy > 0) {
				if (dx < 1 && abs(dx) + abs(dy) < 11) {
					return true;
				} else if (dx > 0 && abs(dx) + abs(dy) < 12) {
					return true;
				}
			} else {
				if ((dx > -1 || dy == 0) && abs(dx) + abs(dy) < 11) {
					return true;
				} else if ((dx < 0 && dy != 0) && abs(dx) + abs(dy) < 12) {
					return true;
				}
			}
		} else if ((devilution::CharFlag && !devilution::invflag) || (!devilution::CharFlag && devilution::invflag)) {
			return panelScreenCheck[std::make_pair(dx, dy)];
		}
		return false;
	};

	auto message = std::make_unique<dapi::message::Message>();
	auto update = message->mutable_frameupdate();

	update->set_connectedto(1);

	for (auto chatLogLine = devilution::ChatLogLines.size() - (devilution::MessageCounter - data->lastLogSize); chatLogLine < devilution::ChatLogLines.size(); chatLogLine++) {
		std::stringstream message;
		for (auto &textLine : devilution::ChatLogLines[chatLogLine].colors) {
			if (devilution::HasAnyOf(textLine.color, devilution::UiFlags::ColorWhitegold) || devilution::HasAnyOf(textLine.color, devilution::UiFlags::ColorBlue))
				message << textLine.text << ": ";
			if (devilution::HasAnyOf(textLine.color, devilution::UiFlags::ColorWhite))
				message << textLine.text;
		}
		if (message.str().size()) {
			auto chatMessage = update->add_chatmessages();
			*chatMessage = message.str();
		}
	}
	data->lastLogSize = devilution::MessageCounter;

	update->set_player(devilution::MyPlayerId);

	update->set_stextflag(static_cast<char>(devilution::ActiveStore));
	update->set_pausemode(devilution::PauseMode);
	if (devilution::sgpCurrentMenu != nullptr)
		data->menuOpen = true;
	else
		data->menuOpen = false;
	update->set_menuopen(static_cast<int>(data->menuOpen));
	update->set_cursor(devilution::pcurs);
	data->chrflag = devilution::CharFlag;
	update->set_chrflag(devilution::CharFlag);
	data->invflag = devilution::invflag;
	update->set_invflag(devilution::invflag);
	data->qtextflag = devilution::qtextflag;
	update->set_qtextflag(devilution::qtextflag);
	if (!devilution::setlevel)
		data->currlevel = static_cast<int>(devilution::currlevel);
	else
		data->currlevel = static_cast<int>(devilution::setlvlnum);
	update->set_currlevel(data->currlevel);
	update->set_setlevel(devilution::setlevel);
	if (devilution::qtextflag) {
		std::stringstream qtextss;
		for (auto &line : devilution::TextLines) {
			if (qtextss.str().size())
				qtextss << " ";
			qtextss << line;
		}
		update->set_qtext(qtextss.str().c_str());
	} else
		update->set_qtext("");
	update->set_fps(FPS);
	if (!devilution::gbIsMultiplayer)
		update->set_gamemode(0);
	else
		update->set_gamemode(1);

	update->set_gndifficulty(devilution::sgGameInitInfo.nDifficulty);

	for (int x = 0; x < 112; x++) {
		for (int y = 0; y < 112; y++) {
			if (isOnScreen(x, y)) {
				auto dpiece = update->add_dpiece();
				dpiece->set_type(devilution::dPiece[x][y]);
				dpiece->set_solid(HasAnyOf(devilution::SOLData[dpiece->type()], devilution::TileProperties::Solid));
				dpiece->set_x(x);
				dpiece->set_y(y);
				dpiece->set_stopmissile(HasAnyOf(devilution::SOLData[dpiece->type()], devilution::TileProperties::BlockMissile));
			}
		}
	}

	for (int i = 0; i < devilution::numtrigs; i++) {
		if (isOnScreen(devilution::trigs[i].position.x, devilution::trigs[i].position.y)) {
			auto trigger = update->add_triggerdata();
			trigger->set_lvl(devilution::trigs[i]._tlvl);
			trigger->set_x(devilution::trigs[i].position.x);
			trigger->set_y(devilution::trigs[i].position.y);
			// Adding 0x402 to the message stored in the trigger to translate to what the front end expects.
			// The front end uses what Diablo 1.09 uses internally.
			trigger->set_type(static_cast<int>(devilution::trigs[i]._tmsg) + 0x402);
		}
	}

	for (int i = 0; i < MAXQUESTS; i++) {
		auto quest = update->add_questdata();
		quest->set_id(i);
		if (devilution::Quests[i]._qactive == 2)
			quest->set_state(devilution::Quests[i]._qactive);
		else
			quest->set_state(0);

		if (devilution::currlevel == devilution::Quests[i]._qlevel && devilution::Quests[i]._qslvl != 0 && devilution::Quests[i]._qactive != devilution::quest_state::QUEST_NOTAVAIL) {
			auto trigger = update->add_triggerdata();
			trigger->set_lvl(devilution::Quests[i]._qslvl);
			trigger->set_x(devilution::Quests[i].position.x);
			trigger->set_y(devilution::Quests[i].position.y);
			trigger->set_type(1029);
		}
	}

	data->storeList.clear();
	// Check for qtextflag added for DevilutionX so that store options are not transmitted
	// while qtext is up.
	if (!devilution::qtextflag && devilution::ActiveStore != devilution::TalkID::None) {
		for (int i = 0; i < 24; i++) {
			if (devilution::TextLine[i].isSelectable()) {
				if (!strcmp(devilution::TextLine[i].text.c_str(), "Talk to Cain") || !strcmp(devilution::TextLine[i].text.c_str(), "Talk to Farnham") || !strcmp(devilution::TextLine[i].text.c_str(), "Talk to Pepin") || !strcmp(devilution::TextLine[i].text.c_str(), "Talk to Gillian") || !strcmp(devilution::TextLine[i].text.c_str(), "Talk to Ogden") || !strcmp(devilution::TextLine[i].text.c_str(), "Talk to Griswold") || !strcmp(devilution::TextLine[i].text.c_str(), "Talk to Adria") || !strcmp(devilution::TextLine[i].text.c_str(), "Talk to Wirt"))
					data->storeList.push_back(StoreOption::TALK);
				else if (!strcmp(devilution::TextLine[i].text.c_str(), "Identify an item"))
					data->storeList.push_back(StoreOption::IDENTIFYANITEM);
				else if (!strcmp(devilution::TextLine[i].text.c_str(), "Say goodbye") || !strcmp(devilution::TextLine[i].text.c_str(), "Say Goodbye") || !strcmp(devilution::TextLine[i].text.c_str(), "Leave Healer's home") || !strcmp(devilution::TextLine[i].text.c_str(), "Leave the shop") || !strcmp(devilution::TextLine[i].text.c_str(), "Leave the shack") || !strcmp(devilution::TextLine[i].text.c_str(), "Leave the tavern") || !strcmp(devilution::TextLine[i].text.c_str(), "Leave"))
					data->storeList.push_back(StoreOption::EXIT);
				else if (!strcmp(devilution::TextLine[i].text.c_str(), "Receive healing"))
					data->storeList.push_back(StoreOption::HEAL);
				else if (!strcmp(devilution::TextLine[i].text.c_str(), "Buy items"))
					data->storeList.push_back(StoreOption::BUYITEMS);
				else if (!strcmp(devilution::TextLine[i].text.c_str(), "What have you got?"))
					data->storeList.push_back(StoreOption::WIRTPEEK);
				else if (!strcmp(devilution::TextLine[i].text.c_str(), "Buy basic items"))
					data->storeList.push_back(StoreOption::BUYBASIC);
				else if (!strcmp(devilution::TextLine[i].text.c_str(), "Buy premium items"))
					data->storeList.push_back(StoreOption::BUYPREMIUM);
				else if (!strcmp(devilution::TextLine[i].text.c_str(), "Sell items"))
					data->storeList.push_back(StoreOption::SELL);
				else if (!strcmp(devilution::TextLine[i].text.c_str(), "Repair items"))
					data->storeList.push_back(StoreOption::REPAIR);
				else if (!strcmp(devilution::TextLine[i].text.c_str(), "Recharge staves"))
					data->storeList.push_back(StoreOption::RECHARGE);
			}
		}

		switch (devilution::ActiveStore) {
		case devilution::TalkID::HealerBuy:
		case devilution::TalkID::StorytellerIdentifyShow:
		case devilution::TalkID::NoMoney:
		case devilution::TalkID::NoRoom:
		case devilution::TalkID::SmithBuy:
		case devilution::TalkID::StorytellerIdentify:
		case devilution::TalkID::SmithPremiumBuy:
		case devilution::TalkID::SmithRepair:
		case devilution::TalkID::SmithSell:
		case devilution::TalkID::WitchBuy:
		case devilution::TalkID::WitchRecharge:
		case devilution::TalkID::WitchSell:
		case devilution::TalkID::Gossip:
			data->storeList.push_back(StoreOption::BACK);
			break;
		default:
			break;
		}
	}

	for (auto &option : data->storeList)
		update->add_storeoption(static_cast<int>(option));

	data->groundItems.clear();

	for (size_t i = 0; i < (devilution::gbIsMultiplayer ? 4 : 1); /* devilution::gbActivePlayers;*/ i++) {
		auto playerData = update->add_playerdata();

		data->playerList[i].InvBody.clear();

		data->playerList[i].pnum = i;
		playerData->set_pnum(i);

		for (int j = 0; j < MAXINV; j++)
			data->playerList[i].InvList[j] = -1;

		if (devilution::MyPlayerId == i && devilution::Players.size() >= i + 1) {
			memcpy(data->playerList[i]._pName, devilution::Players[i]._pName, 32);
			playerData->set__pname(data->playerList[i]._pName);

			data->playerList[i]._pmode = devilution::Players[i]._pmode;
			data->playerList[i].plrlevel = devilution::Players[i].plrlevel;
			data->playerList[i]._px = devilution::Players[i].position.tile.x;
			data->playerList[i]._py = devilution::Players[i].position.tile.y;
			data->playerList[i]._pfutx = devilution::Players[i].position.future.x;
			data->playerList[i]._pfuty = devilution::Players[i].position.future.y;
			data->playerList[i]._pdir = static_cast<int>(devilution::Players[i]._pdir);

			data->playerList[i]._pRSpell = static_cast<int>(devilution::Players[i]._pRSpell);
			data->playerList[i]._pRSplType = static_cast<char>(devilution::Players[i]._pRSplType);

			memcpy(data->playerList[i]._pSplLvl, devilution::Players[i]._pSplLvl, sizeof(data->playerList[i]._pSplLvl));
			data->playerList[i]._pMemSpells = devilution::Players[i]._pMemSpells;
			data->playerList[i]._pAblSpells = devilution::Players[i]._pAblSpells;
			data->playerList[i]._pScrlSpells = devilution::Players[i]._pScrlSpells;

			data->playerList[i]._pClass = static_cast<char>(devilution::Players[i]._pClass);

			data->playerList[i]._pStrength = devilution::Players[i]._pStrength;
			data->playerList[i]._pBaseStr = devilution::Players[i]._pBaseStr;
			data->playerList[i]._pMagic = devilution::Players[i]._pMagic;
			data->playerList[i]._pBaseMag = devilution::Players[i]._pBaseMag;
			data->playerList[i]._pDexterity = devilution::Players[i]._pDexterity;
			data->playerList[i]._pBaseDex = devilution::Players[i]._pBaseDex;
			data->playerList[i]._pVitality = devilution::Players[i]._pVitality;
			data->playerList[i]._pBaseVit = devilution::Players[i]._pBaseVit;

			data->playerList[i]._pStatPts = devilution::Players[i]._pStatPts;

			data->playerList[i]._pDamageMod = devilution::Players[i]._pDamageMod;

			data->playerList[i]._pHitPoints = devilution::Players[i]._pHitPoints;
			data->playerList[i]._pMaxHP = devilution::Players[i]._pMaxHP;
			data->playerList[i]._pMana = devilution::Players[i]._pMana;
			data->playerList[i]._pMaxMana = devilution::Players[i]._pMaxMana;
			data->playerList[i]._pLevel = devilution::Players[i].getCharacterLevel();
			data->playerList[i]._pExperience = devilution::Players[i]._pExperience;

			data->playerList[i]._pArmorClass = devilution::Players[i]._pArmorClass;

			data->playerList[i]._pMagResist = devilution::Players[i]._pMagResist;
			data->playerList[i]._pFireResist = devilution::Players[i]._pFireResist;
			data->playerList[i]._pLightResist = devilution::Players[i]._pLghtResist;

			data->playerList[i]._pGold = devilution::Players[i]._pGold;

			for (int j = 0; j < NUM_INVLOC; j++) {
				if (devilution::Players[i].InvBody[j]._itype == devilution::ItemType::None) {
					data->playerList[i].InvBody[j] = -1;
					continue;
				}

				size_t itemID = data->itemList.size();
				for (size_t k = 0; k < data->itemList.size(); k++) {
					if (data->itemList[k].compare(devilution::Players[i].InvBody[j])) {
						itemID = k;
						break;
					}
				}
				if (itemID == data->itemList.size())
					data->itemList.push_back(ItemData {});
				fullFillItemInfo(itemID, &devilution::Players[i].InvBody[j]);
				data->playerList[i].InvBody[j] = itemID;
			}
			bool used[40] = { false };
			for (int j = 0; j < MAXINV; j++) {
				auto index = devilution::Players[i].InvGrid[j];
				if (index != 0) {
					size_t itemID = data->itemList.size();
					for (size_t k = 0; k < data->itemList.size(); k++) {
						if (data->itemList[k].compare(devilution::Players[i].InvList[abs(index) - 1])) {
							itemID = k;
							break;
						}
					}
					data->playerList[i].InvGrid[j] = itemID;
					if (!used[abs(index) - 1]) {
						if (itemID == data->itemList.size())
							data->itemList.push_back(ItemData {});
						fullFillItemInfo(itemID, &devilution::Players[i].InvList[abs(index) - 1]);
						used[abs(index) - 1] = true;
						data->playerList[i].InvList[abs(index) - 1] = itemID;
					}
				} else
					data->playerList[i].InvGrid[j] = -1;
			}
			for (int j = 0; j < MAXSPD; j++) {
				if (devilution::Players[i].SpdList[j]._itype == devilution::ItemType::None) {
					data->playerList[i].SpdList[j] = -1;
					continue;
				}

				size_t itemID = data->itemList.size();
				for (size_t k = 0; k < data->itemList.size(); k++) {
					if (data->itemList[k].compare(devilution::Players[i].SpdList[j])) {
						itemID = k;
						break;
					}
				}
				if (itemID == data->itemList.size())
					data->itemList.push_back(ItemData {});
				fullFillItemInfo(itemID, &devilution::Players[i].SpdList[j]);
				data->playerList[i].SpdList[j] = itemID;
			}
			if (devilution::pcurs < 12)
				data->playerList[i].HoldItem = -1;
			else {
				size_t itemID = data->itemList.size();
				for (size_t j = 0; j < data->itemList.size(); j++) {
					if (data->itemList[j].compare(devilution::Players[i].HoldItem)) {
						itemID = j;
						break;
					}
				}
				if (itemID == data->itemList.size())
					data->itemList.push_back(ItemData {});
				partialFillItemInfo(itemID, &devilution::Players[i].HoldItem);
				data->playerList[i].HoldItem = itemID;
			}

			data->playerList[i]._pIMinDam = devilution::Players[i]._pIMinDam;
			data->playerList[i]._pIMaxDam = devilution::Players[i]._pIMaxDam;
			data->playerList[i]._pIBonusDam = devilution::Players[i]._pIBonusDam;
			data->playerList[i]._pIAC = devilution::Players[i]._pIAC;
			data->playerList[i]._pIBonusToHit = devilution::Players[i]._pIBonusToHit;
			data->playerList[i]._pIBonusAC = devilution::Players[i]._pIBonusAC;
			data->playerList[i]._pIBonusDamMod = devilution::Players[i]._pIBonusDamMod;
			data->playerList[i].pManaShield = devilution::Players[i].pManaShield;
		} else if (devilution::Players.size() >= i + 1 && devilution::Players[i].plractive && devilution::Players[i].isOnActiveLevel() && devilution::IsTileLit(devilution::Point { devilution::Players[i].position.tile.x, devilution::Players[i].position.tile.y })) {
			memcpy(data->playerList[i]._pName, devilution::Players[i]._pName, 32);
			playerData->set__pname(data->playerList[i]._pName);

			data->playerList[i]._pmode = devilution::Players[i]._pmode;
			data->playerList[i].plrlevel = devilution::Players[i].plrlevel;
			data->playerList[i]._px = devilution::Players[i].position.tile.x;
			data->playerList[i]._py = devilution::Players[i].position.tile.y;
			data->playerList[i]._pfutx = devilution::Players[i].position.future.x;
			data->playerList[i]._pfuty = devilution::Players[i].position.future.y;
			data->playerList[i]._pdir = static_cast<int>(devilution::Players[i]._pdir);

			data->playerList[i]._pRSpell = -1;
			data->playerList[i]._pRSplType = 4;

			memset(data->playerList[i]._pSplLvl, 0, 64);
			data->playerList[i]._pMemSpells = -1;
			data->playerList[i]._pAblSpells = -1;
			data->playerList[i]._pScrlSpells = -1;

			data->playerList[i]._pClass = static_cast<char>(devilution::Players[i]._pClass);

			data->playerList[i]._pStrength = -1;
			data->playerList[i]._pBaseStr = -1;
			data->playerList[i]._pMagic = -1;
			data->playerList[i]._pBaseMag = -1;
			data->playerList[i]._pDexterity = -1;
			data->playerList[i]._pBaseDex = -1;
			data->playerList[i]._pVitality = -1;
			data->playerList[i]._pBaseVit = -1;

			data->playerList[i]._pStatPts = -1;

			data->playerList[i]._pDamageMod = -1;

			data->playerList[i]._pHitPoints = devilution::Players[i]._pHitPoints;
			data->playerList[i]._pMaxHP = devilution::Players[i]._pMaxHP;
			data->playerList[i]._pMana = -1;
			data->playerList[i]._pMaxMana = -1;
			data->playerList[i]._pLevel = devilution::Players[i].getCharacterLevel();
			data->playerList[i]._pExperience = -1;

			data->playerList[i]._pArmorClass = -1;

			data->playerList[i]._pMagResist = -1;
			data->playerList[i]._pFireResist = -1;
			data->playerList[i]._pLightResist = -1;

			data->playerList[i]._pGold = -1;

			for (int j = 0; j < NUM_INVLOC; j++)
				data->playerList[i].InvBody[j] = -1;
			for (int j = 0; j < MAXINV; j++)
				data->playerList[i].InvGrid[j] = -1;
			for (int j = 0; j < MAXSPD; j++)
				data->playerList[i].SpdList[j] = -1;
			data->playerList[i].HoldItem = -1;

			data->playerList[i]._pIMinDam = -1;
			data->playerList[i]._pIMaxDam = -1;
			data->playerList[i]._pIBonusDam = -1;
			data->playerList[i]._pIAC = -1;
			data->playerList[i]._pIBonusToHit = -1;
			data->playerList[i]._pIBonusAC = -1;
			data->playerList[i]._pIBonusDamMod = -1;
			data->playerList[i].pManaShield = devilution::Players[i].pManaShield;
		} else {
			memset(data->playerList[i]._pName, 0, 32);
			playerData->set__pname(data->playerList[i]._pName);

			data->playerList[i]._pmode = 0;
			data->playerList[i].plrlevel = -1;
			data->playerList[i]._px = -1;
			data->playerList[i]._py = -1;
			data->playerList[i]._pfutx = -1;
			data->playerList[i]._pfuty = -1;
			data->playerList[i]._pdir = -1;

			data->playerList[i]._pRSpell = -1;
			data->playerList[i]._pRSplType = -1;

			memset(data->playerList[i]._pSplLvl, 0, 64);
			data->playerList[i]._pMemSpells = -1;
			data->playerList[i]._pAblSpells = -1;
			data->playerList[i]._pScrlSpells = -1;

			data->playerList[i]._pClass = -1;

			data->playerList[i]._pStrength = -1;
			data->playerList[i]._pBaseStr = -1;
			data->playerList[i]._pMagic = -1;
			data->playerList[i]._pBaseMag = -1;
			data->playerList[i]._pDexterity = -1;
			data->playerList[i]._pBaseDex = -1;
			data->playerList[i]._pVitality = -1;
			data->playerList[i]._pBaseVit = -1;

			data->playerList[i]._pStatPts = -1;

			data->playerList[i]._pDamageMod = -1;

			data->playerList[i]._pHitPoints = -1;
			data->playerList[i]._pMaxHP = -1;
			data->playerList[i]._pMana = -1;
			data->playerList[i]._pMaxMana = -1;
			data->playerList[i]._pLevel = devilution::Players[i].getCharacterLevel();
			data->playerList[i]._pExperience = -1;

			data->playerList[i]._pArmorClass = -1;

			data->playerList[i]._pMagResist = -1;
			data->playerList[i]._pFireResist = -1;
			data->playerList[i]._pLightResist = -1;
			data->playerList[i]._pIBonusToHit = -1;

			data->playerList[i]._pGold = -1;
			for (int j = 0; j < NUM_INVLOC; j++)
				data->playerList[i].InvBody[j] = -1;
			for (int j = 0; j < MAXINV; j++)
				data->playerList[i].InvGrid[j] = -1;
			for (int j = 0; j < MAXSPD; j++)
				data->playerList[i].SpdList[j] = -1;
			data->playerList[i].HoldItem = -1;

			data->playerList[i]._pIMinDam = -1;
			data->playerList[i]._pIMaxDam = -1;
			data->playerList[i]._pIBonusDam = -1;
			data->playerList[i]._pIAC = -1;
			data->playerList[i]._pIBonusToHit = -1;
			data->playerList[i]._pIBonusAC = -1;
			data->playerList[i]._pIBonusDamMod = -1;
			data->playerList[i].pManaShield = false;
		}

		playerData->set__pmode(data->playerList[i]._pmode);
		playerData->set_plrlevel(data->playerList[i].plrlevel);
		playerData->set__px(data->playerList[i]._px);
		playerData->set__py(data->playerList[i]._py);
		playerData->set__pfutx(data->playerList[i]._pfutx);
		playerData->set__pfuty(data->playerList[i]._pfuty);
		playerData->set__pdir(data->playerList[i]._pdir);

		playerData->set__prspell(data->playerList[i]._pRSpell);
		playerData->set__prspltype(data->playerList[i]._pRSplType);

		for (int j = 0; j < 64; j++)
			playerData->add__pspllvl(data->playerList[i]._pSplLvl[j]);
		playerData->set__pmemspells(data->playerList[i]._pMemSpells);
		playerData->set__pablspells(data->playerList[i]._pAblSpells);
		playerData->set__pscrlspells(data->playerList[i]._pScrlSpells);

		playerData->set__pclass(data->playerList[i]._pClass);

		playerData->set__pstrength(data->playerList[i]._pStrength);
		playerData->set__pbasestr(data->playerList[i]._pBaseStr);
		playerData->set__pmagic(data->playerList[i]._pMagic);
		playerData->set__pbasemag(data->playerList[i]._pBaseMag);
		playerData->set__pdexterity(data->playerList[i]._pDexterity);
		playerData->set__pbasedex(data->playerList[i]._pBaseDex);
		playerData->set__pvitality(data->playerList[i]._pVitality);
		playerData->set__pbasevit(data->playerList[i]._pBaseVit);

		playerData->set__pstatpts(data->playerList[i]._pStatPts);

		playerData->set__pdamagemod(data->playerList[i]._pDamageMod);

		playerData->set__phitpoints(data->playerList[i]._pHitPoints);
		playerData->set__pmaxhp(data->playerList[i]._pMaxHP);
		playerData->set__pmana(data->playerList[i]._pMana);
		playerData->set__pmaxmana(data->playerList[i]._pMaxMana);
		playerData->set__plevel(data->playerList[i]._pLevel);
		playerData->set__pexperience(data->playerList[i]._pExperience);

		playerData->set__parmorclass(data->playerList[i]._pArmorClass);

		playerData->set__pmagresist(data->playerList[i]._pMagResist);
		playerData->set__pfireresist(data->playerList[i]._pFireResist);
		playerData->set__plightresist(data->playerList[i]._pLightResist);

		playerData->set__pgold(data->playerList[i]._pGold);
		for (int j = 0; j < NUM_INVLOC; j++)
			playerData->add_invbody(data->playerList[i].InvBody[j]);
		for (int j = 0; j < MAXINV; j++)
			playerData->add_invlist(data->playerList[i].InvList[j]);
		for (int j = 0; j < MAXINV; j++)
			playerData->add_invgrid(data->playerList[i].InvGrid[j]);
		for (int j = 0; j < MAXSPD; j++)
			playerData->add_spdlist(data->playerList[i].SpdList[j]);
		playerData->set_holditem(data->playerList[i].HoldItem);

		playerData->set__pimindam(data->playerList[i]._pIMinDam);
		playerData->set__pimaxdam(data->playerList[i]._pIMaxDam);
		playerData->set__pibonusdam(data->playerList[i]._pIBonusDam);
		playerData->set__piac(data->playerList[i]._pIAC);
		playerData->set__pibonustohit(data->playerList[i]._pIBonusToHit);
		playerData->set__pibonusac(data->playerList[i]._pIBonusAC);
		playerData->set__pibonusdammod(data->playerList[i]._pIBonusDamMod);
		playerData->set_pmanashield(data->playerList[i].pManaShield);
	}

	auto emptyFillItemInfo = [&](int itemID, devilution::Item *item) {
		itemsModified.push_back(itemID);

		data->itemList[itemID].ID = itemID;
		data->itemList[itemID]._iSeed = item->_iSeed;
		data->itemList[itemID]._iCreateInfo = item->_iCreateInfo;
		data->itemList[itemID]._itype = static_cast<int>(item->_itype);
		data->itemList[itemID]._ix = -1;
		data->itemList[itemID]._iy = -1;

		data->itemList[itemID]._iIdentified = -1;
		data->itemList[itemID]._iMagical = -1;
		strcpy(data->itemList[itemID]._iName, "");
		strcpy(data->itemList[itemID]._iIName, "");
		data->itemList[itemID]._iFlags = -1;
		data->itemList[itemID]._iPrePower = -1;
		data->itemList[itemID]._iSufPower = -1;
		data->itemList[itemID]._iPLDam = -1;
		data->itemList[itemID]._iPLToHit = -1;
		data->itemList[itemID]._iPLAC = -1;
		data->itemList[itemID]._iPLStr = -1;
		data->itemList[itemID]._iPLMag = -1;
		data->itemList[itemID]._iPLDex = -1;
		data->itemList[itemID]._iPLVit = -1;
		data->itemList[itemID]._iPLFR = -1;
		data->itemList[itemID]._iPLLR = -1;
		data->itemList[itemID]._iPLMR = -1;
		data->itemList[itemID]._iPLMana = -1;
		data->itemList[itemID]._iPLHP = -1;
		data->itemList[itemID]._iPLDamMod = -1;
		data->itemList[itemID]._iPLGetHit = -1;
		data->itemList[itemID]._iPLLight = -1;
		data->itemList[itemID]._iSplLvlAdd = -1;
		data->itemList[itemID]._iFMinDam = -1;
		data->itemList[itemID]._iFMaxDam = -1;
		data->itemList[itemID]._iLMinDam = -1;
		data->itemList[itemID]._iLMaxDam = -1;
		data->itemList[itemID]._iClass = -1;
		data->itemList[itemID]._ivalue = -1;
		data->itemList[itemID]._iMinDam = -1;
		data->itemList[itemID]._iMaxDam = -1;
		data->itemList[itemID]._iAC = -1;
		data->itemList[itemID]._iMiscId = -1;
		data->itemList[itemID]._iSpell = -1;

		data->itemList[itemID]._iCharges = -1;
		data->itemList[itemID]._iMaxCharges = -1;

		data->itemList[itemID]._iDurability = -1;
		data->itemList[itemID]._iMaxDur = -1;

		data->itemList[itemID]._iMinStr = -1;
		data->itemList[itemID]._iMinMag = -1;
		data->itemList[itemID]._iMinDex = -1;
		data->itemList[itemID]._iStatFlag = -1;
		data->itemList[itemID].IDidx = item->IDidx;
	};

	for (int i = 0; i < devilution::ActiveItemCount; i++) {
		size_t itemID = static_cast<int>(data->itemList.size());
		for (size_t j = 0; j < data->itemList.size(); j++) {
			if (data->itemList[j].compare(devilution::Items[devilution::ActiveItems[i]])) {
				itemID = j;
				break;
			}
		}

		if (devilution::dItem[devilution::Items[devilution::ActiveItems[i]].position.x][devilution::Items[devilution::ActiveItems[i]].position.y] != static_cast<char>(devilution::ActiveItems[i] + 1))
			continue;
		if (itemID == data->itemList.size())
			data->itemList.push_back(ItemData {});
		int dx = devilution::Players[devilution::MyPlayerId].position.tile.x - devilution::Items[devilution::ActiveItems[i]].position.x;
		int dy = devilution::Players[devilution::MyPlayerId].position.tile.y - devilution::Items[devilution::ActiveItems[i]].position.y;
		if (dy > 0) {
			if (dx < 1 && abs(dx) + abs(dy) < 11) {
				partialFillItemInfo(itemID, &devilution::Items[devilution::ActiveItems[i]]);
				data->groundItems.push_back(itemID);
			} else if (dx > 0 && abs(dx) + abs(dy) < 12) {
				partialFillItemInfo(itemID, &devilution::Items[devilution::ActiveItems[i]]);
				data->groundItems.push_back(itemID);
			} else
				emptyFillItemInfo(itemID, &devilution::Items[devilution::ActiveItems[i]]);
		} else {
			if ((dx > -1 || dy == 0) && abs(dx) + abs(dy) < 11) {
				partialFillItemInfo(itemID, &devilution::Items[devilution::ActiveItems[i]]);
				data->groundItems.push_back(itemID);
			} else if ((dx < 0 && dy != 0) && abs(dx) + abs(dy) < 12) {
				partialFillItemInfo(itemID, &devilution::Items[devilution::ActiveItems[i]]);
				data->groundItems.push_back(itemID);
			} else
				emptyFillItemInfo(itemID, &devilution::Items[devilution::ActiveItems[i]]);
		}
	}

	data->storeItems.clear();
	int storeLoopMax = 0;
	devilution::Item *currentItem;
	bool useiValue = false;
	bool shiftValue = false;
	switch (devilution::ActiveStore) {
	case devilution::TalkID::StorytellerIdentify:
	case devilution::TalkID::WitchSell:
	case devilution::TalkID::WitchRecharge:
	case devilution::TalkID::SmithSell:
	case devilution::TalkID::SmithRepair:
		storeLoopMax = devilution::CurrentItemIndex;
		currentItem = &devilution::PlayerItems[0];
		useiValue = true;
		break;
	case devilution::TalkID::WitchBuy:
		storeLoopMax = devilution::NumWitchItemsHf;
		currentItem = &devilution::WitchItems[0];
		break;
	case devilution::TalkID::SmithBuy:
		storeLoopMax = devilution::NumSmithBasicItemsHf;
		currentItem = &devilution::SmithItems[0];
		useiValue = true;
		break;
	case devilution::TalkID::HealerBuy:
		storeLoopMax = devilution::NumHealerItemsHf;
		currentItem = &devilution::HealerItems[0];
		useiValue = true;
		break;
	case devilution::TalkID::SmithPremiumBuy:
		storeLoopMax = devilution::NumSmithItemsHf;
		currentItem = &devilution::PremiumItems[0];
		break;
	case devilution::TalkID::BoyBuy:
		storeLoopMax = 1;
		currentItem = &devilution::BoyItem;
		shiftValue = true;
		break;
	default:
		break;
	}
	for (int i = 0; i < storeLoopMax; i++) {
		if (currentItem->_itype != devilution::ItemType::None) {
			int itemID = static_cast<int>(data->itemList.size());

			for (auto &item : data->itemList) {
				if (item.compare(*currentItem)) {
					itemID = item.ID;
					break;
				}
			}
			if (itemID == static_cast<int>(data->itemList.size())) {
				data->itemList.push_back(ItemData {});
				fullFillItemInfo(itemID, currentItem);
			}
			if (useiValue)
				data->itemList[itemID]._ivalue = currentItem->_ivalue;
			else if (shiftValue)
				data->itemList[itemID]._ivalue = currentItem->_iIvalue + (currentItem->_iIvalue >> 1);
			else
				data->itemList[itemID]._ivalue = currentItem->_iIvalue;
			if (data->itemList[itemID]._ivalue != 0) {
				data->storeItems.push_back(itemID);
				update->add_storeitems(itemID);
			}
		}
		currentItem++;
	}

	if (devilution::currlevel != 0) {
		for (auto &[_, townerData] : data->townerList) {
			strcpy(townerData._tName, "");
			townerData._tx = -1;
			townerData._ty = -1;
		}
	} else {
		for (auto i = 0; devilution::gbIsHellfire ? i < NUM_TOWNERS : i < 12; i++) {
			auto townerID = i;
			auto &towner = data->townerList[townerID];
			towner.ID = static_cast<int>(townerID);
			if (isOnScreen(devilution::Towners[i].position.x, devilution::Towners[i].position.y)) {
				towner._ttype = devilution::Towners[i]._ttype;
				towner._tx = devilution::Towners[i].position.x;
				towner._ty = devilution::Towners[i].position.y;
				// might rework this and just change the type in data.
				if (devilution::Towners[i].name.size() < 31) {
					memcpy(towner._tName, devilution::Towners[i].name.data(), devilution::Towners[i].name.size());
					towner._tName[devilution::Towners[i].name.size()] = '\0';
				}
				// strcpy(towner._tName, devilution::Towners[i].name); old code but with devilution subbed in for reference.
			} else {
				towner._ttype = devilution::Towners[i]._ttype;
				towner._tx = -1;
				towner._ty = -1;
				strcpy(towner._tName, "");
			}
		}
	}

	for (auto &[_, townie] : data->townerList) {
		auto townerData = update->add_townerdata();
		townerData->set_id(townie.ID);
		if (townie._tx != -1)
			townerData->set__ttype(static_cast<int>(townie._ttype));
		else
			townerData->set__ttype(-1);
		townerData->set__tx(townie._tx);
		townerData->set__ty(townie._ty);
		townerData->set__tname(townie._tName);
	}

	for (auto &itemID : itemsModified)
	// for (auto& item : data->itemList)
	{
		auto &item = data->itemList[itemID];
		auto itemData = update->add_itemdata();
		itemData->set_id(item.ID);
		itemData->set__itype(item._itype);
		itemData->set__ix(item._ix);
		itemData->set__iy(item._iy);
		itemData->set__iidentified(item._iIdentified);
		itemData->set__imagical(item._iMagical);
		itemData->set__iname(item._iName);
		itemData->set__iiname(item._iIName);
		itemData->set__iclass(item._iClass);
		itemData->set__icurs(item._iCurs);
		itemData->set__ivalue(item._ivalue);
		itemData->set__imindam(item._iMinDam);
		itemData->set__imaxdam(item._iMaxDam);
		itemData->set__iac(item._iAC);
		itemData->set__iflags(item._iFlags);
		itemData->set__imiscid(item._iMiscId);
		itemData->set__ispell(item._iSpell);
		itemData->set__icharges(item._iCharges);
		itemData->set__imaxcharges(item._iMaxCharges);
		itemData->set__idurability(item._iDurability);
		itemData->set__imaxdur(item._iMaxDur);
		itemData->set__ipldam(item._iPLDam);
		itemData->set__ipltohit(item._iPLToHit);
		itemData->set__iplac(item._iPLAC);
		itemData->set__iplstr(item._iPLStr);
		itemData->set__iplmag(item._iPLMag);
		itemData->set__ipldex(item._iPLDex);
		itemData->set__iplvit(item._iPLVit);
		itemData->set__iplfr(item._iPLFR);
		itemData->set__ipllr(item._iPLLR);
		itemData->set__iplmr(item._iPLMR);
		itemData->set__iplmana(item._iPLMana);
		itemData->set__iplhp(item._iPLHP);
		itemData->set__ipldammod(item._iPLDamMod);
		itemData->set__iplgethit(item._iPLGetHit);
		itemData->set__ipllight(item._iPLLight);
		itemData->set__ispllvladd(item._iSplLvlAdd);
		itemData->set__ifmindam(item._iFMinDam);
		itemData->set__ifmaxdam(item._iFMaxDam);
		itemData->set__ilmindam(item._iLMinDam);
		itemData->set__ilmaxdam(item._iLMaxDam);
		itemData->set__iprepower(item._iPrePower);
		itemData->set__isufpower(item._iSufPower);
		itemData->set__iminstr(item._iMinStr);
		itemData->set__iminmag(item._iMinMag);
		itemData->set__imindex(item._iMinDex);
		itemData->set__istatflag(item._iStatFlag);
		itemData->set_ididx(item.IDidx);
	}
	for (auto &itemID : data->groundItems)
		update->add_grounditemid(itemID);

	for (size_t i = 0; i < devilution::ActiveMonsterCount; i++) {
		if (isOnScreen(devilution::Monsters[devilution::ActiveMonsters[i]].position.tile.x, devilution::Monsters[devilution::ActiveMonsters[i]].position.tile.y) && devilution::HasAnyOf(devilution::dFlags[devilution::Monsters[devilution::ActiveMonsters[i]].position.tile.x][devilution::Monsters[devilution::ActiveMonsters[i]].position.tile.y], devilution::DungeonFlag::Lit) && !(devilution::Monsters[devilution::ActiveMonsters[i]].flags & 0x01)) {
			auto m = update->add_monsterdata();
			m->set_index(devilution::ActiveMonsters[i]);
			m->set_x(devilution::Monsters[devilution::ActiveMonsters[i]].position.tile.x);
			m->set_y(devilution::Monsters[devilution::ActiveMonsters[i]].position.tile.y);
			m->set_futx(devilution::Monsters[devilution::ActiveMonsters[i]].position.future.x);
			m->set_futy(devilution::Monsters[devilution::ActiveMonsters[i]].position.future.y);
			m->set_type(devilution::Monsters[devilution::ActiveMonsters[i]].type().type);
			std::string monsterName = std::string(devilution::Monsters[devilution::ActiveMonsters[i]].name());
			m->set_name(monsterName.c_str());
			m->set_mode(static_cast<int>(devilution::Monsters[devilution::ActiveMonsters[i]].mode));
			m->set_unique(devilution::Monsters[devilution::ActiveMonsters[i]].isUnique());
		}
	}

	auto fillObject = [&](int index, devilution::Object &ob) {
		auto o = update->add_objectdata();
		o->set_type(ob._otype);
		o->set_x(ob.position.x);
		o->set_y(ob.position.y);
		o->set_shrinetype(-1);
		o->set_solid(ob._oSolidFlag);
		o->set_doorstate(-1);
		o->set_selectable(ob.selectionRegion != devilution::SelectionRegion::None ? true : false);
		o->set_index(index);
		switch (static_cast<devilution::_object_id>(ob._otype)) {
		case devilution::_object_id::OBJ_BARRELEX:
			o->set_type(static_cast<int>(devilution::_object_id::OBJ_BARREL));
			break;
		case devilution::_object_id::OBJ_SHRINEL:
		case devilution::_object_id::OBJ_SHRINER:
			if (ob.selectionRegion != devilution::SelectionRegion::None)
				o->set_shrinetype(ob._oVar1);
			break;
		case devilution::_object_id::OBJ_L1LDOOR:
		case devilution::_object_id::OBJ_L1RDOOR:
		case devilution::_object_id::OBJ_L2LDOOR:
		case devilution::_object_id::OBJ_L2RDOOR:
		case devilution::_object_id::OBJ_L3LDOOR:
		case devilution::_object_id::OBJ_L3RDOOR:
			o->set_doorstate(ob._oVar4);
			break;
		default:
			break;
		}
		if (devilution::Players[devilution::MyPlayerId]._pClass == devilution::HeroClass::Rogue)
			o->set_trapped(ob._oTrapFlag);
		else
			o->set_trapped(false);
	};

	auto fillMissile = [&](devilution::Missile &ms) {
		auto m = update->add_missiledata();
		m->set_type(static_cast<int>(ms._mitype));
		m->set_x(ms.position.tile.x);
		m->set_y(ms.position.tile.y);
		m->set_xvel(ms.position.velocity.deltaX);
		m->set_yvel(ms.position.velocity.deltaY);
		switch (ms.sourceType()) {
		case devilution::MissileSource::Monster:
			if (isOnScreen(ms.sourceMonster()->position.tile.x, ms.sourceMonster()->position.tile.y)) {
				m->set_sx(ms.sourceMonster()->position.tile.x);
				m->set_sy(ms.sourceMonster()->position.tile.y);
			} else {
				m->set_sx(-1);
				m->set_sy(-1);
			}
			break;
		case devilution::MissileSource::Player:
			if (isOnScreen(ms.sourcePlayer()->position.tile.x, ms.sourcePlayer()->position.tile.y)) {
				m->set_sx(ms.sourcePlayer()->position.tile.x);
				m->set_sy(ms.sourcePlayer()->position.tile.y);
			} else {
				m->set_sx(-1);
				m->set_sy(-1);
			}
			break;
		default:
			m->set_sx(-1);
			m->set_sy(-1);
		}
	};

	if (devilution::Players[devilution::MyPlayerId].plrlevel != 0) {
		for (int i = 0; i < devilution::ActiveObjectCount; i++) {
			if (isOnScreen(devilution::Objects[devilution::ActiveObjects[i]].position.x, devilution::Objects[devilution::ActiveObjects[i]].position.y) && devilution::dObject[devilution::Objects[devilution::ActiveObjects[i]].position.x][devilution::Objects[devilution::ActiveObjects[i]].position.y] == devilution::ActiveObjects[i] + 1) {
				fillObject(devilution::ActiveObjects[i], devilution::Objects[devilution::ActiveObjects[i]]);
			}
		}

		for (auto &missile : devilution::Missiles) {
			if (isOnScreen(missile.position.tile.x, missile.position.tile.y))
				fillMissile(missile);
		}
	}

	for (int i = 0; i < MAXPORTAL; i++) {
		if (devilution::Portals[i].open && (devilution::Portals[i].level == devilution::Players[devilution::MyPlayerId].plrlevel) && isOnScreen(devilution::Portals[i].position.x, devilution::Portals[i].position.y)) {
			auto tp = update->add_portaldata();
			tp->set_x(devilution::Portals[i].position.x);
			tp->set_y(devilution::Portals[i].position.y);
			tp->set_player(i);
		} else if (devilution::Portals[i].open && 0 == devilution::Players[devilution::MyPlayerId].plrlevel && isOnScreen(devilution::PortalTownPosition[i].x, devilution::PortalTownPosition[i].y)) {
			auto tp = update->add_portaldata();
			tp->set_x(devilution::PortalTownPosition[i].x);
			tp->set_y(devilution::PortalTownPosition[i].y);
			tp->set_player(i);
		}
	}

	protoClient.queueMessage(std::move(message));
}

bool Server::isOnScreen(int x, int y)
{
	bool returnValue = false;
	int dx = data->playerList[devilution::MyPlayerId]._px - x;
	int dy = data->playerList[devilution::MyPlayerId]._py - y;
	if (!devilution::CharFlag) {
		if (dy > 0) {
			if (dx < 1 && abs(dx) + abs(dy) < 11)
				returnValue = true;
			else if (dx > 0 && abs(dx) + abs(dy) < 12)
				returnValue = true;
		} else {
			if ((dx > -1 || dy == 0) && abs(dx) + abs(dy) < 11)
				returnValue = true;
			else if ((dx < 0 && dy != 0) && abs(dx) + abs(dy) < 12)
				returnValue = true;
		}
	} else if (devilution::CharFlag) {
		returnValue = panelScreenCheck[std::make_pair(dx, dy)];
	}
	return returnValue;
}

bool Server::OKToAct()
{
	return devilution::ActiveStore == devilution::TalkID::None && devilution::pcurs == 1 && !data->qtextflag;
}

void Server::move(int x, int y)
{
	devilution::NetSendCmdLoc(devilution::MyPlayerId, true, devilution::_cmd_id::CMD_WALKXY, devilution::Point { x, y });
}

void Server::talk(int x, int y)
{
	int index;
	for (index = 0; index < NUM_TOWNERS; index++) {
		if (devilution::Towners[index].position.x == x && devilution::Towners[index].position.y == y)
			break;
	}
	if (index != NUM_TOWNERS)
		devilution::NetSendCmdLocParam1(true, devilution::_cmd_id::CMD_TALKXY, devilution::Point { x, y }, index);
}

void Server::selectStoreOption(StoreOption option)
{
	// Need to check for qtextflag in DevilutionX, for some reason we can access stores when
	// the shop keeper is giving us quest text. Doesn't happen in 1.09.
	if (devilution::qtextflag)
		return;

	switch (option) {
	case StoreOption::TALK:
		if (devilution::ActiveStore == devilution::TalkID::Witch) {
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::OldTextLine = 12;
			devilution::TownerId = devilution::_talker_id::TOWN_WITCH;
			devilution::OldActiveStore = devilution::TalkID::Witch;
			devilution::StartStore(devilution::TalkID::Gossip);
		}
		break;
	case StoreOption::IDENTIFYANITEM:
		if (devilution::ActiveStore == devilution::TalkID::Storyteller) {
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::StorytellerIdentify);
		}
		break;
	case StoreOption::EXIT:
		switch (devilution::ActiveStore) {
		case devilution::TalkID::Barmaid:
		case devilution::TalkID::Boy:
		case devilution::TalkID::BoyBuy:
		case devilution::TalkID::Drunk:
		case devilution::TalkID::Healer:
		case devilution::TalkID::Smith:
		case devilution::TalkID::Storyteller:
		case devilution::TalkID::Tavern:
		case devilution::TalkID::Witch:
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::ActiveStore = devilution::TalkID::None;
		default:
			break;
		}
		break;
	case StoreOption::BUYITEMS:
		switch (devilution::ActiveStore) {
		case devilution::TalkID::Witch:
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::WitchBuy);
			break;
		case devilution::TalkID::Healer:
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::HealerBuy);
			break;
		default:
			break;
		}
		break;
	case StoreOption::BUYBASIC:
		if (devilution::ActiveStore == devilution::TalkID::Smith) {
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::SmithBuy);
		}
		break;
	case StoreOption::BUYPREMIUM:
		if (devilution::ActiveStore == devilution::TalkID::Smith) {
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::SmithPremiumBuy);
		}
		break;
	case StoreOption::SELL:
		switch (devilution::ActiveStore) {
		case devilution::TalkID::Smith:
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::SmithSell);
			break;
		case devilution::TalkID::Witch:
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::WitchSell);
			break;
		default:
			break;
		}
		break;
	case StoreOption::REPAIR:
		if (devilution::ActiveStore == devilution::TalkID::Smith) {
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::SmithRepair);
		}
		break;
	case StoreOption::RECHARGE:
		if (devilution::ActiveStore == devilution::TalkID::Witch) {
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::WitchRecharge);
		}
		break;
	case StoreOption::WIRTPEEK:
		if (devilution::ActiveStore == devilution::TalkID::Boy) {
			if (50 <= devilution::Players[devilution::MyPlayerId]._pGold) {
				devilution::TakePlrsMoney(50);
				devilution::PlaySFX(devilution::SfxID::MenuSelect);
				devilution::StartStore(devilution::TalkID::BoyBuy);
			} else {
				devilution::OldActiveStore = devilution::TalkID::Boy;
				devilution::OldTextLine = 18;
				devilution::OldScrollPos = devilution::ScrollPos;
				devilution::StartStore(devilution::TalkID::NoMoney);
			}
		}
		break;
	case StoreOption::BACK:
		switch (devilution::ActiveStore) {
		case devilution::TalkID::SmithRepair:
		case devilution::TalkID::SmithSell:
		case devilution::TalkID::SmithBuy:
		case devilution::TalkID::SmithPremiumBuy:
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::Smith);
			break;
		case devilution::TalkID::WitchBuy:
		case devilution::TalkID::WitchSell:
		case devilution::TalkID::WitchRecharge:
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::Witch);
			break;
		case devilution::TalkID::HealerBuy:
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::Healer);
			break;
		case devilution::TalkID::StorytellerIdentify:
			devilution::PlaySFX(devilution::SfxID::MenuSelect);
			devilution::StartStore(devilution::TalkID::Storyteller);
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

void Server::buyItem(int itemID)
{
	int idx;

	if (devilution::ActiveStore == devilution::TalkID::WitchBuy) {

		idx = -1;

		for (int i = 0; i < 20; i++) {
			if (data->itemList[itemID].compare(devilution::WitchItems[i])) {
				idx = i;
				break;
			}
		}

		if (idx == -1)
			return;

		devilution::PlaySFX(devilution::SfxID::MenuSelect);

		devilution::OldTextLine = devilution::CurrentTextLine;
		devilution::OldScrollPos = devilution::ScrollPos;
		devilution::OldActiveStore = devilution::ActiveStore;

		if (!devilution::PlayerCanAfford(devilution::WitchItems[idx]._iIvalue)) {
			devilution::StartStore(devilution::TalkID::NoMoney);
			return;
		} else if (devilution::StoreAutoPlace(devilution::WitchItems[idx], true)) {
			if (idx < 3)
				devilution::WitchItems[idx]._iSeed = devilution::AdvanceRndSeed();

			devilution::TakePlrsMoney(devilution::WitchItems[idx]._iIvalue);

			if (idx >= 3) {
				if (idx == devilution::NumWitchItemsHf - 1)
					devilution::WitchItems[devilution::NumWitchItemsHf - 1].clear();
				else {
					for (; !devilution::WitchItems[idx + 1].isEmpty(); idx++) {
						devilution::WitchItems[idx] = std::move(devilution::WitchItems[idx + 1]);
					}
					devilution::WitchItems[idx].clear();
				}
			}

			devilution::CalcPlrInv(*devilution::MyPlayer, true);
		} else {
			devilution::StartStore(devilution::TalkID::NoRoom);
			return;
		}
		devilution::StartStore(devilution::OldActiveStore);
	} else if (devilution::ActiveStore == devilution::TalkID::SmithBuy) {
		idx = -1;

		for (int i = 0; i < 20; i++) {
			if (data->itemList[itemID].compare(devilution::SmithItems[i])) {
				idx = i;
				break;
			}
		}

		if (idx == -1)
			return;

		devilution::PlaySFX(devilution::SfxID::MenuSelect);

		devilution::OldTextLine = devilution::CurrentTextLine;
		devilution::OldScrollPos = devilution::ScrollPos;
		devilution::OldActiveStore = devilution::TalkID::SmithBuy;

		if (!devilution::PlayerCanAfford(devilution::SmithItems[idx]._iIvalue)) {
			devilution::StartStore(devilution::TalkID::NoMoney);
			return;
		} else if (!devilution::StoreAutoPlace(devilution::SmithItems[idx], false)) {
			devilution::StartStore(devilution::TalkID::NoRoom);
		} else {
			devilution::TakePlrsMoney(devilution::SmithItems[idx]._iIvalue);
			if (devilution::SmithItems[idx]._iMagical == devilution::item_quality::ITEM_QUALITY_NORMAL)
				devilution::SmithItems[idx]._iIdentified = false;
			devilution::StoreAutoPlace(devilution::SmithItems[idx], true);
			if (idx == devilution::NumSmithBasicItemsHf - 1) {
				devilution::SmithItems[devilution::NumSmithBasicItemsHf - 1].clear();
			} else {
				for (; !devilution::SmithItems[idx + 1].isEmpty(); idx++) {
					devilution::SmithItems[idx] = std::move(devilution::SmithItems[idx + 1]);
				}
				devilution::SmithItems[idx].clear();
			}
			devilution::CalcPlrInv(*devilution::MyPlayer, true);
			devilution::StartStore(devilution::OldActiveStore);
		}
	} else if (devilution::ActiveStore == devilution::TalkID::SmithPremiumBuy) {
		int idx = -1;
		for (int i = 0; i < 20; i++) {
			if (data->itemList[itemID].compare(devilution::PremiumItems[i])) {
				idx = i;
				break;
			}
		}

		if (idx == -1)
			return;

		devilution::PlaySFX(devilution::SfxID::MenuSelect);

		devilution::OldTextLine = devilution::CurrentTextLine;
		devilution::OldScrollPos = devilution::ScrollPos;
		devilution::OldActiveStore = devilution::TalkID::SmithBuy;

		if (!devilution::PlayerCanAfford(devilution::PremiumItems[idx]._iIvalue)) {
			devilution::StartStore(devilution::TalkID::NoMoney);
			return;
		} else if (!devilution::StoreAutoPlace(devilution::PremiumItems[idx], false)) {
			devilution::StartStore(devilution::TalkID::NoRoom);
			return;
		} else {
			devilution::TakePlrsMoney(devilution::PremiumItems[idx]._iIvalue);
			if (devilution::PremiumItems[idx]._iMagical == devilution::item_quality::ITEM_QUALITY_NORMAL)
				devilution::PremiumItems[idx]._iIdentified = false;
			devilution::StoreAutoPlace(devilution::PremiumItems[idx], true);
			if (idx == devilution::NumSmithBasicItemsHf - 1) {
				devilution::PremiumItems[devilution::NumSmithBasicItemsHf - 1].clear();
			} else {
				for (; !devilution::PremiumItems[idx + 1].isEmpty(); idx++) {
					devilution::PremiumItems[idx] = std::move(devilution::PremiumItems[idx + 1]);
				}
				devilution::PremiumItems[idx].clear();
			}
			devilution::CalcPlrInv(*devilution::MyPlayer, true);
			devilution::StartStore(devilution::OldActiveStore);
		}
	} else if (devilution::ActiveStore == devilution::TalkID::HealerBuy) {
		idx = -1;

		for (int i = 0; i < 20; i++) {
			if (data->itemList[itemID].compare(devilution::HealerItems[i])) {
				idx = i;
				break;
			}
		}

		if (idx == -1)
			return;

		devilution::PlaySFX(devilution::SfxID::MenuSelect);

		devilution::OldTextLine = devilution::CurrentTextLine;
		devilution::OldScrollPos = devilution::ScrollPos;
		devilution::OldActiveStore = devilution::TalkID::HealerBuy;

		if (!devilution::PlayerCanAfford(devilution::HealerItems[idx]._iIvalue)) {
			devilution::StartStore(devilution::TalkID::NoMoney);
			return;
		} else if (!devilution::StoreAutoPlace(devilution::HealerItems[idx], false)) {
			devilution::StartStore(devilution::TalkID::NoRoom);
			return;
		} else {
			if (!devilution::gbIsMultiplayer) {
				if (idx < 2)
					devilution::HealerItems[idx]._iSeed = devilution::AdvanceRndSeed();
			} else {
				if (idx < 3)
					devilution::HealerItems[idx]._iSeed = devilution::AdvanceRndSeed();
			}

			devilution::TakePlrsMoney(devilution::HealerItems[idx]._iIvalue);
			if (devilution::HealerItems[idx]._iMagical == devilution::item_quality::ITEM_QUALITY_NORMAL)
				devilution::HealerItems[idx]._iIdentified = false;
			devilution::StoreAutoPlace(devilution::HealerItems[idx], true);

			if (!devilution::gbIsMultiplayer) {
				if (idx < 2)
					return;
			} else {
				if (idx < 3)
					return;
			}
		}
		if (idx == 19) {
			devilution::HealerItems[19].clear();
		} else {
			for (; !devilution::HealerItems[idx + 1].isEmpty(); idx++) {
				devilution::HealerItems[idx] = std::move(devilution::HealerItems[idx + 1]);
			}
			devilution::HealerItems[idx].clear();
		}
		CalcPlrInv(*devilution::MyPlayer, true);
		devilution::StartStore(devilution::OldActiveStore);
	} else if (devilution::ActiveStore == devilution::TalkID::BoyBuy) {
		devilution::PlaySFX(devilution::SfxID::MenuSelect);

		devilution::OldActiveStore = devilution::TalkID::BoyBuy;
		devilution::OldScrollPos = devilution::ScrollPos;
		devilution::OldTextLine = 10;

		int price = devilution::BoyItem._iIvalue;
		if (devilution::gbIsHellfire)
			price -= devilution::BoyItem._iIvalue / 4;
		else
			price += devilution::BoyItem._iIvalue / 2;

		if (!devilution::PlayerCanAfford(price)) {
			devilution::StartStore(devilution::TalkID::NoMoney);
			return;
		} else if (!devilution::StoreAutoPlace(devilution::BoyItem, false)) {
			devilution::StartStore(devilution::TalkID::NoRoom);
			return;
		} else {
			devilution::TakePlrsMoney(price);
			devilution::StoreAutoPlace(devilution::BoyItem, true);
			devilution::BoyItem.clear();
			devilution::OldActiveStore = devilution::TalkID::Boy;
			devilution::OldTextLine = 12;
		}
		devilution::StartStore(devilution::OldActiveStore);
	}
}

void Server::sellItem(int itemID)
{
	int idx;

	if (devilution::ActiveStore != devilution::TalkID::WitchSell && devilution::ActiveStore != devilution::TalkID::SmithSell)
		return;

	idx = -1;

	for (int i = 0; i < 48; i++) {
		if (data->itemList[itemID].compare(devilution::PlayerItems[i])) {
			idx = i;
			break;
		}
	}

	if (idx == -1)
		return;

	devilution::PlaySFX(devilution::SfxID::MenuSelect);

	devilution::OldTextLine = devilution::CurrentTextLine;
	devilution::OldActiveStore = devilution::ActiveStore;
	devilution::OldScrollPos = devilution::ScrollPos;

	if (!devilution::StoreGoldFit(devilution::PlayerItems[idx])) {
		devilution::StartStore(devilution::TalkID::NoRoom);
		return;
	}

	devilution::Player &myPlayer = *devilution::MyPlayer;

	if (devilution::PlayerItemIndexes[idx] >= 0)
		myPlayer.RemoveInvItem(devilution::PlayerItemIndexes[idx]);
	else
		myPlayer.RemoveSpdBarItem(-(devilution::PlayerItemIndexes[idx] + 1));

	int cost = devilution::PlayerItems[idx]._iIvalue;
	devilution::CurrentItemIndex--;
	if (idx != devilution::CurrentItemIndex) {
		while (idx < devilution::CurrentItemIndex) {
			devilution::PlayerItems[idx] = devilution::PlayerItems[idx + 1];
			devilution::PlayerItemIndexes[idx] = devilution::PlayerItemIndexes[idx + 1];
			idx++;
		}
	}

	devilution::AddGoldToInventory(myPlayer, cost);

	myPlayer._pGold += cost;
	devilution::StartStore(devilution::OldActiveStore);
}

void Server::rechargeItem(int itemID)
{
	int idx;

	if (devilution::ActiveStore != devilution::TalkID::WitchRecharge)
		return;

	idx = -1;

	for (int i = 0; i < 20; i++) {
		if (data->itemList[itemID].compare(devilution::PlayerItems[i])) {
			idx = i;
			break;
		}
	}

	if (idx == -1)
		return;

	devilution::PlaySFX(devilution::SfxID::MenuSelect);

	devilution::OldActiveStore = devilution::TalkID::WitchRecharge;
	devilution::OldTextLine = devilution::CurrentTextLine;
	devilution::OldScrollPos = devilution::ScrollPos;

	int price = devilution::PlayerItems[idx]._iIvalue;

	if (!devilution::PlayerCanAfford(price)) {
		devilution::StartStore(devilution::TalkID::NoMoney);
		return;
	} else {
		devilution::PlayerItems[idx]._iCharges = devilution::PlayerItems[idx]._iMaxCharges;

		devilution::Player &myPlayer = *devilution::MyPlayer;

		int8_t i = devilution::PlayerItemIndexes[idx];
		if (i < 0) {
			myPlayer.InvBody[devilution::inv_body_loc::INVLOC_HAND_LEFT]._iCharges = myPlayer.InvBody[devilution::inv_body_loc::INVLOC_HAND_LEFT]._iMaxCharges;
			devilution::NetSendCmdChItem(true, devilution::inv_body_loc::INVLOC_HAND_LEFT);
		} else {
			myPlayer.InvList[i]._iCharges = myPlayer.InvList[i]._iMaxCharges;
			devilution::NetSyncInvItem(myPlayer, i);
		}

		devilution::TakePlrsMoney(price);
		devilution::CalcPlrInv(myPlayer, true);
		devilution::StartStore(devilution::OldActiveStore);
	}
}

void Server::repairItem(int itemID)
{
	int idx;

	if (devilution::ActiveStore != devilution::TalkID::SmithRepair)
		return;

	idx = -1;

	for (int i = 0; i < 20; i++) {
		if (data->itemList[itemID].compare(devilution::PlayerItems[i])) {
			idx = i;
			break;
		}
	}

	if (idx == -1)
		return;

	devilution::PlaySFX(devilution::SfxID::MenuSelect);

	devilution::OldActiveStore = devilution::TalkID::SmithRepair;
	devilution::OldTextLine = devilution::CurrentTextLine;
	devilution::OldScrollPos = devilution::ScrollPos;

	int price = devilution::PlayerItems[idx]._iIvalue;

	if (!devilution::PlayerCanAfford(price)) {
		devilution::StartStore(devilution::TalkID::NoMoney);
		return;
	}

	devilution::PlayerItems[idx]._iDurability = devilution::PlayerItems[idx]._iMaxDur;

	int8_t i = devilution::PlayerItemIndexes[idx];

	devilution::Player &myPlayer = *devilution::MyPlayer;

	if (i < 0) {
		if (i == -1)
			myPlayer.InvBody[devilution::inv_body_loc::INVLOC_HEAD]._iDurability = myPlayer.InvBody[devilution::inv_body_loc::INVLOC_HEAD]._iMaxDur;
		if (i == -2)
			myPlayer.InvBody[devilution::inv_body_loc::INVLOC_CHEST]._iDurability = myPlayer.InvBody[devilution::inv_body_loc::INVLOC_CHEST]._iMaxDur;
		if (i == -3)
			myPlayer.InvBody[devilution::inv_body_loc::INVLOC_HAND_LEFT]._iDurability = myPlayer.InvBody[devilution::inv_body_loc::INVLOC_HAND_LEFT]._iMaxDur;
		if (i == -4)
			myPlayer.InvBody[devilution::inv_body_loc::INVLOC_HAND_RIGHT]._iDurability = myPlayer.InvBody[devilution::inv_body_loc::INVLOC_HAND_RIGHT]._iMaxDur;
		devilution::TakePlrsMoney(price);
		devilution::StartStore(devilution::OldActiveStore);
		return;
	}

	myPlayer.InvList[i]._iDurability = myPlayer.InvList[i]._iMaxDur;
	devilution::TakePlrsMoney(price);
	devilution::StartStore(devilution::OldActiveStore);
}

void Server::attackMonster(int index)
{
	if (index < 0 || 199 < index)
		return;

	if (!OKToAct())
		return;

	if (!isOnScreen(devilution::Monsters[index].position.tile.x, devilution::Monsters[index].position.tile.y))
		return;

	if (devilution::M_Talker(devilution::Monsters[index]))
		devilution::NetSendCmdParam1(true, devilution::_cmd_id::CMD_ATTACKID, index);
	else if (devilution::Players[devilution::MyPlayerId].InvBody[devilution::inv_body_loc::INVLOC_HAND_LEFT]._itype == devilution::ItemType::Bow || devilution::Players[devilution::MyPlayerId].InvBody[devilution::inv_body_loc::INVLOC_HAND_RIGHT]._itype == devilution::ItemType::Bow)
		devilution::NetSendCmdParam1(true, devilution::_cmd_id::CMD_RATTACKID, index);
	else
		devilution::NetSendCmdParam1(true, devilution::_cmd_id::CMD_ATTACKID, index);
}

void Server::attackXY(int x, int y)
{
	if (!isOnScreen(x, y))
		return;

	if (!OKToAct())
		return;

	if (devilution::Players[devilution::MyPlayerId].InvBody[devilution::inv_body_loc::INVLOC_HAND_LEFT]._itype == devilution::ItemType::Bow || devilution::Players[devilution::MyPlayerId].InvBody[devilution::inv_body_loc::INVLOC_HAND_RIGHT]._itype == devilution::ItemType::Bow)
		devilution::NetSendCmdLoc(devilution::MyPlayerId, true, devilution::_cmd_id::CMD_RATTACKXY, devilution::Point { x, y });
	else
		devilution::NetSendCmdLoc(devilution::MyPlayerId, true, devilution::_cmd_id::CMD_SATTACKXY, devilution::Point { x, y });
}

void Server::operateObject(int index)
{
	if (index < 0 || 126 < index)
		return;

	if (!isOnScreen(devilution::Objects[index].position.x, devilution::Objects[index].position.y))
		return;

	if (!OKToAct())
		return;

	bool found = false;
	for (int i = 0; i < devilution::ActiveObjectCount; i++) {
		if (devilution::ActiveObjects[i] == index) {
			found = true;
			break;
		}
	}

	if (!found)
		return;

	devilution::NetSendCmdLoc(devilution::MyPlayerId, true, devilution::_cmd_id::CMD_OPOBJXY, devilution::Point { devilution::Objects[index].position.x, devilution::Objects[index].position.y });
}

void Server::useBeltItem(int slot)
{
	if (slot < 0 || 7 < slot)
		return;

	if (devilution::Players[devilution::MyPlayerId].SpdList[slot]._itype == devilution::ItemType::None)
		return;

	int cii = slot + 47;
	devilution::UseInvItem(cii);
}

void Server::toggleCharacterScreen()
{
	if (!OKToAct())
		return;

	devilution::CharFlag = !devilution::CharFlag;
}

void Server::increaseStat(CommandType commandType)
{
	int maxValue = 0;

	if (devilution::Players[devilution::MyPlayerId]._pStatPts == 0)
		return;

	switch (commandType) {
	case CommandType::ADDSTR:
		switch (static_cast<devilution::HeroClass>(devilution::Players[devilution::MyPlayerId]._pClass)) {
		case devilution::HeroClass::Warrior:
			maxValue = 250;
			break;
		case devilution::HeroClass::Rogue:
			maxValue = 55;
			break;
		case devilution::HeroClass::Sorcerer:
			maxValue = 45;
			break;
		default:
			break;
		}
		if (devilution::Players[devilution::MyPlayerId]._pBaseStr < maxValue) {
			devilution::NetSendCmdParam1(true, devilution::_cmd_id::CMD_ADDSTR, 1);
			devilution::Players[devilution::MyPlayerId]._pStatPts -= 1;
		}
		break;
	case CommandType::ADDMAG:
		switch (static_cast<devilution::HeroClass>(devilution::Players[devilution::MyPlayerId]._pClass)) {
		case devilution::HeroClass::Warrior:
			maxValue = 50;
			break;
		case devilution::HeroClass::Rogue:
			maxValue = 70;
			break;
		case devilution::HeroClass::Sorcerer:
			maxValue = 250;
			break;
		default:
			break;
		}
		if (devilution::Players[devilution::MyPlayerId]._pBaseMag < maxValue) {
			devilution::NetSendCmdParam1(true, devilution::_cmd_id::CMD_ADDMAG, 1);
			devilution::Players[devilution::MyPlayerId]._pStatPts -= 1;
		}
		break;
	case CommandType::ADDDEX:
		switch (static_cast<devilution::HeroClass>(devilution::Players[devilution::MyPlayerId]._pClass)) {
		case devilution::HeroClass::Warrior:
			maxValue = 60;
			break;
		case devilution::HeroClass::Rogue:
			maxValue = 250;
			break;
		case devilution::HeroClass::Sorcerer:
			maxValue = 85;
			break;
		default:
			break;
		}
		if (devilution::Players[devilution::MyPlayerId]._pBaseDex < maxValue) {
			devilution::NetSendCmdParam1(true, devilution::_cmd_id::CMD_ADDDEX, 1);
			devilution::Players[devilution::MyPlayerId]._pStatPts -= 1;
		}
		break;
	case CommandType::ADDVIT:
		switch (static_cast<devilution::HeroClass>(devilution::Players[devilution::MyPlayerId]._pClass)) {
		case devilution::HeroClass::Warrior:
			maxValue = 100;
			break;
		default:
			maxValue = 80;
			break;
		}
		if (devilution::Players[devilution::MyPlayerId]._pBaseVit < maxValue) {
			devilution::NetSendCmdParam1(true, devilution::_cmd_id::CMD_ADDVIT, 1);
			devilution::Players[devilution::MyPlayerId]._pStatPts -= 1;
		}
		break;
	default:
		break;
	}
}

void Server::getItem(int itemID)
{
	if (!OKToAct())
		return;

	bool found = false;

	for (size_t i = 0; i < data->groundItems.size(); i++) {
		if (data->groundItems[i] == itemID)
			found = true;
		if (found)
			break;
	}

	if (!found)
		return;

	auto itemData = data->itemList[itemID];

	if (!isOnScreen(itemData._ix, itemData._iy))
		return;

	int index = -1;
	for (int i = 0; i < devilution::ActiveItemCount; i++) {
		if (itemData.compare(devilution::Items[devilution::ActiveItems[i]]))
			index = devilution::ActiveItems[i];

		if (index != -1)
			break;
	}

	if (index == -1)
		return;

	if (devilution::invflag)
		devilution::NetSendCmdLocParam1(true, devilution::_cmd_id::CMD_GOTOGETITEM, devilution::Point { itemData._ix, itemData._iy }, index);
	else
		devilution::NetSendCmdLocParam1(true, devilution::_cmd_id::CMD_GOTOAGETITEM, devilution::Point { itemData._ix, itemData._iy }, index);
}

void Server::setSpell(int spellID, devilution::SpellType spellType)
{
	if (spellID == -1)
		return;

	switch (spellType) {
	case devilution::SpellType::Skill:
		if (!(devilution::Players[devilution::MyPlayerId]._pAblSpells & (1 << (spellID - 1))))
			return;
		break;
	case devilution::SpellType::Spell:
		if (!(devilution::Players[devilution::MyPlayerId]._pMemSpells & (1 << (spellID - 1))))
			return;
		break;
	case devilution::SpellType::Scroll:
		if (!(devilution::Players[devilution::MyPlayerId]._pScrlSpells & (1 << (spellID - 1))))
			return;
		break;
	case devilution::SpellType::Charges:
		if ((devilution::Players[devilution::MyPlayerId].InvBody[4]._iSpell != static_cast<devilution::SpellID>(spellID) && devilution::Players[devilution::MyPlayerId].InvBody[5]._iSpell != static_cast<devilution::SpellID>(spellID)) || (devilution::Players[devilution::MyPlayerId].InvBody[4]._iCharges == 0 && devilution::Players[devilution::MyPlayerId].InvBody[5]._iCharges == 0))
			return;
		break;
	case devilution::SpellType::Invalid:
	default:
		return;
		break;
	}

	devilution::Players[devilution::MyPlayerId]._pRSpell = static_cast<devilution::SpellID>(spellID);
	devilution::Players[devilution::MyPlayerId]._pRSplType = static_cast<devilution::SpellType>(spellType);
	//*force_redraw = 255; // TODO: Is this line needed in devilutionX? If so, what is it?
}

void Server::castSpell(int index)
{
	if (!OKToAct())
		return;

	devilution::pcursmonst = index;
	devilution::PlayerUnderCursor = nullptr;
	devilution::cursPosition = devilution::Point { devilution::Monsters[index].position.tile.x, devilution::Monsters[index].position.tile.y };

	devilution::CheckPlrSpell(false);
}

void Server::toggleInventory()
{
	if (!OKToAct())
		return;

	devilution::invflag = !devilution::invflag;
}

void Server::putInCursor(size_t itemID)
{
	if (!OKToAct())
		return;

	if (data->itemList.size() <= itemID)
		return;

	auto &item = data->itemList[itemID];
	int mx, my;

	mx = 0;
	my = 0;
	for (int i = 0; i < 55; i++) {
		if (i < 7) {
			if (item.compare(devilution::Players[devilution::MyPlayerId].InvBody[i]) && devilution::Players[devilution::MyPlayerId].InvBody[i]._itype != devilution::ItemType::None) {
				if (!devilution::invflag)
					return;

				// Switch statement is left here because left and right ring are reversed in DevilutionX
				switch (static_cast<EquipSlot>(i)) {
				case EquipSlot::HEAD:
					mx = devilution::InvRect[0].position.x + 1 + devilution::GetRightPanel().position.x;
					my = devilution::InvRect[0].position.y + 1 + devilution::GetRightPanel().position.y;
					break;
				case EquipSlot::LEFTRING:
					mx = devilution::InvRect[1].position.x + 1 + devilution::GetRightPanel().position.x;
					my = devilution::InvRect[1].position.y + 1 + devilution::GetRightPanel().position.y;
					break;
				case EquipSlot::RIGHTRING:
					mx = devilution::InvRect[2].position.x + 1 + devilution::GetRightPanel().position.x;
					my = devilution::InvRect[2].position.y + 1 + devilution::GetRightPanel().position.y;
					break;
				case EquipSlot::AMULET:
					mx = devilution::InvRect[3].position.x + 1 + devilution::GetRightPanel().position.x;
					my = devilution::InvRect[3].position.y + 1 + devilution::GetRightPanel().position.y;
					break;
				case EquipSlot::LEFTHAND:
					mx = devilution::InvRect[4].position.x + 1 + devilution::GetRightPanel().position.x;
					my = devilution::InvRect[4].position.y + 1 + devilution::GetRightPanel().position.y;
					break;
				case EquipSlot::RIGHTHAND:
					mx = devilution::InvRect[5].position.x + 1 + devilution::GetRightPanel().position.x;
					my = devilution::InvRect[5].position.y + 1 + devilution::GetRightPanel().position.y;
					break;
				case EquipSlot::BODY:
					mx = devilution::InvRect[6].position.x + 1 + devilution::GetRightPanel().position.x;
					my = devilution::InvRect[6].position.y + 1 + devilution::GetRightPanel().position.y;
					break;
				default:
					break;
				}
				break;
			}
		} else if (i < 47) {
			if (item.compare(devilution::Players[devilution::MyPlayerId].InvList[i - 7]) && devilution::Players[devilution::MyPlayerId].InvList[i - 7]._itype != devilution::ItemType::None && i - 7 < devilution::Players[devilution::MyPlayerId]._pNumInv) {
				if (!devilution::invflag)
					return;

				for (int rect_index = 0; rect_index < 40; rect_index++) {
					if (devilution::Players[devilution::MyPlayerId].InvGrid[rect_index] == i - 6) {
						int index = rect_index + 7;
						mx = devilution::InvRect[index].position.x + 1 + devilution::GetRightPanel().position.x;
						my = devilution::InvRect[index].position.y + 1 + devilution::GetRightPanel().position.y;
						break;
					}
				}
				break;
			}
		} else {
			if (item.compare(devilution::Players[devilution::MyPlayerId].SpdList[i - 47]) && devilution::Players[devilution::MyPlayerId].SpdList[i - 47]._itype != devilution::ItemType::None) {
				mx = devilution::InvRect[i].position.x + 1 + devilution::GetMainPanel().position.x;
				my = devilution::InvRect[i].position.y + 1 + devilution::GetMainPanel().position.y;
				break;
			}
		}
	}
	if (mx != 0 && my != 0)
		devilution::CheckInvCut(*devilution::MyPlayer, devilution::Point { mx, my }, false, false);
}

void Server::putCursorItem(int location)
{
	if (devilution::MyPlayer->HoldItem.isEmpty())
		return;

	EquipSlot equipLocation = static_cast<EquipSlot>(location);

	if (!data->invflag)
		return;

	int invRectIndex = location;
	devilution::Point cursorPosition;
	devilution::Displacement panelOffset;
	devilution::Displacement hotPixelCellOffset;
	if (equipLocation == EquipSlot::LEFTRING) {
		invRectIndex = 1;
	} else if (equipLocation == EquipSlot::RIGHTRING) {
		invRectIndex = 2;
	}
	if (equipLocation < EquipSlot::BELT1) {
		panelOffset = devilution::GetRightPanel().position - devilution::Point { 0, 0 };
	} else {
		panelOffset = devilution::GetMainPanel().position - devilution::Point { 0, 0 };
	}
	if (EquipSlot::INV1 <= equipLocation && equipLocation <= EquipSlot::INV40) {
		const devilution::Size itemSize = devilution::GetInventorySize(devilution::MyPlayer->HoldItem);
		if (itemSize.height <= 1 && itemSize.width <= 1) {
			hotPixelCellOffset = devilution::Displacement { 1, 1 };
		}
		hotPixelCellOffset = { (itemSize.width - 1) / 2 + 19, (itemSize.height - 1) / 2 + 19 };
	} else {
		hotPixelCellOffset = devilution::Displacement { 1, 1 };
	}

	cursorPosition = devilution::InvRect[invRectIndex].position + panelOffset + hotPixelCellOffset;
	devilution::CheckInvPaste(*devilution::MyPlayer, cursorPosition);
}

void Server::dropCursorItem()
{
	if (!isOnScreen(devilution::Players[devilution::MyPlayerId].position.tile.x, devilution::Players[devilution::MyPlayerId].position.tile.y))
		return;

	if (12 <= devilution::pcurs) {
		devilution::NetSendCmdPItem(true, devilution::_cmd_id::CMD_PUTITEM, devilution::Point { devilution::Players[devilution::MyPlayerId].position.tile.x, devilution::Players[devilution::MyPlayerId].position.tile.y }, devilution::MyPlayer->HoldItem);
		devilution::NewCursor(devilution::cursor_id::CURSOR_HAND);
	}
}

void Server::useItem(size_t itemID)
{
	if (!OKToAct())
		return;

	if (data->itemList.size() <= itemID)
		return;

	auto itemData = data->itemList[itemID];
	for (int i = 0; i < 8; i++) {
		if (devilution::Players[devilution::MyPlayerId].SpdList[i]._itype != devilution::ItemType::None && itemData.compare(devilution::Players[devilution::MyPlayerId].SpdList[i])) {
			useBeltItem(i);
			return;
		}
	}

	for (int i = 0; i < devilution::Players[devilution::MyPlayerId]._pNumInv; i++) {
		if (devilution::Players[devilution::MyPlayerId].InvList[i]._itype != devilution::ItemType::None && itemData.compare(devilution::Players[devilution::MyPlayerId].InvList[i])) {
			devilution::UseInvItem(i + 7);
			return;
		}
	}
}

void Server::identifyStoreItem(int itemID)
{
	if (devilution::ActiveStore != devilution::TalkID::StorytellerIdentify)
		return;

	int id = -1;

	for (int i = 0; i < 20; i++) {
		if (data->itemList[itemID].compare(devilution::PlayerItems[i])) {
			id = i;
			break;
		}
	}

	if (id == -1)
		return;

	devilution::PlaySFX(devilution::SfxID::MenuSelect);

	devilution::OldActiveStore = devilution::TalkID::StorytellerIdentify;
	devilution::OldTextLine = devilution::CurrentTextLine;
	devilution::OldScrollPos = devilution::ScrollPos;

	if (!devilution::PlayerCanAfford(devilution::PlayerItems[id]._iIvalue)) {
		devilution::StartStore(devilution::TalkID::NoMoney);
		return;
	} else {
		devilution::Player &myPlayer = *devilution::MyPlayer;

		int idx = devilution::PlayerItemIndexes[id];
		if (idx < 0) {
			if (idx == -1)
				myPlayer.InvBody[devilution::INVLOC_HEAD]._iIdentified = true;
			if (idx == -2)
				myPlayer.InvBody[devilution::INVLOC_CHEST]._iIdentified = true;
			if (idx == -3)
				myPlayer.InvBody[devilution::INVLOC_HAND_LEFT]._iIdentified = true;
			if (idx == -4)
				myPlayer.InvBody[devilution::INVLOC_HAND_RIGHT]._iIdentified = true;
			if (idx == -5)
				myPlayer.InvBody[devilution::INVLOC_RING_LEFT]._iIdentified = true;
			if (idx == -6)
				myPlayer.InvBody[devilution::INVLOC_RING_RIGHT]._iIdentified = true;
			if (idx == -7)
				myPlayer.InvBody[devilution::INVLOC_AMULET]._iIdentified = true;
		} else {
			myPlayer.InvList[idx]._iIdentified = true;
		}
		devilution::PlayerItems[id]._iIdentified = true;
		devilution::TakePlrsMoney(devilution::PlayerItems[id]._iIvalue);
		devilution::CalcPlrInv(myPlayer, true);
		devilution::StartStore(devilution::OldActiveStore);
	}
}

void Server::castSpell(int x, int y)
{
	if (!OKToAct())
		return;

	if (!isOnScreen(x, y))
		return;

	devilution::pcursmonst = -1;
	devilution::PlayerUnderCursor = nullptr;
	devilution::cursPosition = devilution::Point { x, y };

	devilution::CheckPlrSpell(false);
}

void Server::cancelQText()
{
	if (!data->qtextflag)
		return;

	devilution::qtextflag = false;
	data->qtextflag = false;
	devilution::stream_stop();
}

void Server::setFPS(int newFPS)
{
	FPS = newFPS;
	devilution::sgGameInitInfo.nTickRate = newFPS;
	devilution::gnTickDelay = 1000 / devilution::sgGameInitInfo.nTickRate;
}

void Server::disarmTrap(int index)
{
	if (devilution::pcurs != devilution::cursor_id::CURSOR_DISARM)
		return;

	if (static_cast<devilution::HeroClass>(data->playerList[devilution::MyPlayerId]._pClass) != devilution::HeroClass::Rogue)
		return;

	devilution::NetSendCmdLoc(devilution::MyPlayerId, true, devilution::_cmd_id::CMD_DISARMXY, devilution::Point { devilution::Objects[index].position.x, devilution::Objects[index].position.y });
}

void Server::skillRepair(int itemID)
{
	if (devilution::pcurs != devilution::cursor_id::CURSOR_REPAIR)
		return;

	if (static_cast<devilution::HeroClass>(data->playerList[devilution::MyPlayerId]._pClass) != devilution::HeroClass::Warrior)
		return;

	if (!data->invflag)
		return;

	for (int i = 0; i < 7; i++) {
		if (data->itemList[itemID].compare(devilution::Players[devilution::MyPlayerId].InvBody[i])) {
			devilution::DoRepair(*devilution::MyPlayer, i);
			return;
		}
	}
	for (int i = 0; i < MAXINV; i++) {
		if (data->itemList[itemID].compare(devilution::Players[devilution::MyPlayerId].InvList[i])) {
			devilution::DoRepair(*devilution::MyPlayer, i + 7);
			return;
		}
	}
}

void Server::skillRecharge(int itemID)
{
	if (devilution::pcurs != devilution::cursor_id::CURSOR_RECHARGE)
		return;

	if (static_cast<devilution::HeroClass>(data->playerList[devilution::MyPlayerId]._pClass) != devilution::HeroClass::Sorcerer)
		return;

	for (int i = 0; i < 7; i++) {
		if (data->itemList[itemID].compare(devilution::Players[devilution::MyPlayerId].InvBody[i])) {
			devilution::DoRecharge(*devilution::MyPlayer, i);
			return;
		}
	}
	for (int i = 0; i < MAXINV; i++) {
		if (data->itemList[itemID].compare(devilution::Players[devilution::MyPlayerId].InvList[i])) {
			devilution::DoRecharge(*devilution::MyPlayer, i + 7);
			return;
		}
	}
}

void Server::toggleMenu()
{
	devilution::qtextflag = false;
	if (!devilution::gmenu_is_active())
		devilution::gamemenu_on();
	else
		devilution::gamemenu_off();
	return;
}

void Server::saveGame()
{
	if (devilution::gbIsMultiplayer || !devilution::gmenu_is_active())
		return;

	devilution::gmenu_presskeys(SDLK_DOWN);
	devilution::gmenu_presskeys(SDLK_KP_ENTER);
	return;
}

void Server::quit()
{
	if (!devilution::gmenu_is_active())
		return;

	devilution::gmenu_presskeys(SDLK_UP);
	devilution::gmenu_presskeys(SDLK_KP_ENTER);
	return;
}

void Server::clearCursor()
{
	if (devilution::pcurs == devilution::cursor_id::CURSOR_REPAIR || devilution::pcurs == devilution::cursor_id::CURSOR_DISARM || devilution::pcurs == devilution::cursor_id::CURSOR_RECHARGE || devilution::pcurs == devilution::cursor_id::CURSOR_IDENTIFY)
		devilution::NewCursor(devilution::cursor_id::CURSOR_HAND);

	return;
}

void Server::identifyItem(int itemID)
{
	if (devilution::pcurs != devilution::cursor_id::CURSOR_IDENTIFY)
		return;

	if (!data->invflag)
		return;

	for (int i = 0; i < 7; i++) {
		if (data->itemList[itemID].compare(devilution::Players[devilution::MyPlayerId].InvBody[i])) {
			devilution::CheckIdentify(*devilution::MyPlayer, i);
			return;
		}
	}
	for (int i = 0; i < MAXINV; i++) {
		if (data->itemList[itemID].compare(devilution::Players[devilution::MyPlayerId].InvList[i])) {
			devilution::CheckIdentify(*devilution::MyPlayer, i + 7);
			return;
		}
	}
}

void Server::sendChat(std::string message)
{
	if (!devilution::gbIsMultiplayer)
		return;

	if (79 < message.length())
		message = message.substr(0, 79);

	char charMsg[MAX_SEND_STR_LEN];
	devilution::CopyUtf8(charMsg, message, sizeof(charMsg));
	devilution::NetSendCmdString(0xFFFFFF, charMsg);
}
} // namespace DAPI
