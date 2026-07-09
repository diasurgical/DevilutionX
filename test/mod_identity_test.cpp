#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include <picosha2.h>

#include "mods/mod_identity.h"

using namespace devilution;

namespace {

std::string TmpPath(const char *suffix)
{
	const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
	return std::string("Test_") + info->test_case_name() + '_' + info->name() + suffix;
}

void WriteFile(const std::string &path, const void *data, size_t size)
{
	FILE *file = std::fopen(path.c_str(), "wb");
	ASSERT_TRUE(file != nullptr);
	if (size > 0) {
		ASSERT_EQ(std::fwrite(data, size, 1, file), 1u);
	}
	std::fclose(file);
}

TEST(ModIdentity, HashesKnownVectors)
{
	// SHA-256("abc")
	const std::string abcPath = TmpPath("_abc");
	WriteFile(abcPath, "abc", 3);
	std::array<uint8_t, 32> hash;
	ASSERT_TRUE(ComputeFileSha256(abcPath.c_str(), hash));
	EXPECT_EQ(ModHashToHex(hash), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

	// SHA-256("") — empty file.
	const std::string emptyPath = TmpPath("_empty");
	WriteFile(emptyPath, "", 0);
	ASSERT_TRUE(ComputeFileSha256(emptyPath.c_str(), hash));
	EXPECT_EQ(ModHashToHex(hash), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(ModIdentity, ChunkedReadMatchesOneShot)
{
	// Larger than the 32 KiB internal read buffer, to exercise the chunking loop and
	// verify it agrees with a single in-memory hash of the same bytes.
	std::vector<uint8_t> data(100000);
	for (size_t i = 0; i < data.size(); ++i)
		data[i] = static_cast<uint8_t>((i * 31 + 7) & 0xFF);

	const std::string path = TmpPath("_big");
	WriteFile(path, data.data(), data.size());

	std::array<uint8_t, 32> fileHash;
	ASSERT_TRUE(ComputeFileSha256(path.c_str(), fileHash));

	std::array<uint8_t, 32> memHash;
	picosha2::hash256(data.begin(), data.end(), memHash.begin(), memHash.end());

	EXPECT_EQ(fileHash, memHash);
}

TEST(ModIdentity, MissingFileFails)
{
	std::array<uint8_t, 32> hash;
	EXPECT_FALSE(ComputeFileSha256("this-file-should-not-exist.mpq", hash));
}

TEST(ModIdentity, EmptyWhitelistRejectsEverything)
{
	std::array<uint8_t, 32> hash = {};
	EXPECT_FALSE(IsHashWhitelisted(hash));
	hash[0] = 0xAB;
	EXPECT_FALSE(IsHashWhitelisted(hash));
}

TEST(ModIdentity, RegisterAndClear)
{
	ClearModIdentifiers();
	ASSERT_TRUE(ActiveModIdentifiers.empty());

	const std::string path = TmpPath("_mod");
	WriteFile(path, "abc", 3);

	RegisterBuiltinModIdentifier("clock");
	RegisterPackedModIdentifier("external", path.c_str());

	ASSERT_EQ(ActiveModIdentifiers.size(), 2u);

	// Built-in: provenance-whitelisted, zero hash.
	EXPECT_EQ(ActiveModIdentifiers[0].name, "clock");
	EXPECT_TRUE(ActiveModIdentifiers[0].whitelisted);
	EXPECT_EQ(ActiveModIdentifiers[0].hash, (std::array<uint8_t, 32> {}));

	// Packed: hashed, not whitelisted (empty approved list).
	EXPECT_EQ(ActiveModIdentifiers[1].name, "external");
	EXPECT_FALSE(ActiveModIdentifiers[1].whitelisted);
	EXPECT_EQ(ModHashToHex(ActiveModIdentifiers[1].hash), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

	ClearModIdentifiers();
	EXPECT_TRUE(ActiveModIdentifiers.empty());
}

} // namespace
