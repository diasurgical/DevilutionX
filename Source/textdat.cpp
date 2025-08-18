/**
 * @file textdat.cpp
 *
 * Implementation of all dialog texts.
 */
#include "textdat.h"

#include <ankerl/unordered_dense.h>
#include <fmt/core.h>
#include <magic_enum/magic_enum.hpp>

#include "data/file.hpp"
#include "data/record_reader.hpp"
#include "effects.h"
#include "utils/language.h"

template <>
struct magic_enum::customize::enum_range<devilution::_speech_id> {
	static constexpr int min = devilution::TEXT_NONE;
	static constexpr int max = devilution::NUM_DEFAULT_TEXT_IDS;
};

namespace devilution {

/* todo: move text out of struct */

/** Contains the data related to each speech ID. */
std::vector<Speech> Speeches;

/** Contains the mapping between text ID strings and indices, used for parsing additional text data. */
ankerl::unordered_dense::map<std::string, int16_t> AdditionalTextIdStringsToIndices;

tl::expected<_speech_id, std::string> ParseSpeechId(std::string_view value)
{
	if (value.empty()) {
		return TEXT_NONE;
	}

	const std::optional<_speech_id> enumValueOpt = magic_enum::enum_cast<_speech_id>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}

	const auto findIt = AdditionalTextIdStringsToIndices.find(std::string(value));
	if (findIt != AdditionalTextIdStringsToIndices.end()) {
		return static_cast<_speech_id>(findIt->second);
	}

	return tl::make_unexpected("Invalid value.");
}

namespace {

void LoadTextDatFromFile(DataFile &dataFile, std::string_view filename)
{
	dataFile.skipHeaderOrDie(filename);

	Speeches.reserve(Speeches.size() + dataFile.numRecords());

	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };

		std::string txtstrid;
		reader.readString("txtstrid", txtstrid);

		if (txtstrid.empty()) {
			continue;
		}

		const std::optional<_speech_id> speechIdEnumValueOpt = magic_enum::enum_cast<_speech_id>(txtstrid);

		if (!speechIdEnumValueOpt.has_value()) {
			const size_t textEntryIndex = Speeches.size();
			const auto [it, inserted] = AdditionalTextIdStringsToIndices.emplace(txtstrid, static_cast<int16_t>(textEntryIndex));
			if (!inserted) {
				DisplayFatalErrorAndExit(_("Loading Text Data Failed"), fmt::format(fmt::runtime(_("A text data entry already exists for ID \"{}\".")), txtstrid));
			}
		}

		// for hardcoded monsters, use their predetermined slot; for non-hardcoded ones, use the slots after that
		Speech &speech = speechIdEnumValueOpt.has_value() ? Speeches[speechIdEnumValueOpt.value()] : Speeches.emplace_back();

		reader.readString("txtstr", speech.txtstr);
		size_t pos = 0;
		while ((pos = speech.txtstr.find("\\n", pos)) != std::string::npos) {
			speech.txtstr.replace(pos, 2, "\n");
			pos += 1;
		}

		reader.readBool("scrlltxt", speech.scrlltxt);
		reader.read("sfxnr", speech.sfxnr, ParseSfxId);
	}
}

} // namespace

void LoadTextData()
{
	const std::string_view filename = "txtdata\\text\\textdat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);

	Speeches.clear();
	AdditionalTextIdStringsToIndices.clear();
	Speeches.resize(NUM_DEFAULT_TEXT_IDS); // ensure the hardcoded text entry slots are filled
	LoadTextDatFromFile(dataFile, filename);

	Speeches.shrink_to_fit();
}

} // namespace devilution
