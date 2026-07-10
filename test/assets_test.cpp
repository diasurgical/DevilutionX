#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "engine/assets.hpp"
#include "utils/file_util.h"
#include "utils/paths.h"

using namespace devilution;

namespace {

class HasLooseLogicAssetsTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		savedOverridePaths_ = OverridePaths;
		savedPrefPath_ = paths::PrefPath();
		OverridePaths.clear();

		const auto *testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
		root_ = std::string("Test_") + testInfo->test_suite_name() + "_" + testInfo->name() + DIRECTORY_SEPARATOR_STR;
		RecursivelyCreateDir((root_ + "sub" DIRECTORY_SEPARATOR_STR "nested").c_str());
		RecursivelyCreateDir((root_ + "mods" DIRECTORY_SEPARATOR_STR "inactive").c_str());
	}

	void TearDown() override
	{
		for (const std::string &file : createdFiles_)
			RemoveFile(file.c_str());
		OverridePaths = savedOverridePaths_;
		paths::SetPrefPath(savedPrefPath_);
	}

	void CreateFile(const std::string &relativePath)
	{
		const std::string path = root_ + relativePath;
		FILE *file = OpenFile(path.c_str(), "wb");
		ASSERT_NE(file, nullptr);
		std::fclose(file);
		createdFiles_.push_back(path);
	}

	std::string root_;

private:
	std::vector<std::string> savedOverridePaths_;
	std::string savedPrefPath_;
	std::vector<std::string> createdFiles_;
};

TEST_F(HasLooseLogicAssetsTest, NoOverridePaths)
{
	EXPECT_FALSE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, OnlyNonLogicFiles)
{
	CreateFile("readme.txt");
	CreateFile("sub" DIRECTORY_SEPARATOR_STR "sprite.clx");
	CreateFile("tsv"); // Extension-less name shorter than the extensions being matched.
	OverridePaths.push_back(root_);
	EXPECT_FALSE(HasLooseLogicAssets());
}

#ifndef UNPACKED_MPQS
TEST_F(HasLooseLogicAssetsTest, LuaAtRoot)
{
	CreateFile("cheat.lua");
	OverridePaths.push_back(root_);
	EXPECT_TRUE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, TsvInNestedDirectory)
{
	CreateFile("sub" DIRECTORY_SEPARATOR_STR "nested" DIRECTORY_SEPARATOR_STR "towners.tsv");
	OverridePaths.push_back(root_);
	EXPECT_TRUE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, SolInNestedDirectory)
{
	CreateFile("sub" DIRECTORY_SEPARATOR_STR "spelldat.sol");
	OverridePaths.push_back(root_);
	EXPECT_TRUE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, UppercaseExtension)
{
	CreateFile("CHEAT.LUA");
	OverridePaths.push_back(root_);
	EXPECT_TRUE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, ModsSubdirectoryOfPrefPathIsSkipped)
{
	CreateFile("mods" DIRECTORY_SEPARATOR_STR "inactive" DIRECTORY_SEPARATOR_STR "init.lua");
	paths::SetPrefPath(root_);
	OverridePaths.push_back(paths::PrefPath());
	EXPECT_FALSE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, ModsSubdirectoryOfOtherRootsIsScanned)
{
	CreateFile("mods" DIRECTORY_SEPARATOR_STR "inactive" DIRECTORY_SEPARATOR_STR "init.lua");
	OverridePaths.push_back(root_);
	EXPECT_TRUE(HasLooseLogicAssets());
}
#endif // !UNPACKED_MPQS

} // namespace
