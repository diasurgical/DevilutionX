#include "utils/ini.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string_view>

namespace devilution {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;

std::string ReplaceNewlines(std::string_view s)
{
	std::string out;
	bool prevR = false;
	for (const char c : s) {
		if (c == '\r') {
			prevR = true;
		} else {
			if (c == '\n' && !prevR) out += '\r';
			prevR = false;
		}
		out += c;
	}
	return out;
}

} // namespace

TEST(IniTest, BasicTest)
{
	std::expected<Ini, std::string> result = Ini::parse(R"(
; Section A comment
[sectionA]
key1 = value1
key2 = value2
; comment multi 1
multi = a
; comment multi 2
multi = b
int=-3
float=2.5
; bool yes comment
bool yes=1
bool no=0

; Section B comment line 1
; Section B comment line 2
[sectionB]
key = value
)");

	ASSERT_TRUE(result.has_value()) << result.error();
	EXPECT_EQ(result->getString("sectionA", "key1"), "value1");
	EXPECT_EQ(result->getString("sectionA", "key2"), "value2");
	{
		const std::span<const Ini::Value> multiVals = result->get("sectionA", "multi");
		std::vector<std::string> multiStrs;
		for (const Ini::Value &val : multiVals) {
			multiStrs.push_back(val.value);
		}
		EXPECT_THAT(multiStrs, ElementsAre(Eq("a"), Eq("b")));
	}
	EXPECT_EQ(result->getInt("sectionA", "int", 0), -3);
	EXPECT_NEAR(result->getFloat("sectionA", "float", 0.0f), 2.5f, 0.001f);
	EXPECT_EQ(result->getString("sectionB", "key"), "value");

	result->set("newSection", "newKey", "hello");
	result->set("sectionA", "key1", "newValue");
	result->set("sectionA", "int", 1337);
	result->set("sectionA", "bool yes", false);
	const std::vector<std::string> newMulti { "x", "y", "z" };
	result->set("sectionA", "multi", newMulti);
	result->set("sectionA", "float", 10.5F);
	EXPECT_EQ(result->serialize(), ReplaceNewlines(R"(; Section A comment
[sectionA]
key1=newValue
key2=value2
; comment multi 1
multi=x
; comment multi 2
multi=y
multi=z
int=1337
float=10.5
; bool yes comment
bool yes=0
bool no=0

; Section B comment line 1
; Section B comment line 2
[sectionB]
key=value

[newSection]
newKey=hello
)"));
}
} // namespace devilution
