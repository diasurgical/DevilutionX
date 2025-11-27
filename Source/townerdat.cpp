/**
 * @file townerdat.cpp
 *
 * Implementation of towner data loading from TSV files.
 */
#include "townerdat.hpp"

#include <charconv>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <expected.hpp>
#include <magic_enum/magic_enum.hpp>

#include "data/file.hpp"
#include "data/record_reader.hpp"

namespace devilution {

std::vector<TownerDataEntry> TownersDataEntries;
std::vector<std::array<_speech_id, MAXQUESTS>> TownerQuestDialogTable;

namespace {

/**
 * @brief Generic enum parser using magic_enum.
 * @tparam EnumT The enum type to parse
 * @param value The string representation of the enum value
 * @return The parsed enum value, or an error message
 */
template <typename EnumT>
tl::expected<EnumT, std::string> ParseEnum(std::string_view value)
{
	const auto enumValueOpt = magic_enum::enum_cast<EnumT>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}
	return tl::make_unexpected("Unknown enum value");
}

/**
 * @brief Parses a comma-separated list of values.
 * @tparam T The output type
 * @tparam Parser A callable that converts string_view to optional<T>
 * @param value The comma-separated string
 * @param out Vector to store parsed values (cleared first)
 * @param parser Function to parse individual tokens
 */
template <typename T, typename Parser>
void ParseCommaSeparatedList(std::string_view value, std::vector<T> &out, Parser parser)
{
	out.clear();
	if (value.empty()) return;

	size_t start = 0;
	while (start < value.size()) {
		size_t end = value.find(',', start);
		if (end == std::string_view::npos) end = value.size();

		std::string_view token = value.substr(start, end - start);
		if (auto result = parser(token)) {
			out.push_back(*result);
		}

		start = end + 1;
	}
}

/**
 * @brief Parses a comma-separated list of speech IDs.
 */
void ParseGossipTexts(std::string_view value, std::vector<_speech_id> &out)
{
	ParseCommaSeparatedList(value, out, [](std::string_view token) -> std::optional<_speech_id> {
		if (auto result = ParseSpeechId(token); result.has_value()) {
			return result.value();
		}
		return std::nullopt;
	});
}

/**
 * @brief Parses a comma-separated list of integers for animation frame order.
 */
void ParseAnimOrder(std::string_view value, std::vector<uint8_t> &out)
{
	ParseCommaSeparatedList(value, out, [](std::string_view token) -> std::optional<uint8_t> {
		int val = 0;
		if (auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), val); ec == std::errc()) {
			return static_cast<uint8_t>(val);
		}
		return std::nullopt;
	});
}

void LoadTownersFromFile()
{
	const std::string_view filename = "txtdata\\towners\\towners.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	dataFile.skipHeaderOrDie(filename);

	TownersDataEntries.clear();
	TownersDataEntries.reserve(dataFile.numRecords());

	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		TownerDataEntry &entry = TownersDataEntries.emplace_back();

		reader.read("type", entry.type, ParseEnum<_talker_id>);
		reader.readString("name", entry.name);
		reader.readInt("position_x", entry.position.x);
		reader.readInt("position_y", entry.position.y);
		reader.read("direction", entry.direction, ParseEnum<Direction>);
		reader.readInt("animWidth", entry.animWidth);
		reader.readString("animPath", entry.animPath);
		reader.readOptionalInt("animFrames", entry.animFrames);
		reader.readOptionalInt("animDelay", entry.animDelay);

		std::string gossipStr;
		reader.readString("gossipTexts", gossipStr);
		ParseGossipTexts(gossipStr, entry.gossipTexts);

		std::string animOrderStr;
		reader.readString("animOrder", animOrderStr);
		ParseAnimOrder(animOrderStr, entry.animOrder);
	}

	TownersDataEntries.shrink_to_fit();
}

void LoadQuestDialogFromFile()
{
	const std::string_view filename = "txtdata\\towners\\quest_dialog.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);

	// Initialize table with TEXT_NONE
	TownerQuestDialogTable.clear();
	TownerQuestDialogTable.resize(NUM_TOWNER_TYPES);
	for (auto &row : TownerQuestDialogTable) {
		row.fill(TEXT_NONE);
	}

	// Parse header to find which quest columns exist
	DataFileRecord headerRecord = *dataFile.begin();
	std::unordered_map<std::string, unsigned> columnMap;
	unsigned columnIndex = 0;
	for (DataFileField field : headerRecord) {
		columnMap[std::string(field.value())] = columnIndex++;
	}

	// Reset header position and skip for data reading
	dataFile.resetHeader();
	dataFile.skipHeaderOrDie(filename);

	// Find the towner_type column index
	auto townerTypeColIt = columnMap.find("towner_type");
	if (townerTypeColIt == columnMap.end()) {
		return; // Invalid file format
	}
	unsigned townerTypeColIndex = townerTypeColIt->second;

	// Build quest column index map
	std::unordered_map<quest_id, unsigned> questColumnMap;
	for (quest_id quest : magic_enum::enum_values<quest_id>()) {
		if (quest == Q_INVALID || quest >= MAXQUESTS) continue;

		auto questName = magic_enum::enum_name(quest);
		auto questColIt = columnMap.find(std::string(questName));
		if (questColIt != columnMap.end()) {
			questColumnMap[quest] = questColIt->second;
		}
	}

	// Read data rows
	for (DataFileRecord record : dataFile) {
		// Read all fields into a map keyed by column index for indexed access
		std::unordered_map<unsigned, std::string_view> fields;
		for (DataFileField field : record) {
			fields[field.column()] = field.value();
		}

		// Read towner_type
		auto townerTypeFieldIt = fields.find(townerTypeColIndex);
		if (townerTypeFieldIt == fields.end()) {
			continue; // Invalid row
		}

		auto townerTypeResult = ParseEnum<_talker_id>(townerTypeFieldIt->second);
		if (!townerTypeResult.has_value()) {
			continue; // Invalid towner type
		}
		_talker_id townerType = townerTypeResult.value();

		if (static_cast<size_t>(townerType) >= TownerQuestDialogTable.size()) {
			continue;
		}

		auto &dialogRow = TownerQuestDialogTable[static_cast<size_t>(townerType)];

		// Read quest columns that exist in this file
		for (const auto &[quest, colIndex] : questColumnMap) {
			auto fieldIt = fields.find(colIndex);
			if (fieldIt == fields.end()) {
				continue; // Column missing in this row
			}

			auto speechResult = ParseSpeechId(fieldIt->second);
			if (speechResult.has_value()) {
				dialogRow[quest] = speechResult.value();
			}
		}
	}
}

} // namespace

void LoadTownerData()
{
	LoadTownersFromFile();
	LoadQuestDialogFromFile();
}

_speech_id GetTownerQuestDialog(_talker_id type, quest_id quest)
{
	if (static_cast<size_t>(type) >= TownerQuestDialogTable.size()) {
		return TEXT_NONE;
	}
	if (quest < 0 || quest >= MAXQUESTS) {
		return TEXT_NONE;
	}
	return TownerQuestDialogTable[static_cast<size_t>(type)][quest];
}

void SetTownerQuestDialog(_talker_id type, quest_id quest, _speech_id speech)
{
	if (static_cast<size_t>(type) >= TownerQuestDialogTable.size()) {
		return;
	}
	if (quest < 0 || quest >= MAXQUESTS) {
		return;
	}
	TownerQuestDialogTable[static_cast<size_t>(type)][quest] = speech;
}

} // namespace devilution
