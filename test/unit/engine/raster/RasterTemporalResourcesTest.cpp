#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "src/engine/raster/RasterTemporalResources.h"

#include <limits>

namespace RasterTemporalResourcesTest {
  using namespace engine::raster::detail;

  TEST(RasterTemporalResources, CompleteContractCanAccumulate) {
    Buffer<Colord> historyColor(8, 4);
    Buffer<Colord> nextHistoryColor(8, 4);
    Buffer<double> currentDepth(8, 4);
    Buffer<double> historyDepth(8, 4);
    Buffer<Vector2d> motionVectors(8, 4);

    TemporalResourceContract contract;
    contract.historyColor = &historyColor;
    contract.nextHistoryColor = &nextHistoryColor;
    contract.currentDepth = &currentDepth;
    contract.historyDepth = &historyDepth;
    contract.motionVectors = &motionVectors;
    contract.currentJitter = {0.25, -0.25};
    contract.previousJitter = {-0.25, 0.25};

    const auto validation = validateTemporalResourceContract(contract, 8, 4);

    EXPECT_TRUE(validation.complete);
    EXPECT_TRUE(validation.canAccumulate);
    EXPECT_TRUE(validation.errors.empty());
  }

  TEST(RasterTemporalResources, MissingAndMismatchedBuffersAreIncomplete) {
    Buffer<Colord> historyColor(8, 4);
    Buffer<Colord> nextHistoryColor(7, 4);
    Buffer<double> currentDepth(8, 4);
    Buffer<double> historyDepth(8, 3);

    TemporalResourceContract contract;
    contract.historyColor = &historyColor;
    contract.nextHistoryColor = &nextHistoryColor;
    contract.currentDepth = &currentDepth;
    contract.historyDepth = &historyDepth;

    const auto validation = validateTemporalResourceContract(contract, 8, 4);

    EXPECT_FALSE(validation.complete);
    EXPECT_FALSE(validation.canAccumulate);
    EXPECT_EQ(3u, validation.errors.size());
  }

  TEST(RasterTemporalResources, ResetConditionKeepsResourcesCompleteButBlocksAccumulation) {
    Buffer<Colord> historyColor(8, 4);
    Buffer<Colord> nextHistoryColor(8, 4);
    Buffer<double> currentDepth(8, 4);
    Buffer<double> historyDepth(8, 4);
    Buffer<Vector2d> motionVectors(8, 4);

    TemporalResourceContract contract;
    contract.historyColor = &historyColor;
    contract.nextHistoryColor = &nextHistoryColor;
    contract.currentDepth = &currentDepth;
    contract.historyDepth = &historyDepth;
    contract.motionVectors = &motionVectors;
    contract.resetCondition = TemporalResetCondition::CameraCut;

    const auto validation = validateTemporalResourceContract(contract, 8, 4);

    EXPECT_TRUE(validation.complete);
    EXPECT_FALSE(validation.canAccumulate);
    EXPECT_TRUE(validation.errors.empty());
  }

  TEST(RasterTemporalResources, NonFiniteJitterIsInvalid) {
    Buffer<Colord> historyColor(8, 4);
    Buffer<Colord> nextHistoryColor(8, 4);
    Buffer<double> currentDepth(8, 4);
    Buffer<double> historyDepth(8, 4);
    Buffer<Vector2d> motionVectors(8, 4);

    TemporalResourceContract contract;
    contract.historyColor = &historyColor;
    contract.nextHistoryColor = &nextHistoryColor;
    contract.currentDepth = &currentDepth;
    contract.historyDepth = &historyDepth;
    contract.motionVectors = &motionVectors;
    contract.currentJitter = {std::numeric_limits<double>::infinity(), 0.0};

    const auto validation = validateTemporalResourceContract(contract, 8, 4);

    EXPECT_FALSE(validation.complete);
    EXPECT_FALSE(validation.canAccumulate);
    ASSERT_EQ(1u, validation.errors.size());
    EXPECT_EQ("current jitter must be finite", validation.errors.front());
  }

  TEST(RasterTemporalResources, RejectsEmptyRenderTarget) {
    TemporalResourceContract contract;

    const auto validation = validateTemporalResourceContract(contract, 0, 4);

    EXPECT_FALSE(validation.complete);
    EXPECT_FALSE(validation.canAccumulate);
    ASSERT_EQ(1u, validation.errors.size());
    EXPECT_EQ("render target dimensions must be positive", validation.errors.front());
  }
}
