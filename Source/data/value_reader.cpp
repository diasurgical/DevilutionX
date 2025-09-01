#include "data/value_reader.hpp"

#include <fmt/core.h>

#include "appfat.h"

namespace devilution {

ValueReader::ValueReader(DataFile &dataFile, std::string_view filename)
    : it_(dataFile.begin())
    , end_(dataFile.end())
    , filename_(filename)
{
}

DataFileField ValueReader::getValueField(std::string_view expectedKey)
{
	if (it_ == end_) {
		app_fatal(fmt::format("Missing field {} in {}", expectedKey, filename_));
	}
	DataFileRecord record = *it_;
	FieldIterator fieldIt = record.begin();
	const FieldIterator endField = record.end();

	const std::string_view key = (*fieldIt).value();
	if (key != expectedKey) {
		app_fatal(fmt::format("Unexpected field in {}: got {}, expected {}", filename_, key, expectedKey));
	}

	++fieldIt;
	if (fieldIt == endField) {
		DataFile::reportFatalError(DataFile::Error::NotEnoughColumns, filename_);
	}
	return *fieldIt;
}

} // namespace devilution
