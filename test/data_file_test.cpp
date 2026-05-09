#include <gtest/gtest.h>

#include "data/file.hpp"
#include "data/parser.hpp"

#include <string_view>
#include <vector>

#include "utils/paths.h"

namespace devilution {
auto LoadDataFile(std::string_view file)
{
	const std::string unitTestFolderCompletePath = paths::BasePath() + "/test/fixtures/";
	paths::SetAssetsPath(unitTestFolderCompletePath);
	return DataFile::load(file);
}

void TestFileContents(
    const DataFile &dataFile,
    std::vector<std::vector<std::string_view>> expectedContent,
    GetFieldResult::Status expectedEndOfRecordStatus = GetFieldResult::Status::EndOfRecord,
    GetFieldResult::Status expectedEndOfFileStatus = GetFieldResult::Status::EndOfFile)
{
	GetFieldResult result { dataFile.data() };
	const char *end = dataFile.data() + dataFile.size();
	unsigned row = 0;
	do {
		ASSERT_LT(row, expectedContent.size()) << "Too many records";
		unsigned col = 0;
		do {
			ASSERT_LT(col, expectedContent[row].size()) << "Too many fields in record " << row;
			result = GetNextField(result.next, end);
			EXPECT_EQ(result.value, expectedContent[row][col]) << "Unexpected value at record " << row << " and field " << col;
			col++;
		} while (!result.endOfRecord());
		if (!result.endOfFile()) {
			EXPECT_EQ(result.status, expectedEndOfRecordStatus) << "Unexpected status when parsing the end of record " << row;
		}

		EXPECT_EQ(col, expectedContent[row].size()) << "Parsing returned fewer fields than expected in record " << row;
		row++;
	} while (!result.endOfFile());
	EXPECT_EQ(result.status, expectedEndOfFileStatus) << "Unexpected status when parsing the end of the file";

	EXPECT_EQ(row, expectedContent.size()) << "Parsing returned fewer records than expected";
}

TEST(DataFileTest, TryLoadMissingFile)
{
	auto result = LoadDataFile("txtdata\\not_found.tsv");
	EXPECT_FALSE(result.has_value()) << "Trying to load a non-existent file should return an unexpected/error response";

	EXPECT_EQ(result.error(), DataFile::Error::NotFound) << "The error code should indicate the file was not found";
}

TEST(DataFileTest, LoadCRFile)
{
	auto result = LoadDataFile("txtdata\\cr.tsv");
	ASSERT_TRUE(result.has_value()) << "Unable to load cr.tsv";

	const DataFile &dataFile = result.value();
	EXPECT_EQ(dataFile.size(), 33) << "File size should be reported in code units";

	const std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" }
	};

	TestFileContents(dataFile, expectedFields, GetFieldResult::Status::BadRecordTerminator, GetFieldResult::Status::FileTruncated);
}

TEST(DataFileTest, LoadWindowsFile)
{
	auto result = LoadDataFile("txtdata\\crlf.tsv");
	ASSERT_TRUE(result.has_value()) << "Unable to load crlf.tsv";

	const DataFile &dataFile = result.value();
	EXPECT_EQ(dataFile.size(), 37) << "File size should be reported in code units";

	const std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	TestFileContents(dataFile, expectedFields);
}

TEST(DataFileTest, LoadTypicalFile)
{
	auto result = LoadDataFile("txtdata\\lf.tsv");
	ASSERT_TRUE(result.has_value()) << "Unable to load lf.tsv";

	const DataFile &dataFile = result.value();
	EXPECT_EQ(dataFile.size(), 33) << "File size should be reported in code units";

	const std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	TestFileContents(dataFile, expectedFields);
}

TEST(DataFileTest, LoadFileWithNoTrailingNewline)
{
	auto result = LoadDataFile("txtdata\\lf_no_trail.tsv");
	ASSERT_TRUE(result.has_value()) << "Unable to load lf_no_trail.tsv";

	const DataFile &dataFile = result.value();
	EXPECT_EQ(dataFile.size(), 32) << "File size should be reported in code units";

	const std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	TestFileContents(dataFile, expectedFields, GetFieldResult::Status::EndOfRecord, GetFieldResult::Status::NoFinalTerminator);
}

std::string_view mapError(DataFile::Error error)
{
	switch (error) {
	case DataFile::Error::NotFound:
		return "not found";
	case DataFile::Error::OpenFailed:
		return "cannot open";
	case DataFile::Error::BadRead:
		return "cannot read contents";
	default:
		return "unexpected error";
	}
}

TEST(DataFileTest, LoadEmptyFile)
{
	auto result = LoadDataFile("txtdata\\empty.tsv");
	if (!result.has_value()) {
		FAIL() << "Unable to load empty.tsv, error: " << mapError(result.error());
	}

	const DataFile &dataFile = result.value();
	EXPECT_EQ(dataFile.size(), 0) << "File size should be reported in code units";

	const std::vector<std::vector<std::string_view>> expectedFields {
		{ "" },
	};

	TestFileContents(dataFile, expectedFields, GetFieldResult::Status::NoFinalTerminator, GetFieldResult::Status::NoFinalTerminator);
}

TEST(DataFileTest, LoadEmptyFileWithBOM)
{
	auto result = LoadDataFile("txtdata\\empty_with_utf8_bom.tsv");
	if (!result.has_value()) {
		FAIL() << "Unable to load empty_with_utf8_bom.tsv, error: " << mapError(result.error());
	}

	const DataFile &dataFile = result.value();
	EXPECT_EQ(dataFile.size(), 0) << "Loading a file containing a UTF8 byte order marker should strip that prefix";

	const std::vector<std::vector<std::string_view>> expectedFields {
		{ "" },
	};

	TestFileContents(dataFile, expectedFields, GetFieldResult::Status::NoFinalTerminator, GetFieldResult::Status::NoFinalTerminator);
}

TEST(DataFileTest, LoadUtf8WithBOM)
{
	auto result = LoadDataFile("txtdata\\utf8_bom.tsv");
	ASSERT_TRUE(result.has_value()) << "Unable to load utf8_bom.tsv";

	const DataFile &dataFile = result.value();
	EXPECT_EQ(dataFile.size(), 33) << "Loading a file containing a UTF8 byte order marker should strip that prefix";

	const std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	TestFileContents(dataFile, expectedFields);
}

TEST(DataFileTest, ParseInt)
{
	auto result = LoadDataFile("txtdata\\sample.tsv");
	if (!result.has_value()) {
		FAIL() << "Unable to load sample.tsv, error: " << mapError(result.error());
	}

	DataFile &dataFile = result.value();
	auto unused = dataFile.parseHeader(nullptr, nullptr, [](std::string_view) -> tl::expected<uint8_t, ColumnDefinition::Error> { return tl::unexpected { ColumnDefinition::Error::UnknownColumn }; });

	EXPECT_TRUE(unused.has_value()) << "Should be able to parse and discard the header from the sample.tsv file";

	for (DataFileRecord record : dataFile) {
		auto fieldIt = record.begin();
		auto end = record.end();

		ASSERT_NE(fieldIt, end) << "sample.tsv must contain at least one field to use as a test value for strings";

		// First field is a string that doesn't start with a digit or - character
		DataFileField field = *fieldIt;
		uint8_t shortVal = 5;
		auto parseIntResult = field.parseInt(shortVal);
		if (parseIntResult.has_value()) {
			ADD_FAILURE() << "Parsing a string as an int should not succeed";
		} else {
			EXPECT_EQ(parseIntResult.error(), DataFileField::Error::NotANumber) << "Strings are not uint8_t values";
		}
		EXPECT_EQ(shortVal, 5) << "Value is not modified when parsing as uint8_t fails due to non-numeric fields";
		EXPECT_EQ(*field, "Sample") << "Should be able to access the field value as a string after failure";
		++fieldIt;

		ASSERT_NE(fieldIt, end) << "sample.tsv must contain a second field to use as a test value for small ints";

		// Second field is a number that fits into an uint8_t value
		field = *fieldIt;
		shortVal = 5;
		parseIntResult = field.parseInt(shortVal);
		EXPECT_TRUE(parseIntResult.has_value()) << "Expected " << field << " to fit into a uint8_t variable";
		EXPECT_EQ(shortVal, 145) << "Parsing should give the expected base 10 value";
		EXPECT_EQ(*field, "145") << "Should be able to access the field value as a string even after parsing as an int";

		int longVal = 1;
		auto parseFixedResult = field.parseFixed6(longVal);
		EXPECT_TRUE(parseFixedResult.has_value()) << "Expected " << field << " to be parsed as a fixed point integer with only the integer part";
		EXPECT_EQ(longVal, 145 << 6) << "Parsing should give the expected fixed point base 10 value";

		++fieldIt;

		ASSERT_NE(fieldIt, end) << "sample.tsv must contain a third field to use as a test value for large ints";

		// Third field is a number too large for a uint8_t but that fits into an int value
		field = *fieldIt;
		parseIntResult = field.parseInt(shortVal);
		if (parseIntResult.has_value()) {
			ADD_FAILURE() << "Parsing an int into a short variable should not succeed";
		} else {
			EXPECT_EQ(parseIntResult.error(), DataFileField::Error::OutOfRange) << "A value too large to fit into a uint8_t variable should report an error";
		}
		EXPECT_EQ(shortVal, 145) << "Value is not modified when parsing as uint8_t fails due to out of range value";
		longVal = 42;
		parseIntResult = field.parseInt(longVal);
		EXPECT_TRUE(parseIntResult.has_value()) << "Expected " << field << " to fit into an int variable";
		EXPECT_EQ(longVal, 70322) << "Value is expected to be parsed into a larger type after an out of range failure";
		EXPECT_EQ(*field, "70322") << "Should be able to access the field value as a string after parsing as an int";
		++fieldIt;

		ASSERT_NE(fieldIt, end) << "sample.tsv must contain a fourth field to use as a test value for fields that look like ints";

		// Fourth field is not an integer, but a value that starts with one or more digits that fit into an uint8_t value
		field = *fieldIt;
		parseIntResult = field.parseInt(shortVal);
		EXPECT_TRUE(parseIntResult.has_value()) << "Expected " << field << " to fit into a uint8_t variable (even though it's not really an int)";
		EXPECT_EQ(shortVal, 6) << "Value is loaded as expected until the first non-digit character";
		EXPECT_EQ(*field, "6.34") << "Should be able to access the field value as a string after parsing as an int";
		int fixedVal = 64;
		parseFixedResult = field.parseFixed6(fixedVal);
		EXPECT_TRUE(parseFixedResult.has_value()) << "Expected " << field << " to be parsed as a fixed point value";
		// 6.34 is parsed as 384 (6<<6) + 22 (0.34 rounds to 0.34375, 22/64)
		EXPECT_EQ(fixedVal, 406) << "Value is loaded as a fixed point number";

		uint8_t shortFixedVal = 32;
		parseFixedResult = field.parseFixed6(shortFixedVal);
		EXPECT_FALSE(parseFixedResult.has_value()) << "Expected " << field << " to fail to parse into a 2.6 fixed point variable";
		EXPECT_EQ(parseFixedResult.error(), DataFileField::Error::OutOfRange) << "A value too large to fit into a 2 bit integer part should report an error";
		EXPECT_EQ(shortFixedVal, 32) << "The variable should not be modified when parsing fails";

		++fieldIt;

		ASSERT_NE(fieldIt, end) << "sample.tsv must contain a fifth field to use as a test value for fixed point overflow";

		field = *fieldIt;
		parseFixedResult = field.parseFixed6(shortFixedVal);
		EXPECT_FALSE(parseFixedResult.has_value()) << "Expected " << field << " to fail to parse into a 2.6 fixed point variable";
		EXPECT_EQ(parseFixedResult.error(), DataFileField::Error::OutOfRange) << "A value that after rounding is too large to fit into a 2 bit integer part should report an error";
		EXPECT_EQ(shortFixedVal, 32) << "The variable should not be modified when parsing fails";
	}
}

TEST(DataFileTest, IterateOverRecords)
{
	auto result = LoadDataFile("txtdata\\lf.tsv");
	ASSERT_TRUE(result.has_value()) << "Unable to load lf.tsv";

	const DataFile &dataFile = result.value();

	std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	unsigned row = 0;
	for (DataFileRecord record : dataFile) {
		ASSERT_LT(row, expectedFields.size()) << "Too many records";
		unsigned col = 0;
		for (DataFileField field : record) {
			if (col < expectedFields[row].size())
				EXPECT_EQ(*field, expectedFields[row][col]) << "Unexpected value at record " << row << " and field " << col;
			else
				ADD_FAILURE() << "Extra value '" << field << "' in record " << row << " at field " << col;
			col++;
		}
		EXPECT_GE(col, expectedFields[row].size()) << "Parsing returned fewer fields than expected in record " << row;
		row++;
	}
	EXPECT_EQ(row, expectedFields.size()) << "Parsing returned fewer records than expected";
}

TEST(DataFileTest, ParseHeaderThenIterateOverRecords)
{
	auto result = LoadDataFile("txtdata\\lf.tsv");
	ASSERT_TRUE(result.has_value()) << "Unable to load lf.tsv";

	DataFile &dataFile = result.value();

	std::vector<std::vector<std::string_view>> expectedFields {
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	auto parseHeaderResult = dataFile.parseHeader(nullptr, nullptr, [](std::string_view) -> tl::expected<uint8_t, ColumnDefinition::Error> { return tl::unexpected { ColumnDefinition::Error::UnknownColumn }; });
	EXPECT_TRUE(parseHeaderResult.has_value()) << "Expected to be able to parse and discard the header record";

	unsigned row = 0;
	for (DataFileRecord record : dataFile) {
		ASSERT_LT(row, expectedFields.size()) << "Too many records";
		EXPECT_EQ(row + 1, record.row()) << "DataFileRecord (through iterator) should report a 1-indexed row after parsing the header record";
		unsigned col = 0;
		for (DataFileField field : record) {
			EXPECT_EQ(record.row(), field.row()) << "Field should report the same row as the current DataFileRecord";
			EXPECT_EQ(col, field.column()) << "Field (through iterator) should report a 0-indexed column";
			if (col < expectedFields[row].size())
				EXPECT_EQ(*field, expectedFields[row][col]) << "Unexpected value at record " << row << " and field " << col;
			else
				ADD_FAILURE() << "Extra value '" << field << "' in record " << row << " at field " << col;
			col++;
		}
		EXPECT_GE(col, expectedFields[row].size()) << "Parsing returned fewer fields than expected in record " << row;
		row++;
	}
	EXPECT_EQ(row, expectedFields.size()) << "Parsing returned fewer records than expected";
}

TEST(DataFileTest, DiscardAllAfterFirstField)
{
	auto loadDataResult = LoadDataFile("txtdata\\lf.tsv");
	ASSERT_TRUE(loadDataResult.has_value()) << "Unable to load lf.tsv";

	const DataFile &dataFile = loadDataResult.value();

	std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	GetFieldResult result { dataFile.data() };
	const char *end = dataFile.data() + dataFile.size();
	unsigned row = 0;
	do {
		ASSERT_LT(row, expectedFields.size()) << "Too many records";
		result = GetNextField(result.next, end);
		EXPECT_EQ(result.value, expectedFields[row][0]) << "Unexpected first value at record " << row;

		if (result.endOfRecord() && !result.endOfFile())
			ADD_FAILURE() << "Parsing returned fewer fields than expected in record " << row;
		else
			result = DiscardRemainingFields(result.next, end);
		row++;
	} while (!result.endOfFile());

	EXPECT_EQ(row, expectedFields.size()) << "Parsing returned fewer records than expected";
}

TEST(DataFileTest, DiscardAllAfterFirstFieldIterator)
{
	auto result = LoadDataFile("txtdata\\lf.tsv");
	ASSERT_TRUE(result.has_value()) << "Unable to load lf.tsv";

	const DataFile &dataFile = result.value();

	std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	unsigned row = 0;
	for (DataFileRecord record : dataFile) {
		ASSERT_LT(row, expectedFields.size()) << "Too many records";
		for (DataFileField field : record) {
			EXPECT_EQ(*field, expectedFields[row][0]) << "Field with value " << field << " does not match the expected value for record " << row;
			break;
		}
		row++;
	}
}

TEST(DataFileTest, DiscardAllUpToLastField)
{
	auto loadDataResult = LoadDataFile("txtdata\\lf.tsv");
	ASSERT_TRUE(loadDataResult.has_value()) << "Unable to load lf.tsv";

	const DataFile &dataFile = loadDataResult.value();

	std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	GetFieldResult result { dataFile.data() };
	const char *const end = dataFile.data() + dataFile.size();
	unsigned row = 0;
	do {
		ASSERT_LT(row, expectedFields.size()) << "Too many records";
		result = DiscardMultipleFields(result.next, end, 2);
		if (row < expectedFields.size()) {
			EXPECT_FALSE(result.endOfRecord()) << "Parsing returned fewer fields than expected in record " << row;
			if (!result.endOfRecord())
				result = GetNextField(result.next, end);
		}
		EXPECT_EQ(result.value, expectedFields[row][2]) << "Unexpected last value at record " << row;

		if (!result.endOfRecord()) {
			ADD_FAILURE() << "Parsing returned fewer fields than expected in record " << row;
			result = DiscardRemainingFields(result.next, end);
		}
		row++;
	} while (!result.endOfFile());

	EXPECT_EQ(row, expectedFields.size()) << "Parsing returned fewer records than expected";
}

TEST(DataFileTest, SkipFieldIterator)
{
	auto loadDataResult = LoadDataFile("txtdata\\lf.tsv");
	ASSERT_TRUE(loadDataResult.has_value()) << "Unable to load lf.tsv";

	const DataFile &dataFile = loadDataResult.value();

	std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	unsigned row = 0;
	for (DataFileRecord record : dataFile) {
		ASSERT_LT(row, expectedFields.size()) << "Too many records";
		auto fieldIt = record.begin();
		auto endField = record.end();
		fieldIt += 0;
		EXPECT_EQ((*fieldIt).value(), expectedFields[row][0]) << "Advancing a field iterator by 0 should not discard any values";

		fieldIt += 2;
		if (row < expectedFields.size()) {
			if (fieldIt == endField) {
				ADD_FAILURE() << "Parsing returned fewer fields than expected in record " << row;
			} else {
				EXPECT_EQ((*fieldIt).value(), expectedFields[row][2]) << "Unexpected last value at record " << row;
				++fieldIt;
			}
		}
		EXPECT_EQ(fieldIt, endField) << "Parsing returned more fields than expected in record " << row;
		EXPECT_EQ(fieldIt.column(), expectedFields[row].size() - 1) << "Field iterator should report the index of the last column";

		++row;
	}

	EXPECT_EQ(row, expectedFields.size()) << "Parsing returned fewer records than expected";
}

TEST(DataFileTest, SkipRowIterator)
{
	auto loadDataResult = LoadDataFile("txtdata\\lf.tsv");
	ASSERT_TRUE(loadDataResult.has_value()) << "Unable to load lf.tsv";

	const DataFile &dataFile = loadDataResult.value();

	std::vector<std::vector<std::string_view>> expectedFields {
		{ "Test", "Empty", "Values" },
		{ "", "2", "3" },
		{ "1", "2", "" },
		{ "1", "", "3" },
	};

	auto recordIt = dataFile.begin();
	auto endRecord = dataFile.end();
	recordIt += 0;
	EXPECT_EQ((*(*recordIt).begin()).value(), expectedFields[0][0]) << "Advancing a record iterator by 0 should not discard any values";
	recordIt += 2;
	unsigned row = 2;
	while (recordIt != endRecord) {
		ASSERT_LT(row, expectedFields.size()) << "Too many records";
		unsigned col = 0;
		for (DataFileField field : *recordIt) {
			if (col < expectedFields[row].size())
				EXPECT_EQ(*field, expectedFields[row][col]) << "Unexpected value at record " << row << " and field " << col;
			else
				ADD_FAILURE() << "Extra value '" << field << "' in record " << row << " at field " << col;
			col++;
		}
		EXPECT_GE(col, expectedFields[row].size()) << "Parsing returned fewer fields than expected in record " << row;
		++row;
		++recordIt;
	}

	EXPECT_EQ(row, expectedFields.size()) << "Parsing returned fewer records than expected";
}

} // namespace devilution
