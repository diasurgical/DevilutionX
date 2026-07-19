#include "tables/questdat.hpp"

#include <expected.hpp>
#include <magic_enum/magic_enum.hpp>

#include "data/file.hpp"
#include "data/record_reader.hpp"
#include "game_mode.hpp"
#include "levels/dun_tile_data.hpp"
#include "levels/parse_dungeon_type.hpp"
#include "tables/textdat.h"

namespace devilution {

/** Contains the quests of the current game. */
Quest Quests[MAXQUESTS];
Point ReturnLvlPosition;
dungeon_type ReturnLevelType;
int ReturnLevel;

/** Contains the data related to each quest_id. */
std::vector<QuestData> QuestsData;

namespace {

tl::expected<_setlevels, std::string> ParseSetLevel(std::string_view value)
{
	const std::optional<_setlevels> enumValueOpt = magic_enum::enum_cast<_setlevels>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	return tl::make_unexpected("Unknown enum value");
}

void LoadQuestDatFromFile(DataFile &dataFile, std::string_view filename)
{
	dataFile.skipHeaderOrDie(filename);

	QuestsData.reserve(QuestsData.size() + dataFile.numRecords());

	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		QuestData &quest = QuestsData.emplace_back();
		reader.readInt("qdlvl", quest._qdlvl);
		reader.readInt("qdmultlvl", quest._qdmultlvl);
		reader.read("qlvlt", quest._qlvlt, ParseDungeonType);
		reader.readInt("bookOrder", quest.questBookOrder);
		reader.readInt("qdrnd", quest._qdrnd);
		reader.read("qslvl", quest._qslvl, ParseSetLevel);
		reader.readBool("isSinglePlayerOnly", quest.isSinglePlayerOnly);
		reader.read("qdmsg", quest._qdmsg, ParseSpeechId);
		reader.readString("qlstr", quest._qlstr);
	}
}

} // namespace

bool UseMultiplayerQuests()
{
	return sgGameInitInfo.fullQuests == 0;
}

bool Quest::IsAvailable() const
{
	if (setlevel)
		return false;
	if (currlevel != _qlevel)
		return false;
	if (_qactive == QUEST_NOTAVAIL)
		return false;
	if (QuestsData[_qidx].isSinglePlayerOnly && UseMultiplayerQuests())
		return false;

	return true;
}

void LoadQuestData()
{
	const std::string_view filename = "txtdata\\quests\\questdat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);

	QuestsData.clear();
	LoadQuestDatFromFile(dataFile, filename);

	QuestsData.shrink_to_fit();
}

} // namespace devilution
