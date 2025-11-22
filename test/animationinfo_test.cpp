#include <iomanip>

#include <gtest/gtest.h>

#include "engine/animationinfo.h"
#include "nthread.h"

using namespace devilution;

enum class TestDataType {
	SetNewAnimation,
	GameTick,
	Rendering
};

/**
 * @brief Represents a Action in the game_logic or rendering.
 */
struct TestData {
	virtual ~TestData() = default;
	virtual TestDataType type() const = 0;
};

/**
 * @brief Represents a call to setNewAnimation
 */
struct SetNewAnimationData : public TestData {
	SetNewAnimationData(int numberOfFrames, int delayLen, AnimationDistributionFlags params = AnimationDistributionFlags::None, int numSkippedFrames = 0, int distributeFramesBeforeFrame = 0)
	{
		_NumberOfFrames = numberOfFrames;
		_DelayLen = delayLen;
		_Params = params;
		_NumSkippedFrames = numSkippedFrames;
		_DistributeFramesBeforeFrame = distributeFramesBeforeFrame;
	}

	TestDataType type() const override
	{
		return TestDataType::SetNewAnimation;
	}

	int _NumberOfFrames;
	int _DelayLen;
	AnimationDistributionFlags _Params;
	int _NumSkippedFrames;
	int _DistributeFramesBeforeFrame;
};

/**
 * @brief Represents a GameTick, this includes skipping of Frames (for example because of Fastest Attack Modifier) and processAnimation (which also updates Animation Frame/Count).
 */
struct GameTickData : TestData {
	int _ExpectedAnimationFrame;
	int _ExpectedAnimationCnt;
	GameTickData(int expectedAnimationFrame, int expectedAnimationCnt)
	{
		_ExpectedAnimationFrame = expectedAnimationFrame;
		_ExpectedAnimationCnt = expectedAnimationCnt;
	}

	TestDataType type() const override
	{
		return TestDataType::GameTick;
	}
};

/**
 * @brief Represents a rendering/drawing Action.
 * This happens directly after the game_logic (_fProgressToNextGameTick = 0) or if no GameTick is due between two GameTicks (_fProgressToNextGameTick != 0).
 */
struct RenderingData : TestData {
	/**
	 * @brief the progress as a fraction (0.0f to 1.0f) in time to the next game tick
	 */
	float _fProgressToNextGameTick;
	int _ExpectedRenderingFrame;
	RenderingData(float fProgressToNextGameTick, int expectedRenderingFrame)
	{
		this->_fProgressToNextGameTick = fProgressToNextGameTick;
		this->_ExpectedRenderingFrame = expectedRenderingFrame;
	}

	TestDataType type() const override
	{
		return TestDataType::Rendering;
	}
};

/**
 * @brief
 * This UnitTest tries to simulate the GameLoop (GameTickData) and the Rendering that can happen (RenderingData).
 * Rendering can happen more often than GameTicks and at any time between two GameTicks.
 * The Animation Distribution Logic must adjust to the Rendering that happens at any give point in time.
 */
void RunAnimationTest(const std::vector<TestData *> &vecTestData)
{
	AnimationInfo animInfo = {};

	int currentGameTick = 0;
	for (TestData *x : vecTestData) {
		switch (x->type()) {
		case TestDataType::SetNewAnimation: {
			auto *setNewAnimationData = static_cast<SetNewAnimationData *>(x);
			animInfo.setNewAnimation(std::nullopt, setNewAnimationData->_NumberOfFrames, setNewAnimationData->_DelayLen, setNewAnimationData->_Params, setNewAnimationData->_NumSkippedFrames, setNewAnimationData->_DistributeFramesBeforeFrame);
		} break;
		case TestDataType::GameTick: {
			auto *gameTickData = static_cast<GameTickData *>(x);
			currentGameTick += 1;
			animInfo.processAnimation();
			EXPECT_EQ(animInfo.currentFrame, gameTickData->_ExpectedAnimationFrame);
			EXPECT_EQ(animInfo.tickCounterOfCurrentFrame, gameTickData->_ExpectedAnimationCnt);
		} break;
		case TestDataType::Rendering: {
			auto *renderingData = static_cast<RenderingData *>(x);
			ProgressToNextGameTick = static_cast<uint8_t>(renderingData->_fProgressToNextGameTick * AnimationInfo::baseValueFraction);
			EXPECT_EQ(animInfo.getFrameToUseForRendering(), renderingData->_ExpectedRenderingFrame)
			    << std::fixed << std::setprecision(2)
			    << "ProgressToNextGameTick: " << renderingData->_fProgressToNextGameTick
			    << " currentFrame: " << animInfo.currentFrame
			    << " DelayCounter: " << animInfo.tickCounterOfCurrentFrame
			    << " GameTick: " << currentGameTick;
		} break;
		}
	}
	for (TestData *x : vecTestData) {
		delete x;
	}
}

TEST(AnimationInfo, AttackSwordWarrior) // ProcessAnimationPending should be considered by distribution logic
{
	RunAnimationTest(
	    {
	        new SetNewAnimationData(16, 1, AnimationDistributionFlags::ProcessAnimationPending, 0, 9),
	        // processAnimation directly after StartAttack (in same GameTick). So we don't see any rendering before.
	        new GameTickData(1, 0),
	        new RenderingData(0.0f, 0),
	        new RenderingData(0.3f, 0),
	        new RenderingData(0.6f, 0),
	        new RenderingData(0.8f, 0),
	        new GameTickData(2, 0),
	        new RenderingData(0.0f, 1),
	        new RenderingData(0.3f, 1),
	        new RenderingData(0.6f, 1),
	        new RenderingData(0.8f, 2),
	        new GameTickData(3, 0),
	        new RenderingData(0.0f, 2),
	        new RenderingData(0.3f, 2),
	        new RenderingData(0.6f, 2),
	        new RenderingData(0.8f, 3),
	        new GameTickData(4, 0),
	        new RenderingData(0.0f, 3),
	        new RenderingData(0.3f, 3),
	        new RenderingData(0.6f, 4),
	        new RenderingData(0.8f, 4),
	        new GameTickData(5, 0),
	        new RenderingData(0.0f, 4),
	        new RenderingData(0.3f, 4),
	        new RenderingData(0.6f, 5),
	        new RenderingData(0.8f, 5),
	        new GameTickData(6, 0),
	        new RenderingData(0.0f, 5),
	        new RenderingData(0.3f, 6),
	        new RenderingData(0.6f, 6),
	        new RenderingData(0.8f, 6),
	        new GameTickData(7, 0),
	        new RenderingData(0.0f, 6),
	        new RenderingData(0.3f, 7),
	        new RenderingData(0.6f, 7),
	        new RenderingData(0.8f, 7),

	        // After this GameTick, the Animation Distribution Logic is disabled
	        new GameTickData(8, 0),
	        new RenderingData(0.1f, 8),
	        new GameTickData(9, 0),
	        new RenderingData(0.4f, 9),
	        new GameTickData(10, 0),
	        new RenderingData(0.4f, 10),
	        new GameTickData(11, 0),
	        new RenderingData(0.3f, 11),
	        new GameTickData(12, 0),
	        new RenderingData(0.0f, 12),
	        new GameTickData(13, 0),
	        new RenderingData(0.6f, 13),
	        new GameTickData(14, 0),
	        new RenderingData(0.6f, 14),
	        new GameTickData(15, 0),
	        new RenderingData(0.6f, 15),
	        // Animation stopped because PM_DoAttack would stop the Animation "if (plr[pnum].AnimInfo.currentFrame == plr[pnum]._pAFrames - 1) {"
	    });
}

TEST(AnimationInfo, AttackSwordWarriorWithFastestAttack) // Skipped frames and ProcessAnimationPending should be considered by distribution logic
{
	RunAnimationTest(
	    {
	        new SetNewAnimationData(16, 1, AnimationDistributionFlags::ProcessAnimationPending, 2, 9),
	        // processAnimation directly after StartAttack (in same GameTick). So we don't see any rendering before.
	        new GameTickData(3, 0),
	        new RenderingData(0.0f, 0),
	        new RenderingData(0.3f, 0),
	        new RenderingData(0.6f, 0),
	        new RenderingData(0.8f, 1),
	        new GameTickData(4, 0),
	        new RenderingData(0.0f, 1),
	        new RenderingData(0.3f, 2),
	        new RenderingData(0.6f, 2),
	        new RenderingData(0.8f, 2),
	        new GameTickData(5, 0),
	        new RenderingData(0.0f, 3),
	        new RenderingData(0.3f, 3),
	        new RenderingData(0.6f, 4),
	        new RenderingData(0.8f, 4),
	        new GameTickData(6, 0),
	        new RenderingData(0.0f, 4),
	        new RenderingData(0.3f, 5),
	        new RenderingData(0.6f, 5),
	        new RenderingData(0.8f, 6),
	        new GameTickData(7, 0),
	        new RenderingData(0.0f, 6),
	        new RenderingData(0.3f, 6),
	        new RenderingData(0.6f, 7),
	        new RenderingData(0.8f, 7),

	        // After this GameTick, the Animation Distribution Logic is disabled
	        new GameTickData(8, 0),
	        new RenderingData(0.1f, 8),
	        new GameTickData(9, 0),
	        new RenderingData(0.4f, 9),
	        new GameTickData(10, 0),
	        new RenderingData(0.4f, 10),
	        new GameTickData(11, 0),
	        new RenderingData(0.3f, 11),
	        new GameTickData(12, 0),
	        new RenderingData(0.0f, 12),
	        new GameTickData(13, 0),
	        new RenderingData(0.6f, 13),
	        new GameTickData(14, 0),
	        new RenderingData(0.6f, 14),
	        new GameTickData(15, 0),
	        new RenderingData(0.6f, 15),
	        // Animation stopped because PM_DoAttack would stop the Animation "if (plr[pnum].AnimInfo.currentFrame == plr[pnum]._pAFrames - 1) {"
	    });
}

/**
 * @brief The Warrior make two attacks. The second queued attack cancels the first after the Hit Frame.
 */
TEST(AnimationInfo, AttackSwordWarriorRepeated)
{
	RunAnimationTest(
	    {
	        new SetNewAnimationData(16, 1, AnimationDistributionFlags::ProcessAnimationPending, 0, 9),
	        // processAnimation directly after StartAttack (in same GameTick). So we don't see any rendering before.
	        new GameTickData(1, 0),
	        new RenderingData(0.0f, 0),
	        new RenderingData(0.3f, 0),
	        new RenderingData(0.6f, 0),
	        new RenderingData(0.8f, 0),
	        new GameTickData(2, 0),
	        new RenderingData(0.0f, 1),
	        new RenderingData(0.3f, 1),
	        new RenderingData(0.6f, 1),
	        new RenderingData(0.8f, 2),
	        new GameTickData(3, 0),
	        new RenderingData(0.0f, 2),
	        new RenderingData(0.3f, 2),
	        new RenderingData(0.6f, 2),
	        new RenderingData(0.8f, 3),
	        new GameTickData(4, 0),
	        new RenderingData(0.0f, 3),
	        new RenderingData(0.3f, 3),
	        new RenderingData(0.6f, 4),
	        new RenderingData(0.8f, 4),
	        new GameTickData(5, 0),
	        new RenderingData(0.0f, 4),
	        new RenderingData(0.3f, 4),
	        new RenderingData(0.6f, 5),
	        new RenderingData(0.8f, 5),
	        new GameTickData(6, 0),
	        new RenderingData(0.0f, 5),
	        new RenderingData(0.3f, 6),
	        new RenderingData(0.6f, 6),
	        new RenderingData(0.8f, 6),
	        new GameTickData(7, 0),
	        new RenderingData(0.0f, 6),
	        new RenderingData(0.3f, 7),
	        new RenderingData(0.6f, 7),
	        new RenderingData(0.8f, 7),

	        // After this GameTick, the Animation Distribution Logic is disabled
	        new GameTickData(8, 0),
	        new RenderingData(0.1f, 8),
	        new GameTickData(9, 0),
	        new RenderingData(0.3f, 9),

	        // Start of repeated attack, because plr[pnum].AnimInfo.currentFrame > plr[myplr]._pAFNum
	        new SetNewAnimationData(16, 1, static_cast<AnimationDistributionFlags>(AnimationDistributionFlags::ProcessAnimationPending | AnimationDistributionFlags::RepeatedAction), 0, 9),
	        // processAnimation directly after StartAttack (in same GameTick). So we don't see any rendering before.
	        new GameTickData(1, 0),
	        new RenderingData(0.0f, 10),
	        new RenderingData(0.3f, 10),
	        new RenderingData(0.6f, 11),
	        new RenderingData(0.8f, 11),
	        new GameTickData(2, 0),
	        new RenderingData(0.0f, 12),
	        new RenderingData(0.3f, 12),
	        new RenderingData(0.6f, 13),
	        new RenderingData(0.8f, 13),
	        new GameTickData(3, 0),
	        new RenderingData(0.0f, 14),
	        new RenderingData(0.3f, 14),
	        new RenderingData(0.6f, 15),
	        new RenderingData(0.8f, 15),
	        new GameTickData(4, 0),
	        new RenderingData(0.0f, 0),
	        new RenderingData(0.3f, 0),
	        new RenderingData(0.6f, 1),
	        new RenderingData(0.8f, 1),
	        new GameTickData(5, 0),
	        new RenderingData(0.0f, 2),
	        new RenderingData(0.3f, 2),
	        new RenderingData(0.6f, 3),
	        new RenderingData(0.8f, 3),
	        new GameTickData(6, 0),
	        new RenderingData(0.0f, 4),
	        new RenderingData(0.3f, 4),
	        new RenderingData(0.6f, 5),
	        new RenderingData(0.8f, 5),
	        new GameTickData(7, 0),
	        new RenderingData(0.0f, 6),
	        new RenderingData(0.3f, 6),
	        new RenderingData(0.6f, 7),
	        new RenderingData(0.8f, 7),

	        // After this GameTick, the Animation Distribution Logic is disabled
	        new GameTickData(8, 0),
	        new RenderingData(0.1f, 8),
	        new GameTickData(9, 0),
	        new RenderingData(0.4f, 9),
	        new GameTickData(10, 0),
	        new RenderingData(0.4f, 10),
	        new GameTickData(11, 0),
	        new RenderingData(0.3f, 11),
	        new GameTickData(12, 0),
	        new RenderingData(0.0f, 12),
	        new GameTickData(13, 0),
	        new RenderingData(0.6f, 13),
	        new GameTickData(14, 0),
	        new RenderingData(0.6f, 14),
	        new GameTickData(15, 0),
	        new RenderingData(0.6f, 15),
	        // Animation stopped because PM_DoAttack would stop the Animation "if (plr[pnum].AnimInfo.currentFrame == plr[pnum]._pAFrames - 1) {"
	    });
}

TEST(AnimationInfo, BlockingWarriorNormal) // Ignored delay for last Frame should be considered by distribution logic
{
	RunAnimationTest(
	    {
	        new SetNewAnimationData(2, 3, AnimationDistributionFlags::SkipsDelayOfLastFrame),
	        new RenderingData(0.0f, 0),
	        new RenderingData(0.3f, 0),
	        new RenderingData(0.6f, 0),
	        new RenderingData(0.8f, 0),
	        new GameTickData(0, 1),
	        new RenderingData(0.0f, 0),
	        new RenderingData(0.3f, 0),
	        new RenderingData(0.6f, 0),
	        new RenderingData(0.8f, 0),
	        new GameTickData(0, 2),
	        new RenderingData(0.0f, 1),
	        new RenderingData(0.3f, 1),
	        new RenderingData(0.6f, 1),
	        new RenderingData(0.8f, 1),
	        new GameTickData(1, 0),
	        new RenderingData(0.0f, 1),
	        new RenderingData(0.3f, 1),
	        new RenderingData(0.6f, 1),
	        new RenderingData(0.8f, 1),
	        // Animation stopped because PM_DoBlock would stop the Animation "if (plr[pnum].AnimInfo.currentFrame >= plr[pnum]._pBFrames) {"
	    });
}

TEST(AnimationInfo, BlockingSorcererWithFastBlock) // Skipped frames and ignored delay for last Frame should be considered by distribution logic
{
	RunAnimationTest(
	    {
	        new SetNewAnimationData(6, 3, AnimationDistributionFlags::SkipsDelayOfLastFrame, 4),
	        new RenderingData(0.0f, 0),
	        new RenderingData(0.3f, 0),
	        new RenderingData(0.6f, 0),
	        new RenderingData(0.8f, 1),
	        new GameTickData(4, 1),
	        new RenderingData(0.0f, 1),
	        new RenderingData(0.3f, 1),
	        new RenderingData(0.6f, 2),
	        new RenderingData(0.8f, 2),
	        new GameTickData(4, 2),
	        new RenderingData(0.0f, 3),
	        new RenderingData(0.3f, 3),
	        new RenderingData(0.6f, 3),
	        new RenderingData(0.8f, 4),
	        new GameTickData(5, 0),
	        new RenderingData(0.0f, 4),
	        new RenderingData(0.3f, 4),
	        new RenderingData(0.6f, 5),
	        new RenderingData(0.8f, 5),
	        // Animation stopped because PM_DoBlock would stop the Animation "if (plr[pnum].AnimInfo.currentFrame >= plr[pnum]._pBFrames) {"
	    });
}

TEST(AnimationInfo, HitRecoverySorcererHarmony) // Skipped frames and ignored delay for last Frame should be considered by distribution logic
{
	RunAnimationTest(
	    {
	        new SetNewAnimationData(8, 1, AnimationDistributionFlags::None, 3),
	        new RenderingData(0.0f, 0),
	        new RenderingData(0.3f, 0),
	        new RenderingData(0.6f, 0),
	        new RenderingData(0.8f, 1),
	        new GameTickData(4, 0),
	        new RenderingData(0.0f, 1),
	        new RenderingData(0.3f, 2),
	        new RenderingData(0.6f, 2),
	        new RenderingData(0.8f, 2),
	        new GameTickData(5, 0),
	        new RenderingData(0.0f, 3),
	        new RenderingData(0.3f, 3),
	        new RenderingData(0.6f, 4),
	        new RenderingData(0.8f, 4),
	        new GameTickData(6, 0),
	        new RenderingData(0.0f, 4),
	        new RenderingData(0.3f, 5),
	        new RenderingData(0.6f, 5),
	        new RenderingData(0.8f, 6),
	        new GameTickData(7, 0),
	        new RenderingData(0.0f, 6),
	        new RenderingData(0.3f, 6),
	        new RenderingData(0.6f, 7),
	        new RenderingData(0.8f, 7),
	        // Animation stopped because PM_DoGotHit would stop the Animation "if (plr[pnum].AnimInfo.currentFrame >= plr[pnum]._pHFrames) {"
	    });
}
TEST(AnimationInfo, Stand) // Distribution Logic shouldn't change anything here
{
	RunAnimationTest(
	    {
	        new SetNewAnimationData(10, 4),
	        new RenderingData(0.1f, 0),
	        new GameTickData(0, 1),
	        new RenderingData(0.6f, 0),
	        new GameTickData(0, 2),
	        new RenderingData(0.6f, 0),
	        new GameTickData(0, 3),
	        new RenderingData(0.6f, 0),

	        new GameTickData(1, 0),
	        new RenderingData(0.6f, 1),
	        new GameTickData(1, 1),
	        new RenderingData(0.6f, 1),
	        new GameTickData(1, 2),
	        new RenderingData(0.6f, 1),
	        new GameTickData(1, 3),
	        new RenderingData(0.6f, 1),

	        new GameTickData(2, 0),
	        new RenderingData(0.6f, 2),
	        new GameTickData(2, 1),
	        new RenderingData(0.6f, 2),
	        new GameTickData(2, 2),
	        new RenderingData(0.6f, 2),
	        new GameTickData(2, 3),
	        new RenderingData(0.6f, 2),

	        new GameTickData(3, 0),
	        new RenderingData(0.6f, 3),
	        new GameTickData(3, 1),
	        new RenderingData(0.6f, 3),
	        new GameTickData(3, 2),
	        new RenderingData(0.6f, 3),
	        new GameTickData(3, 3),
	        new RenderingData(0.6f, 3),

	        new GameTickData(4, 0),
	        new RenderingData(0.6f, 4),
	        new GameTickData(4, 1),
	        new RenderingData(0.6f, 4),
	        new GameTickData(4, 2),
	        new RenderingData(0.6f, 4),
	        new GameTickData(4, 3),
	        new RenderingData(0.6f, 4),

	        new GameTickData(5, 0),
	        new RenderingData(0.6f, 5),
	        new GameTickData(5, 1),
	        new RenderingData(0.6f, 5),
	        new GameTickData(5, 2),
	        new RenderingData(0.6f, 5),
	        new GameTickData(5, 3),
	        new RenderingData(0.6f, 5),

	        new GameTickData(6, 0),
	        new RenderingData(0.6f, 6),
	        new GameTickData(6, 1),
	        new RenderingData(0.6f, 6),
	        new GameTickData(6, 2),
	        new RenderingData(0.6f, 6),
	        new GameTickData(6, 3),
	        new RenderingData(0.6f, 6),

	        new GameTickData(7, 0),
	        new RenderingData(0.6f, 7),
	        new GameTickData(7, 1),
	        new RenderingData(0.6f, 7),
	        new GameTickData(7, 2),
	        new RenderingData(0.6f, 7),
	        new GameTickData(7, 3),
	        new RenderingData(0.6f, 7),

	        new GameTickData(8, 0),
	        new RenderingData(0.6f, 8),
	        new GameTickData(8, 1),
	        new RenderingData(0.6f, 8),
	        new GameTickData(8, 2),
	        new RenderingData(0.6f, 8),
	        new GameTickData(8, 3),
	        new RenderingData(0.6f, 8),

	        new GameTickData(9, 0),
	        new RenderingData(0.6f, 9),
	        new GameTickData(9, 1),
	        new RenderingData(0.6f, 9),
	        new GameTickData(9, 2),
	        new RenderingData(0.6f, 9),
	        new GameTickData(9, 3),
	        new RenderingData(0.6f, 9),

	        // Animation starts again
	        new GameTickData(0, 0),
	        new RenderingData(0.1f, 0),
	        new GameTickData(0, 1),
	        new RenderingData(0.6f, 0),
	        new GameTickData(0, 2),
	        new RenderingData(0.6f, 0),
	        new GameTickData(0, 3),
	        new RenderingData(0.6f, 0),
	    });
}
