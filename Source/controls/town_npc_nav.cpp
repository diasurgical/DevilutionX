/**
 * @file controls/town_npc_nav.cpp
 *
 * Town NPC navigation for accessibility.
 */
#include "controls/town_npc_nav.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "controls/accessibility_keys.hpp"
#include "controls/plrctrls.h"
#include "diablo.h"
#include "engine/path.h"
#include "help.h"
#include "levels/gendung.h"
#include "levels/tile_properties.hpp"
#include "multi.h"
#include "options.h"
#include "player.h"
#include "qol/chatlog.h"
#include "stores.h"
#include "towners.h"
#include "utils/language.h"
#include "utils/screen_reader.hpp"
#include "utils/str_cat.hpp"
#include "utils/walk_path_speech.hpp"

namespace devilution {

namespace {

std::vector<int> TownNpcOrder;
int SelectedTownNpc = -1;
int AutoWalkTownNpcTarget = -1;

void ResetTownNpcSelection()
{
	TownNpcOrder.clear();
	SelectedTownNpc = -1;
}

void RefreshTownNpcOrder(bool selectFirst = false)
{
	TownNpcOrder.clear();
	if (leveltype != DTYPE_TOWN)
		return;

	const Point playerPosition = MyPlayer->position.future;

	for (size_t i = 0; i < GetNumTowners(); ++i) {
		const Towner &towner = Towners[i];
		if (!IsTownerPresent(towner._ttype))
			continue;
		if (towner._ttype == TOWN_COW)
			continue;
		TownNpcOrder.push_back(static_cast<int>(i));
	}

	if (TownNpcOrder.empty()) {
		SelectedTownNpc = -1;
		return;
	}

	std::sort(TownNpcOrder.begin(), TownNpcOrder.end(), [&playerPosition](int a, int b) {
		const Towner &townerA = Towners[a];
		const Towner &townerB = Towners[b];
		const int distanceA = playerPosition.WalkingDistance(townerA.position);
		const int distanceB = playerPosition.WalkingDistance(townerB.position);
		if (distanceA != distanceB)
			return distanceA < distanceB;
		return townerA.name < townerB.name;
	});

	if (selectFirst) {
		SelectedTownNpc = TownNpcOrder.front();
		return;
	}

	const auto it = std::find(TownNpcOrder.begin(), TownNpcOrder.end(), SelectedTownNpc);
	if (it == TownNpcOrder.end())
		SelectedTownNpc = TownNpcOrder.front();
}

void EnsureTownNpcOrder()
{
	if (leveltype != DTYPE_TOWN) {
		ResetTownNpcSelection();
		return;
	}
	if (TownNpcOrder.empty()) {
		RefreshTownNpcOrder(true);
		return;
	}
	if (SelectedTownNpc < 0 || SelectedTownNpc >= static_cast<int>(GetNumTowners())) {
		RefreshTownNpcOrder(true);
		return;
	}
	const auto it = std::find(TownNpcOrder.begin(), TownNpcOrder.end(), SelectedTownNpc);
	if (it == TownNpcOrder.end())
		SelectedTownNpc = TownNpcOrder.front();
}

void SelectTownNpcRelative(int delta)
{
	if (!IsTownNpcActionAllowed())
		return;

	EnsureTownNpcOrder();
	if (TownNpcOrder.empty()) {
		SpeakText(_("No town NPCs found."), true);
		return;
	}

	auto it = std::find(TownNpcOrder.begin(), TownNpcOrder.end(), SelectedTownNpc);
	int currentIndex = (it != TownNpcOrder.end()) ? static_cast<int>(it - TownNpcOrder.begin()) : 0;

	const int size = static_cast<int>(TownNpcOrder.size());
	int newIndex = (currentIndex + delta) % size;
	if (newIndex < 0)
		newIndex += size;
	SelectedTownNpc = TownNpcOrder[static_cast<size_t>(newIndex)];
	SpeakSelectedTownNpc();
}

} // namespace

bool IsTownNpcActionAllowed()
{
	return CanPlayerTakeAction()
	    && leveltype == DTYPE_TOWN
	    && !IsPlayerInStore()
	    && !ChatLogFlag
	    && !HelpFlag;
}

void SpeakSelectedTownNpc()
{
	EnsureTownNpcOrder();

	if (SelectedTownNpc < 0 || SelectedTownNpc >= static_cast<int>(GetNumTowners())) {
		SpeakText(_("No NPC selected."), true);
		return;
	}

	const Towner &towner = Towners[SelectedTownNpc];
	const Point playerPosition = MyPlayer->position.future;
	const int distance = playerPosition.WalkingDistance(towner.position);

	std::string msg;
	StrAppend(msg, towner.name);
	StrAppend(msg, "\n", _("Distance: "), distance);
	StrAppend(msg, "\n", _("Position: "), towner.position.x, ", ", towner.position.y);
	SpeakText(msg, true);
}

void SelectNextTownNpcKeyPressed()
{
	SelectTownNpcRelative(+1);
}

void SelectPreviousTownNpcKeyPressed()
{
	SelectTownNpcRelative(-1);
}

void GoToSelectedTownNpcKeyPressed()
{
	if (!IsTownNpcActionAllowed())
		return;

	EnsureTownNpcOrder();
	if (SelectedTownNpc < 0 || SelectedTownNpc >= static_cast<int>(GetNumTowners())) {
		SpeakText(_("No NPC selected."), true);
		return;
	}

	const Towner &towner = Towners[SelectedTownNpc];

	std::string msg;
	StrAppend(msg, _("Going to: "), towner.name);
	SpeakText(msg, true);

	AutoWalkTownNpcTarget = SelectedTownNpc;
	UpdateAutoWalkTownNpc();
}

void UpdateAutoWalkTownNpc()
{
	if (AutoWalkTownNpcTarget < 0)
		return;
	if (leveltype != DTYPE_TOWN || IsPlayerInStore() || ChatLogFlag || HelpFlag) {
		AutoWalkTownNpcTarget = -1;
		return;
	}
	if (!CanPlayerTakeAction())
		return;

	if (MyPlayer->_pmode != PM_STAND)
		return;
	if (MyPlayer->walkpath[0] != WALK_NONE)
		return;
	if (MyPlayer->destAction != ACTION_NONE)
		return;

	if (AutoWalkTownNpcTarget >= static_cast<int>(GetNumTowners())) {
		AutoWalkTownNpcTarget = -1;
		SpeakText(_("No NPC selected."), true);
		return;
	}

	const Towner &towner = Towners[AutoWalkTownNpcTarget];
	if (!IsTownerPresent(towner._ttype) || towner._ttype == TOWN_COW) {
		AutoWalkTownNpcTarget = -1;
		SpeakText(_("No NPC selected."), true);
		return;
	}

	Player &myPlayer = *MyPlayer;
	const Point playerPosition = myPlayer.position.future;
	if (playerPosition.WalkingDistance(towner.position) < 2) {
		const int townerIdx = AutoWalkTownNpcTarget;
		AutoWalkTownNpcTarget = -1;
		NetSendCmdLocParam1(true, CMD_TALKXY, towner.position, static_cast<uint16_t>(townerIdx));
		return;
	}

	constexpr size_t MaxAutoWalkPathLength = 512;
	std::array<int8_t, MaxAutoWalkPathLength> path;
	path.fill(WALK_NONE);

	const int steps = FindPath(CanStep, [&myPlayer](Point position) { return PosOkPlayer(myPlayer, position); }, playerPosition, towner.position, path.data(), path.size());
	if (steps == 0) {
		AutoWalkTownNpcTarget = -1;
		std::string error;
		StrAppend(error, _("Can't find a path to: "), towner.name);
		SpeakText(error, true);
		return;
	}

	// FindPath returns 0 if the path length is equal to the maximum.
	// The player walkpath buffer is MaxPathLengthPlayer, so keep segments strictly shorter.
	if (steps < static_cast<int>(MaxPathLengthPlayer)) {
		const int townerIdx = AutoWalkTownNpcTarget;
		AutoWalkTownNpcTarget = -1;
		NetSendCmdLocParam1(true, CMD_TALKXY, towner.position, static_cast<uint16_t>(townerIdx));
		return;
	}

	const int segmentSteps = std::min(steps - 1, static_cast<int>(MaxPathLengthPlayer - 1));
	const Point waypoint = PositionAfterWalkPathSteps(playerPosition, path.data(), segmentSteps);
	NetSendCmdLoc(MyPlayerId, true, CMD_WALKXY, waypoint);
}

void ResetAutoWalkTownNpc()
{
	AutoWalkTownNpcTarget = -1;
}

void ListTownNpcsKeyPressed()
{
	if (leveltype != DTYPE_TOWN) {
		ResetTownNpcSelection();
		SpeakText(_("Not in town."), true);
		return;
	}
	if (IsPlayerInStore())
		return;

	std::vector<const Towner *> townNpcs;
	std::vector<const Towner *> cows;

	townNpcs.reserve(Towners.size());
	cows.reserve(Towners.size());

	const Point playerPosition = MyPlayer->position.future;

	for (const Towner &towner : Towners) {
		if (!IsTownerPresent(towner._ttype))
			continue;

		if (towner._ttype == TOWN_COW) {
			cows.push_back(&towner);
			continue;
		}

		townNpcs.push_back(&towner);
	}

	if (townNpcs.empty() && cows.empty()) {
		ResetTownNpcSelection();
		SpeakText(_("No town NPCs found."), true);
		return;
	}

	std::sort(townNpcs.begin(), townNpcs.end(), [&playerPosition](const Towner *a, const Towner *b) {
		const int distanceA = playerPosition.WalkingDistance(a->position);
		const int distanceB = playerPosition.WalkingDistance(b->position);
		if (distanceA != distanceB)
			return distanceA < distanceB;
		return a->name < b->name;
	});

	std::string output;
	StrAppend(output, _("Town NPCs:"));
	for (size_t i = 0; i < townNpcs.size(); ++i) {
		StrAppend(output, "\n", i + 1, ". ", townNpcs[i]->name);
	}
	if (!cows.empty()) {
		StrAppend(output, "\n", _("Cows: "), static_cast<int>(cows.size()));
	}

	RefreshTownNpcOrder(true);
	if (SelectedTownNpc >= 0 && SelectedTownNpc < static_cast<int>(GetNumTowners())) {
		const Towner &towner = Towners[SelectedTownNpc];
		StrAppend(output, "\n", _("Selected: "), towner.name);
		StrAppend(output, "\n", _("PageUp/PageDown: select. Home: go. End: repeat."));
	}
	const std::string_view exitKey = GetOptions().Keymapper.KeyNameForAction("SpeakNearestExit");
	if (!exitKey.empty()) {
		StrAppend(output, "\n", fmt::format(fmt::runtime(_("Cathedral entrance: press {:s}.")), exitKey));
	}

	SpeakText(output, true);
}

} // namespace devilution
