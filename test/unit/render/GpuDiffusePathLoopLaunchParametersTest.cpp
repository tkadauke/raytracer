#include <gtest/gtest.h>

#include "render/GpuDiffusePathLoopLaunch.h"

using namespace render;

TEST(GpuDiffusePathLoopLaunchParameters, PixelCountMultipliesImageDimensions) {
  GpuDiffusePathLoopLaunchParameters parameters;
  parameters.imageWidth = 0xFFFFFFFFu;
  parameters.imageHeight = 3u;
  EXPECT_EQ(0xFFFFFFFFull * 3ull, parameters.pixelCount());
}

TEST(GpuDiffusePathLoopLaunchParameters, DisplayPixelCountFollowsAccumulationTarget) {
  GpuDiffusePathLoopLaunchParameters parameters;
  parameters.imageWidth = 8u;
  parameters.imageHeight = 4u;

  parameters.accumulationTargetMode = gpuDiffusePathLoopAccumulationTargetPixel;
  EXPECT_EQ(32u, parameters.displayPixelCount());
  parameters.accumulationTargetMode = gpuDiffusePathLoopAccumulationTargetPath;
  EXPECT_EQ(0u, parameters.displayPixelCount());
  parameters.accumulationTargetMode = gpuDiffusePathLoopAccumulationTargetSampleSlot;
  EXPECT_EQ(8u, parameters.displayPixelCount());
}
