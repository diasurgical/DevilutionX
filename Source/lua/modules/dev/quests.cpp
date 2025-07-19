#ifdef _DEBUG
#include "lua/modules/dev/quests.hpp"

#include <cstdint>
#include <string>

#include <sol/sol.hpp>

#include "engine/render/primitive_render.hpp"
#include "lua/metadoc.hpp"
#include "quests.h"
#include "utils/is_of.hpp"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {

std::string DebugCmdEnableQuest(uint8_t questId)
{
	if (questId >= MAXQUESTS) return StrCat("Quest ", questId, " does not exist!");
	Quest &quest = Quests[questId];

	if (IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT))
		return StrCat(QuestsData[questId]._qlstr, " is already active!");

	quest._qactive = QUEST_ACTIVE;
	quest._qlog = true;

	return StrCat(QuestsData[questId]._qlstr, " activated.");
}

std::string DebugCmdEnableQuests()
{
	for (Quest &quest : Quests) {
		if (IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT)) continue;
		quest._qactive = QUEST_ACTIVE;
		quest._qlog = true;
	}
	return "Activated all quests.";
}

std::string DebugCmdQuestInfo(const uint8_t questId)
{
	if (questId >= MAXQUESTS) return StrCat("Quest ", questId, " does not exist!");
	const Quest &quest = Quests[questId];
	return StrCat("Quest id=", quest._qidx, " ", QuestsData[quest._qidx]._qlstr,
	    " active=", quest._qactive, " var1=", quest._qvar1, " var2=", quest._qvar2);
}

std::string DebugCmdQuestsInfo()
{
	std::string ret;
	for (const Quest &quest : Quests) {
		StrAppend(ret, "Quest id=", quest._qidx, " ", QuestsData[quest._qidx]._qlstr,
		    " active=", quest._qactive, " var1=", quest._qvar1, " var2=", quest._qvar2, "\n");
	}
	if (!ret.empty()) ret.pop_back();
	return ret;
}

} // namespace

sol::table LuaDevQuestsModule(sol::state_view &lua)
{
	sol::table table = lua.create_table();
	LuaSetDocFn(table, "activate", "(id: number)", "Activate the given quest.", &DebugCmdEnableQuest);
	LuaSetDocFn(table, "activateAll", "()", "Activate all available quests.", &DebugCmdEnableQuests);
	LuaSetDocFn(table, "all", "()", "Information on all available quest.", &DebugCmdQuestsInfo);
	LuaSetDocFn(table, "info", "(id: number)", "Information on the given quest.", &DebugCmdQuestInfo);
	return table;
}

} // namespace devilution
#endif // _DEBUG
