#include <gtest/gtest.h>

#include <cstdint>

#include "render/samplers/GpuSampleStream.h"

namespace GpuSampleStreamTest {
  using namespace render;

  constexpr double kOneOverTwoTo24 = 1.0 / 16777216.0;

  void expectSample2D(const char* label, const Vector2d& sample, std::uint32_t expectedX,
                      std::uint32_t expectedY) {
    SCOPED_TRACE(label);
    EXPECT_DOUBLE_EQ(static_cast<double>(expectedX) * kOneOverTwoTo24, sample.x());
    EXPECT_DOUBLE_EQ(static_cast<double>(expectedY) * kOneOverTwoTo24, sample.y());
  }

  void expectSample1D(const char* label, double sample, std::uint32_t expected) {
    SCOPED_TRACE(label);
    EXPECT_DOUBLE_EQ(static_cast<double>(expected) * kOneOverTwoTo24, sample);
  }

  TEST(GpuSampleStream, PcgHash32MatchesReferenceAlgorithm) {
    EXPECT_EQ(129708002u, GpuSampleStream::pcgHash32(0u));
    EXPECT_EQ(2831084092u, GpuSampleStream::pcgHash32(1u));
    EXPECT_EQ(2572358369u, GpuSampleStream::pcgHash32(0x12345678u));
  }

  TEST(GpuSampleStream, StaticSamplesArePureFunctionsOfCoordinate) {
    const GpuSampleCoordinate coordinate{
      /*seed=*/17,
      /*pixelIndex=*/23,
      /*primarySampleIndex=*/5,
      /*dimension=*/sampleDimensionIndex(SampleDimension::BSDF, 2),
      /*component=*/1};

    EXPECT_EQ(1925028672u, GpuSampleStream::hash(coordinate));
    EXPECT_DOUBLE_EQ(7519643.0 / 16777216.0, GpuSampleStream::sample1D(coordinate));
    EXPECT_EQ(GpuSampleStream::hash(coordinate), GpuSampleStream::hash(coordinate));
    EXPECT_DOUBLE_EQ(GpuSampleStream::sample1D(coordinate), GpuSampleStream::sample1D(coordinate));

    GpuSampleCoordinate changed = coordinate;
    changed.component = 0;
    EXPECT_NE(GpuSampleStream::hash(coordinate), GpuSampleStream::hash(changed));

    changed = coordinate;
    changed.dimension = sampleDimensionIndex(SampleDimension::Light, 2);
    EXPECT_NE(GpuSampleStream::hash(coordinate), GpuSampleStream::hash(changed));
  }

  TEST(GpuSampleStream, NamedDimensionsUseStableDimensionIndices) {
    GpuSampleStream stream(/*seed=*/42, /*pixelIndex=*/19, /*primarySampleIndex=*/3);

    EXPECT_EQ(GpuSampleStream::sample2D(/*seed=*/42, /*pixelIndex=*/19,
                                        /*primarySampleIndex=*/3,
                                        /*dimension=*/sampleDimensionIndex(SampleDimension::Pixel)),
              stream.sample2D(SampleDimension::Pixel));
    EXPECT_DOUBLE_EQ(GpuSampleStream::sample1D(GpuSampleCoordinate{
                       /*seed=*/42,
                       /*pixelIndex=*/19,
                       /*primarySampleIndex=*/3,
                       /*dimension=*/sampleDimensionIndex(SampleDimension::Continuation, 4),
                       /*component=*/0}),
                     stream.sample1D(SampleDimension::Continuation, 4));
  }

  TEST(GpuSampleStream, NamedDimensionsMatchFixedVectors) {
    {
      GpuSampleStream stream(/*seed=*/0, /*pixelIndex=*/0, /*primarySampleIndex=*/0);

      expectSample2D("seed=0 pixel=0 primary=0 pixel", stream.sample2D(SampleDimension::Pixel),
                     /*expectedX=*/9151356u, /*expectedY=*/11420491u);
      expectSample2D("seed=0 pixel=0 primary=0 bsdf[0]", stream.sample2D(SampleDimension::BSDF),
                     /*expectedX=*/14513316u, /*expectedY=*/5515632u);
      expectSample2D("seed=0 pixel=0 primary=0 bsdf[2]", stream.sample2D(SampleDimension::BSDF, 2),
                     /*expectedX=*/6124461u, /*expectedY=*/14798912u);
      expectSample2D("seed=0 pixel=0 primary=0 light[0]", stream.sample2D(SampleDimension::Light),
                     /*expectedX=*/8860795u, /*expectedY=*/15298400u);
      expectSample2D(
        "seed=0 pixel=0 primary=0 light[bounce=1 light=2]",
        stream.sample2D(SampleDimension::Light, SampleStream::lightSampleIndex(/*bounce=*/1,
                                                                               /*lightIndex=*/2)),
        /*expectedX=*/8830170u, /*expectedY=*/13936716u);
      expectSample1D("seed=0 pixel=0 primary=0 continuation[0]",
                     stream.sample1D(SampleDimension::Continuation),
                     /*expected=*/9784475u);
      expectSample1D("seed=0 pixel=0 primary=0 continuation[3]",
                     stream.sample1D(SampleDimension::Continuation, 3),
                     /*expected=*/2244376u);
    }

    {
      GpuSampleStream stream(/*seed=*/42, /*pixelIndex=*/19, /*primarySampleIndex=*/3);

      expectSample2D("seed=42 pixel=19 primary=3 pixel", stream.sample2D(SampleDimension::Pixel),
                     /*expectedX=*/2146489u, /*expectedY=*/3461617u);
      expectSample2D("seed=42 pixel=19 primary=3 bsdf[0]", stream.sample2D(SampleDimension::BSDF),
                     /*expectedX=*/3233765u, /*expectedY=*/11552210u);
      expectSample2D("seed=42 pixel=19 primary=3 bsdf[2]",
                     stream.sample2D(SampleDimension::BSDF, 2),
                     /*expectedX=*/15755970u, /*expectedY=*/6611835u);
      expectSample2D("seed=42 pixel=19 primary=3 light[0]", stream.sample2D(SampleDimension::Light),
                     /*expectedX=*/3703532u, /*expectedY=*/12227597u);
      expectSample2D(
        "seed=42 pixel=19 primary=3 light[bounce=1 light=2]",
        stream.sample2D(SampleDimension::Light, SampleStream::lightSampleIndex(/*bounce=*/1,
                                                                               /*lightIndex=*/2)),
        /*expectedX=*/1388577u, /*expectedY=*/5983127u);
      expectSample1D("seed=42 pixel=19 primary=3 continuation[0]",
                     stream.sample1D(SampleDimension::Continuation),
                     /*expected=*/10884828u);
      expectSample1D("seed=42 pixel=19 primary=3 continuation[3]",
                     stream.sample1D(SampleDimension::Continuation, 3),
                     /*expected=*/2768189u);
    }
  }

  TEST(GpuSampleStream, SequentialReadsMatchPixelTimeLensDimensionOrder) {
    GpuSampleStream stream(/*seed=*/42, /*pixelIndex=*/19, /*primarySampleIndex=*/3);

    EXPECT_EQ(stream.sample2D(SampleDimension::Pixel), stream.next2D());
    EXPECT_DOUBLE_EQ(stream.sample1D(SampleDimension::Time), stream.next1D());
    EXPECT_EQ(stream.sample2D(SampleDimension::Lens), stream.next2D());
  }

  TEST(GpuSampleStream, PrimarySampleConsumesPixelAndTimeDimensions) {
    GpuSampleStream stream(/*seed=*/42, /*pixelIndex=*/19, /*primarySampleIndex=*/3);

    const SampleStream::PrimarySample primary = stream.primarySample();

    EXPECT_EQ(GpuSampleStream(/*seed=*/42, /*pixelIndex=*/19, /*primarySampleIndex=*/3)
                .sample2D(SampleDimension::Pixel),
              primary.pixel);
    EXPECT_DOUBLE_EQ(GpuSampleStream(/*seed=*/42, /*pixelIndex=*/19, /*primarySampleIndex=*/3)
                       .sample1D(SampleDimension::Time),
                     primary.time);
    EXPECT_EQ(GpuSampleStream(/*seed=*/42, /*pixelIndex=*/19, /*primarySampleIndex=*/3)
                .sample2D(SampleDimension::Lens),
              stream.next2D());
  }

  TEST(GpuSampleStream, SamplesStayInHalfOpenUnitInterval) {
    for (std::uint32_t dimension = 0; dimension != 32; ++dimension) {
      const Vector2d sample = GpuSampleStream::sample2D(/*seed=*/123, /*pixelIndex=*/456,
                                                        /*primarySampleIndex=*/7, dimension);
      EXPECT_GE(sample.x(), 0.0);
      EXPECT_LT(sample.x(), 1.0);
      EXPECT_GE(sample.y(), 0.0);
      EXPECT_LT(sample.y(), 1.0);
    }
  }
}
