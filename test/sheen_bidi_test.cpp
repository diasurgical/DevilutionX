
#include <string_view>

#include <gtest/gtest.h>

#include "utils/sheen_bidi.hpp"

namespace devilution {
namespace {

TEST(SheenBidiTest, AlgorithmCreated)
{
	sb::Algorithm algorithm = sb::Algorithm::create("Hello");
	EXPECT_TRUE(algorithm);
	EXPECT_NE(algorithm.get(), nullptr);
}

TEST(SheenBidiTest, CreateParagraphEnglish)
{
	std::string_view text = "Hello, World!";
	sb::Algorithm algorithm = sb::Algorithm::create(text);

	sb::Paragraph paragraph = algorithm.createParagraph(0, text.size(), SBLevelDefaultLTR);
	EXPECT_TRUE(paragraph);
}

TEST(SheenBidiTest, CreateParagraphHebrew)
{
	std::string_view text = "שלום עולם";
	sb::Algorithm algorithm = sb::Algorithm::create(text);

	sb::Paragraph paragraph = algorithm.createParagraph(0, text.size(), SBLevelDefaultRTL);
	EXPECT_TRUE(paragraph);
}

TEST(SheenBidiTest, CreateParagraphMixed)
{
	std::string_view text = "Hello שלום World";
	sb::Algorithm algorithm = sb::Algorithm::create(text);

	sb::Paragraph paragraph = algorithm.createParagraph(0, text.size(), SBLevelDefaultLTR);
	EXPECT_TRUE(paragraph);
}

TEST(SheenBidiTest, CreateLineFromParagraph)
{
	std::string_view text = "Hello, World!";
	sb::Algorithm algorithm = sb::Algorithm::create(text);

	sb::Paragraph paragraph = algorithm.createParagraph(0, text.size(), SBLevelDefaultLTR);
	EXPECT_TRUE(paragraph);

	sb::Line line = paragraph.createLine(0, text.size());
	EXPECT_TRUE(line);
}

TEST(SheenBidiTest, GetRunsFromLine)
{
	std::string_view text = "Hello, World!";
	sb::Algorithm algorithm = sb::Algorithm::create(text);

	sb::Paragraph paragraph = algorithm.createParagraph(0, text.size(), SBLevelDefaultLTR);
	sb::Line line = paragraph.createLine(0, text.size());

	SBUInteger runCount = line.runCount();
	EXPECT_GT(runCount, 0);

	const SBRun *runs = line.runsPtr();
	EXPECT_NE(runs, nullptr);
}

TEST(SheenBidiTest, IsRTLDetectsLTRRun)
{
	SBRun run;
	run.level = 0; // LTR (even level)

	EXPECT_FALSE(sb::IsRTL(run));
}

TEST(SheenBidiTest, IsRTLDetectsRTLRun)
{
	SBRun run;
	run.level = 1; // RTL (odd level)

	EXPECT_TRUE(sb::IsRTL(run));
}

TEST(SheenBidiTest, MixedLanguageAnalysis)
{
	std::string_view text = "Hello שלום World";
	sb::Algorithm algorithm = sb::Algorithm::create(text);

	sb::Paragraph paragraph = algorithm.createParagraph(0, text.size(), SBLevelDefaultLTR);
	EXPECT_TRUE(paragraph);

	sb::Line line = paragraph.createLine(0, text.size());
	EXPECT_TRUE(line);

	SBUInteger runCount = line.runCount();
	EXPECT_GT(runCount, 0);

	// We expect multiple runs due to the directional changes
	// (English -> Hebrew -> English)
	const SBRun *runs = line.runsPtr();
	EXPECT_NE(runs, nullptr);

	// Verify each run has valid level
	for (const SBRun &run : line.runs()) {
		EXPECT_GE(run.level, 0);
		EXPECT_LE(run.level, 125); // Max bidi level according to Unicode spec
	}
}

TEST(SheenBidiTest, ParagraphCopySemantics)
{
	std::string_view text = "Test";
	sb::Algorithm algorithm = sb::Algorithm::create(text);

	sb::Paragraph para1(algorithm.createParagraph(0, text.size(), SBLevelDefaultLTR));
	EXPECT_TRUE(para1);

	// Copy constructor
	sb::Paragraph para2 = para1;
	EXPECT_TRUE(para2);
	EXPECT_EQ(para1.get(), para2.get());
}

TEST(SheenBidiTest, ParagraphMoveSemantics)
{
	std::string_view text = "Test";
	sb::Algorithm algorithm = sb::Algorithm::create(text);

	sb::Paragraph para1(algorithm.createParagraph(0, text.size(), SBLevelDefaultLTR));
	EXPECT_TRUE(para1);

	// Move constructor
	sb::Paragraph para2 = std::move(para1);
	EXPECT_TRUE(para2);
	EXPECT_FALSE(para1);
	EXPECT_EQ(para1.get(), nullptr);
}

TEST(SheenBidiTest, LineCopySemantics)
{
	std::string_view text = "Test";
	sb::Algorithm algorithm = sb::Algorithm::create(text);

	sb::Paragraph paragraph = algorithm.createParagraph(0, text.size(), SBLevelDefaultLTR);
	sb::Line line1(SBParagraphCreateLine(paragraph.get(), 0, text.size()));
	EXPECT_TRUE(line1);

	// Copy constructor
	sb::Line line2 = line1;
	EXPECT_TRUE(line2);
	EXPECT_EQ(line1.get(), line2.get());
}

TEST(SheenBidiTest, LineMoveSemantics)
{
	std::string_view text = "Test";
	sb::Algorithm algorithm = sb::Algorithm::create(text);

	sb::Paragraph paragraph = algorithm.createParagraph(0, text.size(), SBLevelDefaultLTR);
	sb::Line line1(SBParagraphCreateLine(paragraph.get(), 0, text.size()));
	EXPECT_TRUE(line1);

	// Move constructor
	sb::Line line2 = std::move(line1);
	EXPECT_TRUE(line2);
	EXPECT_FALSE(line1);
	EXPECT_EQ(line1.get(), nullptr);
}

TEST(SheenBidiTest, AlgorithmPointerOperations)
{
	sb::Algorithm algorithm = sb::Algorithm::create("Hello");
	EXPECT_TRUE(algorithm);
	EXPECT_NE(algorithm.get(), nullptr);

	// Test nullptr comparison
	EXPECT_FALSE(algorithm == nullptr);
	EXPECT_TRUE(algorithm != nullptr);
	EXPECT_FALSE(nullptr == algorithm);
	EXPECT_TRUE(nullptr != algorithm);
}

TEST(SheenBidiTest, ParagraphPointerNullptr)
{
	sb::Paragraph paragraph;
	EXPECT_FALSE(paragraph);
	EXPECT_EQ(paragraph.get(), nullptr);
	EXPECT_TRUE(paragraph == nullptr);
}

TEST(SheenBidiTest, LinePointerNullptr)
{
	sb::Line line;
	EXPECT_FALSE(line);
	EXPECT_EQ(line.get(), nullptr);
	EXPECT_TRUE(line == nullptr);
}

} // namespace
} // namespace devilution
