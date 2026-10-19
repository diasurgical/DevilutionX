#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "engine/assets.hpp"
#include "diablo.h"
#include "mods/mod_identity.h"
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
class LooseModIdentityTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		savedPrefPath_ = paths::PrefPath();
		savedBasePath_ = paths::BasePath();
		savedOverrides_ = OverridePaths;
		savedIdentifiers_ = ActiveModIdentifiers;
		const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
		root_ = std::string("Test_LooseModIdentity_") + info->name() + DIRECTORY_SEPARATOR_STR;
		paths::SetPrefPath(root_);
		paths::SetBasePath(root_ + "base");
		OverridePaths.clear();
	}

	void TearDown() override
	{
		for (const std::string &file : files_)
			RemoveFile(file.c_str());
		paths::SetPrefPath(savedPrefPath_);
		paths::SetBasePath(savedBasePath_);
		OverridePaths = std::move(savedOverrides_);
		ActiveModIdentifiers = std::move(savedIdentifiers_);
	}

	void Write(const std::string &relative, std::string_view content)
	{
		const std::string path = root_ + relative;
		const std::string dir { Dirname(path) };
		RecursivelyCreateDir(dir.c_str());
		FILE *file = OpenFile(path.c_str(), "wb");
		ASSERT_NE(file, nullptr);
		EXPECT_EQ(std::fwrite(content.data(), 1, content.size(), file), content.size());
		std::fclose(file);
		files_.push_back(path);
	}

private:
	std::string root_;
	std::string savedPrefPath_;
	std::string savedBasePath_;
	std::vector<std::string> files_;
	std::vector<std::string> savedOverrides_;
	std::vector<ModIdentifier> savedIdentifiers_;
};

TEST_F(LooseModIdentityTest, ManifestIsolatesSavesWithoutWhitelistingLooseLogic)
{
	Write("mods/loose_identity_test/manifest.ini", "[mod]\nsaveExtension=asv\nprogramId=ABYS\n");
	Write("mods/loose_identity_test/lua/mods/loose_identity_test/init.lua", "return {}\n");
	LoadModArchives({ { "loose_identity_test" } });

	ASSERT_EQ(ActiveModIdentifiers.size(), 1u);
	EXPECT_EQ(GetActiveModSaveExtension(), "asv");
	EXPECT_EQ(GetGameId(), 0x41425953U);
	EXPECT_FALSE(ActiveModIdentifiers[0].whitelisted);
	EXPECT_TRUE(HasLooseLogicAssets());
	EXPECT_FALSE(SatisfiesRequiredIdentifier(ActiveModIdentifiers[0], ActiveModIdentifiers[0].hash));

	LoadModArchives({});
	EXPECT_TRUE(GetActiveModSaveExtension().empty());
	EXPECT_TRUE(ActiveModIdentifiers.empty());
}

TEST_F(LooseModIdentityTest, PreferenceManifestMatchesLooseAssetPrecedence)
{
	Write("mods/loose_identity_test/manifest.ini", "[mod]\nsaveExtension=asv\nprogramId=ABYS\n");
	Write("base/mods/loose_identity_test/manifest.ini", "[mod]\nsaveExtension=bsv\nprogramId=BASE\n");
	LoadModArchives({ { "loose_identity_test" } });
	ASSERT_EQ(ActiveModIdentifiers.size(), 1u);
	EXPECT_EQ(GetActiveModSaveExtension(), "asv");
	EXPECT_EQ(GetGameId(), 0x41425953U);
}

TEST_F(LooseModIdentityTest, MissingManifestStillUsesModSaveNamespace)
{
	Write("mods/loose_identity_test/lua/mods/loose_identity_test/init.lua", "return {}\n");
	LoadModArchives({ { "loose_identity_test" } });
	ASSERT_EQ(ActiveModIdentifiers.size(), 1u);
	EXPECT_EQ(GetActiveModSaveExtension(), "msv");
	EXPECT_FALSE(ActiveModIdentifiers[0].whitelisted);
}

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

} // namespace
