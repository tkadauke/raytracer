#include <gtest/gtest.h>

#include "render/samplers/GpuSampleStream.h"

namespace GpuSampleStreamTest {
  using namespace render;

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
