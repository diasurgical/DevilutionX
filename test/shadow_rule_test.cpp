#include <gtest/gtest.h>

#include "engine/shadow_rule.hpp"

namespace devilution {
namespace {

TEST(ShadowRuleTest, ParsesTypes)
{
	EXPECT_EQ(ParseShadowRule(""), (ShadowRule { ShadowRuleType::Unknown, 0 }));
	EXPECT_EQ(ParseShadowRule("Unknown"), (ShadowRule { ShadowRuleType::Unknown, 0 }));
	EXPECT_EQ(ParseShadowRule("None"), (ShadowRule { ShadowRuleType::None, 0 }));
	EXPECT_EQ(ParseShadowRule("Baked"), (ShadowRule { ShadowRuleType::Baked, 0 }));
	EXPECT_EQ(ParseShadowRule("FirstFrame"), (ShadowRule { ShadowRuleType::FirstFrame, 0 }));
	EXPECT_EQ(ParseShadowRule("EachFrame"), (ShadowRule { ShadowRuleType::EachFrame, 0 }));
}

TEST(ShadowRuleTest, ParsesBaselineOffset)
{
	EXPECT_EQ(ParseShadowRule("FirstFrame+15"), (ShadowRule { ShadowRuleType::FirstFrame, 15 }));
	EXPECT_EQ(ParseShadowRule("FirstFrame+255"), (ShadowRule { ShadowRuleType::FirstFrame, 255 }));
}

TEST(ShadowRuleTest, ParsesTags)
{
	EXPECT_EQ(ParseShadowRule("FirstFrame|Stray"), (ShadowRule { ShadowRuleType::FirstFrame, 0, ShadowRuleFlags::Stray }));
	EXPECT_EQ(ParseShadowRule("EachFrame|Unexplained"), (ShadowRule { ShadowRuleType::EachFrame, 0, ShadowRuleFlags::Unexplained }));
	EXPECT_EQ(ParseShadowRule("FirstFrame|Unexplained|Stray"), (ShadowRule { ShadowRuleType::FirstFrame, 0, ShadowRuleFlags::Unexplained | ShadowRuleFlags::Stray }));
	EXPECT_EQ(ParseShadowRule("FirstFrame+15|Stray"), (ShadowRule { ShadowRuleType::FirstFrame, 15, ShadowRuleFlags::Stray }));

	const ShadowRule rule = *ParseShadowRule("FirstFrame|Stray");
	EXPECT_TRUE(HasAnyOf(rule.flags, ShadowRuleFlags::Stray));
	EXPECT_FALSE(HasAnyOf(rule.flags, ShadowRuleFlags::Unexplained));
	// The tags are notes: the shadow is extracted all the same.
	EXPECT_TRUE(rule.canExtractShadow());
	EXPECT_EQ(ParseShadowRule("FirstFrame")->flags, ShadowRuleFlags::None);
}

TEST(ShadowRuleTest, RejectsInvalidValues)
{
	EXPECT_FALSE(ParseShadowRule("firstframe").has_value());
	EXPECT_FALSE(ParseShadowRule("FirstFrame+").has_value());
	EXPECT_FALSE(ParseShadowRule("FirstFrame+0").has_value());
	EXPECT_FALSE(ParseShadowRule("FirstFrame+256").has_value());
	EXPECT_FALSE(ParseShadowRule("FirstFrame+1x").has_value());
	EXPECT_FALSE(ParseShadowRule("FirstFrame-1").has_value());
	EXPECT_FALSE(ParseShadowRule("EachFrame+1").has_value());
	EXPECT_FALSE(ParseShadowRule("FirstFrame|").has_value());
	EXPECT_FALSE(ParseShadowRule("FirstFrame|stray").has_value());
	EXPECT_FALSE(ParseShadowRule("FirstFrame|Stray|Other").has_value());
	EXPECT_FALSE(ParseShadowRule("Stray").has_value());
	// A comma separates the rules of a list, not a rule from its tags.
	EXPECT_FALSE(ParseShadowRule("FirstFrame,Stray").has_value());
}

TEST(ShadowRuleTest, CanExtractShadow)
{
	EXPECT_FALSE(ShadowRule {}.canExtractShadow());
	EXPECT_FALSE((ShadowRule { ShadowRuleType::None, 0 }).canExtractShadow());
	// A baked shadow is known but stays part of the sprite.
	EXPECT_FALSE((ShadowRule { ShadowRuleType::Baked, 0 }).canExtractShadow());
	EXPECT_TRUE((ShadowRule { ShadowRuleType::FirstFrame, 11 }).canExtractShadow());
	EXPECT_TRUE((ShadowRule { ShadowRuleType::EachFrame, 0 }).canExtractShadow());
}

} // namespace
} // namespace devilution
