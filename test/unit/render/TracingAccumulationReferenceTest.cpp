#include <gtest/gtest.h>

#include "render/TracingAccumulationReference.h"
#include "render/tonemap/ReinhardTonemap.h"
#include "test/helpers/ColorTestHelper.h"
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalTracingAccumulationKernel.h"
#endif

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace TracingAccumulationReferenceTest {
  using namespace render;

  TEST(TracingAccumulationReference, ClearResetsColorSumsCountsAndResolve) {
    TracingAccumulationBuffer accumulation(2, 1);
    accumulation.addSample(0, 0, Colord(0.25, 0.5, 0.75));
    accumulation.addSample(1, 0, Colord(1.0, 0.0, 0.0));

    accumulation.clear();

    EXPECT_EQ(Colord::black(), accumulation.colorSum()[0][0]);
    EXPECT_EQ(0u, accumulation.sampleCount()[0][0]);
    EXPECT_EQ(Colord::black(), accumulation.colorSum()[0][1]);
    EXPECT_EQ(0u, accumulation.sampleCount()[0][1]);

    Buffer<unsigned int> resolved(2, 1);
    accumulation.resolve(resolved);
    EXPECT_EQ(Colord::black().rgb(), resolved[0][0]);
    EXPECT_EQ(Colord::black().rgb(), resolved[0][1]);
  }

  TEST(TracingAccumulationReference, AddSampleAccumulatesHdrColorAndCount) {
    TracingAccumulationBuffer accumulation(2, 1);

    accumulation.addSample(1, 0, Colord(0.25, 0.5, 1.5));

    EXPECT_EQ(Colord::black(), accumulation.colorSum()[0][0]);
    EXPECT_EQ(0u, accumulation.sampleCount()[0][0]);
    EXPECT_EQ(Colord(0.25, 0.5, 1.5), accumulation.colorSum()[0][1]);
    EXPECT_EQ(1u, accumulation.sampleCount()[0][1]);
    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 1.5), accumulation.resolvedColor(1, 0), 1e-12);
  }

  TEST(TracingAccumulationReference, MultiSampleAccumulationAveragesOnResolve) {
    TracingAccumulationBuffer accumulation(1, 1);

    accumulation.addSample(0, 0, Colord(0.25, 0.5, 1.0));
    accumulation.addSample(0, 0, Colord(0.75, 0.25, 0.0));
    accumulation.addSample(0, 0, Colord(0.5, 0.75, 0.5));

    EXPECT_EQ(3u, accumulation.sampleCount()[0][0]);
    EXPECT_EQ(Colord(1.5, 1.5, 1.5), accumulation.colorSum()[0][0]);
    ASSERT_COLOR_NEAR(Colord(0.5, 0.5, 0.5), accumulation.resolvedColor(0, 0), 1e-12);

    Buffer<unsigned int> resolved(1, 1);
    accumulation.resolve(resolved);
    EXPECT_EQ(Colord(0.5, 0.5, 0.5).rgb(), resolved[0][0]);
  }

  TEST(TracingAccumulationReference, AddSamplesAccumulatesOneSampleForEveryPixel) {
    TracingAccumulationBuffer accumulation(2, 2);
    Buffer<Colord> colors(2, 2);
    colors[0][0] = Colord(0.1, 0.2, 0.3);
    colors[0][1] = Colord(0.4, 0.5, 0.6);
    colors[1][0] = Colord(0.7, 0.8, 0.9);
    colors[1][1] = Colord(1.0, 1.1, 1.2);

    accumulation.addSamples(colors);
    accumulation.addSamples(colors);

    EXPECT_EQ(2u, accumulation.sampleCount()[1][1]);
    ASSERT_COLOR_NEAR(Colord(2.0, 2.2, 2.4), accumulation.colorSum()[1][1], 1e-12);
    ASSERT_COLOR_NEAR(colors[1][1], accumulation.resolvedColor(1, 1), 1e-12);
  }

  TEST(TracingAccumulationReference, OptionalMomentPlaneAccumulatesSecondRawMoments) {
    TracingAccumulationLayout layout = TracingAccumulationLayout::image(1, 1);
    layout.momentFormat = TracingAccumulationMomentFormat::RGBA32FloatSecondRawMoment;
    TracingAccumulationBuffer accumulation(layout);

    ASSERT_TRUE(accumulation.hasSecondMoment());
    ASSERT_NE(nullptr, accumulation.secondMoment());

    accumulation.addSample(0, 0, Colord(2.0, 3.0, 4.0));
    accumulation.addSample(0, 0, Colord(0.5, 0.25, 0.125));

    ASSERT_COLOR_NEAR(Colord(4.25, 9.0625, 16.015625), (*accumulation.secondMoment())[0][0], 1e-12);
  }

  TEST(TracingAccumulationReference, ResolveAppliesOptionalTonemapAfterAveraging) {
    TracingAccumulationBuffer accumulation(1, 1);
    accumulation.addSample(0, 0, Colord(2.0, 0.5, 0.0));
    ReinhardTonemap tonemap;

    Buffer<unsigned int> resolved(1, 1);
    accumulation.resolve(resolved, &tonemap);

    EXPECT_EQ(Colord(2.0 / 3.0, 1.0 / 3.0, 0.0).rgb(), resolved[0][0]);
  }

  TEST(TracingAccumulationReference, RejectsMismatchedSampleAndResolveShapes) {
    TracingAccumulationBuffer accumulation(2, 2);
    Buffer<Colord> colors(1, 2);
    Buffer<unsigned int> resolved(2, 1);

    EXPECT_THROW(accumulation.addSamples(colors), std::invalid_argument);
    EXPECT_THROW(accumulation.resolve(resolved), std::invalid_argument);
  }

  TEST(TracingAccumulationReference, RejectsOutOfRangePixelsAndSampleCountOverflow) {
    TracingAccumulationBuffer accumulation(1, 1);

    EXPECT_THROW(accumulation.addSample(-1, 0, Colord::white()), std::out_of_range);
    EXPECT_THROW(accumulation.addSample(1, 0, Colord::white()), std::out_of_range);
    EXPECT_THROW((void)accumulation.resolvedColor(0, 1), std::out_of_range);

    accumulation.sampleCount()[0][0] = std::numeric_limits<std::uint32_t>::max();
    EXPECT_THROW(accumulation.addSample(0, 0, Colord::white()), std::overflow_error);
  }

  TEST(MetalTracingAccumulationKernel, MatchesCpuReferenceOnSyntheticInputs) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalTracingAccumulationKernel kernel;
    if (!kernel.deviceAvailable()) {
      GTEST_SKIP() << "No Metal device is available";
    }
    if (!kernel.accumulationPathAvailable()) {
      GTEST_SKIP() << kernel.accumulationPathUnavailableReason();
    }

    TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 2);
    layout.momentFormat = TracingAccumulationMomentFormat::RGBA32FloatSecondRawMoment;

    Buffer<Colord> firstSamples(2, 2);
    firstSamples[0][0] = Colord(0.25, 0.5, 0.75);
    firstSamples[0][1] = Colord(1.0, 0.0, 0.25);
    firstSamples[1][0] = Colord(0.0, 0.125, 0.5);
    firstSamples[1][1] = Colord(1.5, 0.25, 0.0);

    Buffer<Colord> secondSamples(2, 2);
    secondSamples[0][0] = Colord(0.75, 0.25, 0.0);
    secondSamples[0][1] = Colord(0.0, 0.5, 0.25);
    secondSamples[1][0] = Colord(0.5, 0.375, 0.25);
    secondSamples[1][1] = Colord(0.25, 0.75, 0.5);

    TracingAccumulationBuffer expected(layout);
    expected.addSamples(firstSamples);
    expected.addSamples(secondSamples);

    MetalTracingAccumulationBuffer metal(layout);
    metal.addSamples(firstSamples);
    metal.addSamples(secondSamples);

    TracingAccumulationBuffer actual(layout);
    metal.copyTo(actual);

    for (int y = 0; y != layout.height; ++y) {
      for (int x = 0; x != layout.width; ++x) {
        EXPECT_EQ(expected.sampleCount()[y][x], actual.sampleCount()[y][x]);
        ASSERT_COLOR_NEAR(expected.colorSum()[y][x], actual.colorSum()[y][x], 1e-6);
        ASSERT_NE(nullptr, expected.secondMoment());
        ASSERT_NE(nullptr, actual.secondMoment());
        ASSERT_COLOR_NEAR((*expected.secondMoment())[y][x], (*actual.secondMoment())[y][x], 1e-6);
      }
    }

    Buffer<unsigned int> expectedResolved(2, 2);
    Buffer<unsigned int> actualResolved(2, 2);
    expected.resolve(expectedResolved);
    metal.resolve(actualResolved);
    for (int y = 0; y != layout.height; ++y) {
      for (int x = 0; x != layout.width; ++x) {
        EXPECT_EQ(expectedResolved[y][x], actualResolved[y][x]);
      }
    }

    metal.clear();
    metal.copyTo(actual);
    for (int y = 0; y != layout.height; ++y) {
      for (int x = 0; x != layout.width; ++x) {
        EXPECT_EQ(0u, actual.sampleCount()[y][x]);
        EXPECT_EQ(Colord::black(), actual.colorSum()[y][x]);
        ASSERT_NE(nullptr, actual.secondMoment());
        EXPECT_EQ(Colord::black(), (*actual.secondMoment())[y][x]);
      }
    }
#else
    GTEST_SKIP() << "Metal tracing accumulation backend is disabled";
#endif
  }
}
