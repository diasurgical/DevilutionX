#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "engine/assets.hpp"
#include "engine/load_shadow_fixes.hpp"
#include "engine/shadow_fix.hpp"
#include "utils/file_util.h"
#include "utils/paths.h"

using namespace devilution;

namespace {

class HasLooseLogicAssetsTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		savedOverridePaths_ = OverridePaths;
		savedAssetsPath_ = paths::AssetsPath();
		OverridePaths.clear();

		const auto *testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
		root_ = std::string("Test_") + testInfo->test_suite_name() + "_" + testInfo->name() + DIRECTORY_SEPARATOR_STR;
		assetsRoot_ = root_ + "fake_assets" DIRECTORY_SEPARATOR_STR;

		// A fake `/assets` directory providing the canonical assets that loose files can shadow.
		CreateFileAt(assetsRoot_ + "levels" DIRECTORY_SEPARATOR_STR "l1data" DIRECTORY_SEPARATOR_STR "l1.sol");
		CreateFileAt(assetsRoot_ + "txtdata" DIRECTORY_SEPARATOR_STR "towners" DIRECTORY_SEPARATOR_STR "towners.tsv");
		paths::SetAssetsPath(assetsRoot_);
	}

	void TearDown() override
	{
		for (const std::string &file : createdFiles_)
			RemoveFile(file.c_str());
		OverridePaths = savedOverridePaths_;
		paths::SetAssetsPath(savedAssetsPath_);
	}

	// Creates an empty file in the override root, creating parent directories as needed.
	void CreateFile(const std::string &relativePath)
	{
		CreateFileAt(root_ + relativePath);
	}

	std::string root_;

private:
	void CreateFileAt(const std::string &path)
	{
		const std::string dir { Dirname(path) };
		RecursivelyCreateDir(dir.c_str());
		FILE *file = OpenFile(path.c_str(), "wb");
		ASSERT_NE(file, nullptr);
		std::fclose(file);
		createdFiles_.push_back(path);
	}

	std::string assetsRoot_;
	std::vector<std::string> savedOverridePaths_;
	std::string savedAssetsPath_;
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
TEST_F(HasLooseLogicAssetsTest, LuaModEntryPoint)
{
	CreateFile("lua" DIRECTORY_SEPARATOR_STR "mods" DIRECTORY_SEPARATOR_STR "cheat" DIRECTORY_SEPARATOR_STR "init.lua");
	OverridePaths.push_back(root_);
	EXPECT_TRUE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, LuaOutsideLuaNamespaceIsInert)
{
	// The game only requests lua modules under `lua\`, so this file can never be loaded.
	CreateFile("cheat.lua");
	OverridePaths.push_back(root_);
	EXPECT_FALSE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, TsvShadowingExistingAsset)
{
	CreateFile("txtdata" DIRECTORY_SEPARATOR_STR "towners" DIRECTORY_SEPARATOR_STR "towners.tsv");
	OverridePaths.push_back(root_);
	EXPECT_TRUE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, SolShadowingExistingAsset)
{
	CreateFile("levels" DIRECTORY_SEPARATOR_STR "l1data" DIRECTORY_SEPARATOR_STR "l1.sol");
	OverridePaths.push_back(root_);
	EXPECT_TRUE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, BackupDirectoryIsInert)
{
	// A renamed directory does not shadow any asset the game requests.
	CreateFile("levels.bak" DIRECTORY_SEPARATOR_STR "l1data" DIRECTORY_SEPARATOR_STR "l1.sol");
	OverridePaths.push_back(root_);
	EXPECT_FALSE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, UppercaseLuaIsFlagged)
{
	CreateFile("LUA" DIRECTORY_SEPARATOR_STR "MODS" DIRECTORY_SEPARATOR_STR "CHEAT" DIRECTORY_SEPARATOR_STR "INIT.LUA");
	OverridePaths.push_back(root_);
	EXPECT_TRUE(HasLooseLogicAssets());
}

TEST_F(HasLooseLogicAssetsTest, InactiveLooseModsAreInert)
{
	// Inactive loose mods under `mods/` are not override roots and their paths are never
	// requested relative to the pref path. Active loose mods get their own override root,
	// under which their lua files sit at `lua/mods/<name>/...` and are caught by the lua rule.
	CreateFile("mods" DIRECTORY_SEPARATOR_STR "inactive" DIRECTORY_SEPARATOR_STR "lua" DIRECTORY_SEPARATOR_STR "mods" DIRECTORY_SEPARATOR_STR "inactive" DIRECTORY_SEPARATOR_STR "init.lua");
	OverridePaths.push_back(root_);
	EXPECT_FALSE(HasLooseLogicAssets());
}
#endif // !UNPACKED_MPQS

TEST(LoadShadowFixesTest, LoadsTheCorrectionsStoredNextToASpriteFile)
{
	// The cauldron is the one object whose shadow cannot be separated by a rule.
	const ShadowFixData fixes = LoadShadowFixes("objects\\cauldren");
	ASSERT_GT(fixes.size, ShadowFixHeaderSize);
	ShadowFixReader reader { fixes, 1 };
	ASSERT_TRUE(reader.ok());
	EXPECT_TRUE(reader.beginList(3));
	for (int i = 0; i < 3; ++i)
		EXPECT_FALSE(reader.nextFrame(96, 96).empty());
	EXPECT_TRUE(reader.ok());
}

TEST(LoadShadowFixesTest, HellfireSpritesHaveTheirCorrectionsInTheHellfireMod)
{
	EXPECT_EQ(LoadShadowFixes("plrgfx\\monk\\mha\\mhaat").size, 0U);
	LoadModArchives({ { "hf" } });
	EXPECT_GT(LoadShadowFixes("plrgfx\\monk\\mha\\mhaat").size, ShadowFixHeaderSize);
	UnloadModArchives();
}

TEST(LoadShadowFixesTest, SpriteFileWithoutCorrections)
{
	EXPECT_EQ(LoadShadowFixes("objects\\no_such_object").size, 0U);
}

} // namespace
