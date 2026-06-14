#include <gtest/gtest.h>

#include "render/TracingAccumulationReference.h"
#include "render/VulkanTracingAccumulationKernel.h"
#include "test/helpers/ColorTestHelper.h"

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace VulkanTracingAccumulationKernelTest {
  using namespace render;

  std::vector<Colord> frame(std::initializer_list<Colord> colors) {
    return std::vector<Colord>(colors);
  }

  void addFrame(TracingAccumulationBuffer& reference, int width, int height,
                const std::vector<Colord>& colors) {
    Buffer<Colord> buffer(width, height);
    for (int y = 0; y != height; ++y) {
      for (int x = 0; x != width; ++x) {
        buffer[y][x] = colors[static_cast<std::size_t>(y * width + x)];
      }
    }
    reference.addSamples(buffer);
  }

  void addReferenceFrames(TracingAccumulationBuffer& reference,
                          const std::vector<std::vector<Colord>>& sampleFrames) {
    const TracingAccumulationLayout& layout = reference.layout();
    for (const std::vector<Colord>& colors : sampleFrames) {
      addFrame(reference, layout.width, layout.height, colors);
    }
  }

  TEST(VulkanTracingAccumulationKernel, ReportsUnavailableWhenDisabled) {
    VulkanTracingAccumulationKernel kernel;
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    EXPECT_NO_THROW((void)kernel.deviceAvailable());
    if (kernel.deviceAvailable()) {
      EXPECT_TRUE(kernel.deviceUnavailableReason().empty());
    } else {
      EXPECT_FALSE(kernel.deviceUnavailableReason().empty());
    }
    if (kernel.accumulationAvailable()) {
      EXPECT_TRUE(kernel.accumulationUnavailableReason().empty());
    } else {
      EXPECT_FALSE(kernel.accumulationUnavailableReason().empty());
    }
#else
    EXPECT_FALSE(kernel.deviceAvailable());
    EXPECT_FALSE(kernel.accumulationAvailable());
    EXPECT_NE(std::string::npos, kernel.deviceUnavailableReason().find("not enabled"));
    EXPECT_NE(std::string::npos, kernel.accumulationUnavailableReason().find("not enabled"));
    EXPECT_THROW((void)kernel.runClearAddResolve(TracingAccumulationLayout::image(1, 1),
                                                 {frame({Colord::white()})}),
                 std::runtime_error);
#endif
  }

  TEST(VulkanTracingAccumulationKernel, ClearAddAndResolveMatchCpuReference) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VulkanTracingAccumulationKernel kernel;
    if (!kernel.accumulationAvailable()) {
      GTEST_SKIP() << kernel.accumulationUnavailableReason();
    }

    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(3, 2);
    const std::vector<std::vector<Colord>> frames{
      frame({Colord(0.25, 0.5, 0.75), Colord(1.5, 0.0, 0.0), Colord(0.0, 1.5, 0.0),
             Colord(0.0, 0.0, 1.5), Colord(0.125, 0.25, 0.5), Colord(0.9, 0.8, 0.7)}),
      frame({Colord(0.75, 0.25, 0.0), Colord(0.0, 0.5, 0.0), Colord(0.5, 0.0, 0.5),
             Colord(0.25, 0.25, 0.25), Colord(0.375, 0.25, 0.0), Colord(0.1, 0.2, 0.3)}),
      frame({Colord(0.5, 0.25, 0.25), Colord(0.0, 0.0, 0.5), Colord(0.5, 0.5, 0.0),
             Colord(0.75, 0.0, 0.0), Colord(0.5, 0.0, 0.5), Colord(0.0, 0.0, 0.0)}),
    };

    TracingAccumulationBuffer expected(layout);
    addReferenceFrames(expected, frames);
    Buffer<unsigned int> expectedResolved(layout.width, layout.height);
    expected.resolve(expectedResolved);

    const VulkanTracingAccumulationResult actual = kernel.runClearAddResolve(layout, frames);

    ASSERT_EQ(static_cast<std::size_t>(layout.pixelCount()), actual.colorSums.size());
    ASSERT_EQ(static_cast<std::size_t>(layout.pixelCount()), actual.sampleCounts.size());
    ASSERT_TRUE(actual.secondMoments.empty());
    ASSERT_EQ(static_cast<std::size_t>(layout.pixelCount()), actual.resolved.size());
    for (int y = 0; y != layout.height; ++y) {
      for (int x = 0; x != layout.width; ++x) {
        ASSERT_COLOR_NEAR(expected.colorSum()[y][x], actual.colorSumAt(x, y), 1e-6);
        EXPECT_EQ(expected.sampleCount()[y][x], actual.sampleCountAt(x, y));
        EXPECT_EQ(expectedResolved[y][x], actual.resolvedAt(x, y));
      }
    }
    EXPECT_EQ("vulkan", actual.diagnostics.backend);
    EXPECT_EQ("gpu_device", actual.diagnostics.residency);
    EXPECT_EQ(layout.totalBytes(), actual.diagnostics.residentBytes);
    EXPECT_EQ(1u, actual.diagnostics.clearOperations);
    EXPECT_EQ(frames.size(), actual.diagnostics.addOperations);
    EXPECT_EQ(layout.pixelCount() * frames.size(), actual.diagnostics.addedSamples);
    EXPECT_EQ(1u, actual.diagnostics.resolveOperations);
    EXPECT_EQ(3u, actual.diagnostics.readbackOperations);
    EXPECT_EQ(layout.colorSumBytes() + layout.sampleCountBytes() + layout.resolveBytes(),
              actual.diagnostics.readbackBytes);
#else
    GTEST_SKIP() << "Vulkan tracing accumulation kernels are disabled";
#endif
  }

  TEST(VulkanTracingAccumulationKernel, OptionalSecondMomentMatchesCpuReference) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VulkanTracingAccumulationKernel kernel;
    if (!kernel.accumulationAvailable()) {
      GTEST_SKIP() << kernel.accumulationUnavailableReason();
    }

    TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 1);
    layout.momentFormat = TracingAccumulationMomentFormat::RGBA32FloatSecondRawMoment;
    const std::vector<std::vector<Colord>> frames{
      frame({Colord(2.0, 3.0, 4.0), Colord(0.5, 0.25, 0.125)}),
      frame({Colord(0.5, 0.25, 0.125), Colord(2.0, 3.0, 4.0)}),
    };

    TracingAccumulationBuffer expected(layout);
    addReferenceFrames(expected, frames);
    const VulkanTracingAccumulationResult actual = kernel.runClearAddResolve(layout, frames);

    ASSERT_TRUE(expected.hasSecondMoment());
    ASSERT_NE(nullptr, expected.secondMoment());
    ASSERT_EQ(static_cast<std::size_t>(layout.pixelCount()), actual.secondMoments.size());
    EXPECT_EQ(4u, actual.diagnostics.readbackOperations);
    EXPECT_EQ(layout.colorSumBytes() + layout.sampleCountBytes() + layout.momentBytes() +
                layout.resolveBytes(),
              actual.diagnostics.readbackBytes);
    for (int y = 0; y != layout.height; ++y) {
      for (int x = 0; x != layout.width; ++x) {
        ASSERT_COLOR_NEAR((*expected.secondMoment())[y][x], actual.secondMomentAt(x, y), 1e-5);
      }
    }
#else
    GTEST_SKIP() << "Vulkan tracing accumulation kernels are disabled";
#endif
  }

  TEST(VulkanTracingAccumulationKernel, RejectsMismatchedSampleFrameShape) {
    VulkanTracingAccumulationKernel kernel;
    EXPECT_THROW((void)kernel.runClearAddResolve(TracingAccumulationLayout::image(2, 2),
                                                 {frame({Colord::white()})}),
                 std::invalid_argument);
  }
}
