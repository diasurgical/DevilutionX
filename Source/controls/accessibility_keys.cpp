/**
 * @file controls/accessibility_keys.cpp
 *
 * UI accessibility key handlers and action-guard helpers.
 */
#include "controls/accessibility_keys.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

#include <fmt/format.h>

#include "control/control.hpp"
#include "controls/plrctrls.h"
#include "diablo.h"
#include "gamemenu.h"
#include "help.h"
#include "inv.h"
#include "levels/gendung.h"
#include "levels/setmaps.h"
#include "minitext.h"
#include "options.h"
#include "panels/charpanel.hpp"
#include "panels/partypanel.hpp"
#include "panels/spell_book.hpp"
#include "panels/spell_list.hpp"
#include "player.h"
#include "qol/chatlog.h"
#include "qol/stash.h"
#include "quests.h"
#include "stores.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/screen_reader.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

void SpeakPlayerHealthPercentageKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (MyPlayer == nullptr)
		return;

	const int maxHp = MyPlayer->_pMaxHP;
	if (maxHp <= 0)
		return;

	const int currentHp = std::max(MyPlayer->_pHitPoints, 0);
	int hpPercent = static_cast<int>((static_cast<int64_t>(currentHp) * 100 + maxHp / 2) / maxHp);
	hpPercent = std::clamp(hpPercent, 0, 100);
	SpeakText(fmt::format("{:d}%", hpPercent), /*force=*/true);
}

void SpeakExperienceToNextLevelKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;
	if (MyPlayer == nullptr)
		return;

	const Player &myPlayer = *MyPlayer;
	if (myPlayer.isMaxCharacterLevel()) {
		SpeakText(_("Max level."), /*force=*/true);
		return;
	}

	const uint32_t nextExperienceThreshold = myPlayer.getNextExperienceThreshold();
	const uint32_t currentExperience = myPlayer._pExperience;
	const uint32_t remainingExperience = currentExperience >= nextExperienceThreshold ? 0 : nextExperienceThreshold - currentExperience;
	const int nextLevel = myPlayer.getCharacterLevel() + 1;
	SpeakText(
	    fmt::format(fmt::runtime(_("{:s} to Level {:d}")), FormatInteger(remainingExperience), nextLevel),
	    /*force=*/true);
}

std::string BuildCurrentLocationForSpeech()
{
	// Quest Level Name
	if (setlevel) {
		const char *const questLevelName = QuestLevelNames[setlvlnum];
		if (questLevelName == nullptr || questLevelName[0] == '\0')
			return std::string { _("Set level") };

		return fmt::format("{:s}: {:s}", _("Set level"), _(questLevelName));
	}

	// Dungeon Name
	constexpr std::array<const char *, DTYPE_LAST + 1> DungeonStrs = {
		N_("Town"),
		N_("Cathedral"),
		N_("Catacombs"),
		N_("Caves"),
		N_("Hell"),
		N_("Nest"),
		N_("Crypt"),
	};
	std::string dungeonStr;
	if (leveltype >= DTYPE_TOWN && leveltype <= DTYPE_LAST) {
		dungeonStr = _(DungeonStrs[static_cast<size_t>(leveltype)]);
	} else {
		dungeonStr = _(/* TRANSLATORS: type of dungeon (i.e. Cathedral, Caves)*/ "None");
	}

	if (leveltype == DTYPE_TOWN || currlevel <= 0)
		return dungeonStr;

	// Dungeon Level
	int level = currlevel;
	if (leveltype == DTYPE_CATACOMBS)
		level -= 4;
	else if (leveltype == DTYPE_CAVES)
		level -= 8;
	else if (leveltype == DTYPE_HELL)
		level -= 12;
	else if (leveltype == DTYPE_NEST)
		level -= 16;
	else if (leveltype == DTYPE_CRYPT)
		level -= 20;

	if (level <= 0)
		return dungeonStr;

	return fmt::format(fmt::runtime(_(/* TRANSLATORS: dungeon type and floor number i.e. "Cathedral 3"*/ "{} {}")), dungeonStr, level);
}

void SpeakCurrentLocationKeyPressed()
{
	if (!CanPlayerTakeAction())
		return;

	SpeakText(BuildCurrentLocationForSpeech(), /*force=*/true);
}

void InventoryKeyPressed()
{
	if (IsPlayerInStore())
		return;
	invflag = !invflag;
	if (!IsLeftPanelOpen() && CanPanelsCoverView()) {
		if (!invflag) { // We closed the inventory
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		} else if (!SpellbookFlag) { // We opened the inventory
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		}
	}
	SpellbookFlag = false;
	CloseGoldWithdraw();
	CloseStash();
	if (invflag)
		FocusOnInventory();
}

void CharacterSheetKeyPressed()
{
	if (IsPlayerInStore())
		return;
	if (!IsRightPanelOpen() && CanPanelsCoverView()) {
		if (CharFlag) { // We are closing the character sheet
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		} else if (!QuestLogIsOpen) { // We opened the character sheet
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		}
	}
	ToggleCharPanel();
}

void PartyPanelSideToggleKeyPressed()
{
	PartySidePanelOpen = !PartySidePanelOpen;
}

void QuestLogKeyPressed()
{
	if (IsPlayerInStore())
		return;
	if (!QuestLogIsOpen) {
		StartQuestlog();
	} else {
		QuestLogIsOpen = false;
	}
	if (!IsRightPanelOpen() && CanPanelsCoverView()) {
		if (!QuestLogIsOpen) { // We closed the quest log
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		} else if (!CharFlag) { // We opened the quest log
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		}
	}
	CloseCharPanel();
	CloseGoldWithdraw();
	CloseStash();
}

void SpeakSelectedSpeedbookSpell()
{
	for (const auto &spellListItem : GetSpellListItems()) {
		if (spellListItem.isSelected) {
			SpeakText(pgettext("spell", GetSpellData(spellListItem.id).sNameText), /*force=*/true);
			return;
		}
	}
	SpeakText(_("No spell selected."), /*force=*/true);
}

void DisplaySpellsKeyPressed()
{
	if (IsPlayerInStore())
		return;
	CloseCharPanel();
	QuestLogIsOpen = false;
	CloseInventory();
	SpellbookFlag = false;
	if (!SpellSelectFlag) {
		DoSpeedBook();
		SpeakSelectedSpeedbookSpell();
	} else {
		SpellSelectFlag = false;
	}
	LastPlayerAction = PlayerActionType::None;
}

void SpellBookKeyPressed()
{
	if (IsPlayerInStore())
		return;
	SpellbookFlag = !SpellbookFlag;
	if (SpellbookFlag && MyPlayer != nullptr) {
		const Player &player = *MyPlayer;
		if (IsValidSpell(player._pRSpell)) {
			SpeakText(pgettext("spell", GetSpellData(player._pRSpell).sNameText), /*force=*/true);
		} else {
			SpeakText(_("No spell selected."), /*force=*/true);
		}
	}
	if (!IsLeftPanelOpen() && CanPanelsCoverView()) {
		if (!SpellbookFlag) { // We closed the spellbook
			if (MousePosition.x < 480 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition + Displacement { 160, 0 });
			}
		} else if (!invflag) { // We opened the spellbook
			if (MousePosition.x > 160 && MousePosition.y < GetMainPanel().position.y) {
				SetCursorPos(MousePosition - Displacement { 160, 0 });
			}
		}
	}
	CloseInventory();
}

void CycleSpellHotkeys(bool next)
{
	if (MyPlayer == nullptr)
		return;
	StaticVector<size_t, NumHotkeys> validHotKeyIndexes;
	std::optional<size_t> currentIndex;
	for (size_t slot = 0; slot < NumHotkeys; slot++) {
		if (!IsValidSpeedSpell(slot))
			continue;
		if (MyPlayer->_pRSpell == MyPlayer->_pSplHotKey[slot] && MyPlayer->_pRSplType == MyPlayer->_pSplTHotKey[slot]) {
			// found current
			currentIndex = validHotKeyIndexes.size();
		}
		validHotKeyIndexes.emplace_back(slot);
	}
	if (validHotKeyIndexes.empty())
		return;

	size_t newIndex;
	if (!currentIndex) {
		newIndex = next ? 0 : (validHotKeyIndexes.size() - 1);
	} else if (next) {
		newIndex = (*currentIndex == validHotKeyIndexes.size() - 1) ? 0 : (*currentIndex + 1);
	} else {
		newIndex = *currentIndex == 0 ? (validHotKeyIndexes.size() - 1) : (*currentIndex - 1);
	}
	ToggleSpell(validHotKeyIndexes[newIndex]);
}

bool IsPlayerDead()
{
	if (MyPlayer == nullptr)
		return true;
	return MyPlayer->_pmode == PM_DEATH || MyPlayerIsDead;
}

bool IsGameRunning()
{
	return PauseMode != 2;
}

bool CanPlayerTakeAction()
{
	return !IsPlayerDead() && IsGameRunning();
}

bool CanAutomapBeToggledOff()
{
	return !QuestLogIsOpen && !IsWithdrawGoldOpen && !IsStashOpen && !CharFlag
	    && !SpellbookFlag && !invflag && !isGameMenuOpen && !qtextflag && !SpellSelectFlag
	    && !ChatLogFlag && !HelpFlag;
}

} // namespace devilution
