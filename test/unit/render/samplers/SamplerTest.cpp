#include <gtest/gtest.h>
#include "render/SamplingSeed.h"
#include "render/samplers/RegularSampler.h"
#include "render/samplers/SampleStream.h"
#include "render/samplers/Sampler.h"

namespace SamplerTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;

  class ConcreteSampler : public Sampler {
  protected:
    virtual std::vector<Vector2d> generateSet() {
      std::vector<Vector2d> result;
      for (int i = 0; i != numSamples(); ++i)
        result.push_back(Vector2d::null);
      return result;
    }
  };

  // Sampler that produces a recognisable pattern: set s holds points
  // ((s + i) / 100, 0.0) for i in [0, numSamples). That lets the
  // stream tests assert which set was looked up by reading the
  // returned coordinate back.
  class IndexedSampler : public Sampler {
  protected:
    virtual std::vector<Vector2d> generateSet() {
      std::vector<Vector2d> result;
      for (int i = 0; i != numSamples(); ++i)
        result.push_back(Vector2d((m_setIdx + i) / 100.0, 0.0));
      ++m_setIdx;
      return result;
    }

  private:
    int m_setIdx = 0;
  };

  class MarkerSampleStream : public SampleStream {
  public:
    Vector2d next2D() override {
      return Vector2d(0.25, 0.75);
    }

    double next1D() override {
      return 0.125;
    }

    Vector2d sample2D(SampleDimension, std::uint64_t = 0) override {
      return Vector2d(0.5, 0.625);
    }

    double sample1D(SampleDimension, std::uint64_t = 0) override {
      return 0.875;
    }
  };

  class CustomStreamSampler : public Sampler {
  public:
    std::unique_ptr<SampleStream> stream(int, std::uint64_t) const override {
      ++m_streamCalls;
      return std::make_unique<MarkerSampleStream>();
    }

    int streamCalls() const {
      return m_streamCalls;
    }

  protected:
    std::vector<Vector2d> generateSet() override {
      std::vector<Vector2d> result;
      for (int i = 0; i != numSamples(); ++i)
        result.push_back(Vector2d::null);
      return result;
    }

  private:
    mutable int m_streamCalls{0};
  };

  TEST(Sampler, ShouldSetupWithNumberOfSamples) {
    ConcreteSampler sampler;
    sampler.setup(4, 123);
    ASSERT_EQ(4, sampler.numSamples());
  }

  TEST(Sampler, ShouldReturnSampleSet) {
    ConcreteSampler sampler;
    sampler.setup(4, 123);
    auto set = sampler.sampleSet();
    ASSERT_EQ(4u, set.size());
  }

  TEST(Sampler, ShouldExposeNumSets) {
    ConcreteSampler sampler;
    sampler.setup(4, 7);
    ASSERT_EQ(7, sampler.numSets());
  }

  // numSamples() reflects the actual produced count, not the
  // requested count. Concrete samplers like render::RegularSampler floor
  // sqrt(N) and may produce fewer points than requested; the average
  // in `Camera::plot` divides by numSamples(), so it has to match
  // the iteration count to come out right.
  TEST(NullSampleStream, ReturnsCenterFor2D) {
    render::NullSampleStream stream;
    auto sample = stream.next2D();
    ASSERT_DOUBLE_EQ(0.5, sample.x());
    ASSERT_DOUBLE_EQ(0.5, sample.y());
  }

  TEST(NullSampleStream, ReturnsCenterFor1D) {
    render::NullSampleStream stream;
    ASSERT_DOUBLE_EQ(0.5, stream.next1D());
  }

  TEST(NullSampleStream, AlwaysReturnsCenterRegardlessOfDimension) {
    render::NullSampleStream stream;
    for (int i = 0; i != 10; ++i) {
      auto two = stream.next2D();
      ASSERT_DOUBLE_EQ(0.5, two.x());
      ASSERT_DOUBLE_EQ(0.5, two.y());
      ASSERT_DOUBLE_EQ(0.5, stream.next1D());
    }
  }

  TEST(NullSampleStream, NamedDimensionsReturnCenter) {
    render::NullSampleStream stream;

    auto lens = stream.sample2D(SampleDimension::Lens);
    ASSERT_DOUBLE_EQ(0.5, lens.x());
    ASSERT_DOUBLE_EQ(0.5, lens.y());
    ASSERT_DOUBLE_EQ(0.5, stream.sample1D(SampleDimension::Continuation, 4));
  }

  TEST(SampleDimension, ShouldExposeStableDimensionIndices) {
    ASSERT_EQ(0u, sampleDimensionIndex(SampleDimension::Pixel));
    ASSERT_EQ(1u, sampleDimensionIndex(SampleDimension::Time));
    ASSERT_EQ(2u, sampleDimensionIndex(SampleDimension::Lens));
    ASSERT_EQ(3u, sampleDimensionIndex(SampleDimension::BSDF));
    ASSERT_EQ(4u, sampleDimensionIndex(SampleDimension::Light));
    ASSERT_EQ(5u, sampleDimensionIndex(SampleDimension::Continuation));

    ASSERT_EQ(6u, sampleDimensionIndex(SampleDimension::BSDF, 1));
    ASSERT_EQ(7u, sampleDimensionIndex(SampleDimension::Light, 1));
    ASSERT_EQ(8u, sampleDimensionIndex(SampleDimension::Continuation, 1));
  }

  TEST(SamplerStream, DefaultStreamReturnsSamplesFromCorrectSet) {
    IndexedSampler sampler;
    sampler.setup(4, 8); // 4 samples per set, 8 sets

    // With pixelHash=0, dim=0, the lookup is set 0 → samples are
    // (0/100, 0), (1/100, 0), (2/100, 0), (3/100, 0).
    auto stream = sampler.stream(/*sampleIndex*/ 2, /*pixelHash*/ 0);
    auto sample = stream->next2D();
    ASSERT_DOUBLE_EQ(2.0 / 100.0, sample.x());
  }

  TEST(SamplerStream, SharedStreamReturnsSameSequenceAsUniqueStream) {
    IndexedSampler sampler;
    sampler.setup(4, 8);

    auto uniqueStream = sampler.stream(/*sampleIndex*/ 2, /*pixelHash*/ 3);
    auto sharedStream = sampler.sharedStream(/*sampleIndex*/ 2, /*pixelHash*/ 3);

    ASSERT_NE(nullptr, sharedStream);
    ASSERT_EQ(uniqueStream->next2D(), sharedStream->next2D());
    ASSERT_DOUBLE_EQ(uniqueStream->next1D(), sharedStream->next1D());
    ASSERT_EQ(uniqueStream->sample2D(SampleDimension::BSDF, 1),
              sharedStream->sample2D(SampleDimension::BSDF, 1));
  }

  TEST(SamplerStream, PrimarySampleMatchesSequentialPixelAndTimeDimensions) {
    IndexedSampler sampler;
    sampler.setup(4, 8);

    auto stream = sampler.stream(/*sampleIndex*/ 2, /*pixelHash*/ 3);
    const auto primary = stream->primarySample();

    EXPECT_DOUBLE_EQ(5.0 / 100.0, primary.pixel.x());
    EXPECT_DOUBLE_EQ(0.0, primary.pixel.y());
    EXPECT_DOUBLE_EQ(6.0 / 100.0, primary.time);
    EXPECT_DOUBLE_EQ(7.0 / 100.0, stream->next2D().x());
  }

  TEST(SamplerStream, PrimarySampleUsesDefaultSequentialReadsForCustomStreams) {
    MarkerSampleStream stream;

    const auto primary = stream.primarySample();

    EXPECT_EQ(Vector2d(0.25, 0.75), primary.pixel);
    EXPECT_DOUBLE_EQ(0.125, primary.time);
  }

  TEST(SamplerStream, StorageKeepsSamplerBackedStreamsStable) {
    IndexedSampler sampler;
    sampler.setup(4, 64);
    SampleStreamStorage storage;
    storage.reserve(32);

    SampleStream* first = storage.appendSamplerBacked(sampler, /*sampleIndex*/ 2,
                                                      /*pixelHash*/ 3);
    for (int i = 0; i != 32; ++i)
      storage.appendSamplerBacked(sampler, i % sampler.numSamples(), i);

    ASSERT_NE(nullptr, first);
    ASSERT_DOUBLE_EQ(5.0 / 100.0, first->next2D().x());
    ASSERT_DOUBLE_EQ(6.0 / 100.0, first->next1D());
  }

  TEST(SamplerStream, StorageKeepsUnreservedSamplerBackedStreamsStable) {
    IndexedSampler sampler;
    sampler.setup(4, 64);
    SampleStreamStorage storage;

    SampleStream* first = storage.appendSamplerBacked(sampler, /*sampleIndex*/ 2,
                                                      /*pixelHash*/ 3);
    for (int i = 0; i != 32; ++i)
      storage.appendSamplerBacked(sampler, i % sampler.numSamples(), i);

    ASSERT_NE(nullptr, first);
    ASSERT_DOUBLE_EQ(5.0 / 100.0, first->next2D().x());
    ASSERT_DOUBLE_EQ(6.0 / 100.0, first->next1D());
  }

  TEST(SamplerStream, StorageKeepsSamplerBackedStreamsStableAfterLateReserve) {
    IndexedSampler sampler;
    sampler.setup(4, 64);
    SampleStreamStorage storage;
    storage.reserve(1);

    SampleStream* first = storage.appendSamplerBacked(sampler, /*sampleIndex*/ 2,
                                                      /*pixelHash*/ 3);
    storage.reserve(64);
    for (int i = 0; i != 32; ++i)
      storage.appendSamplerBacked(sampler, i % sampler.numSamples(), i);

    ASSERT_NE(nullptr, first);
    ASSERT_DOUBLE_EQ(5.0 / 100.0, first->next2D().x());
    ASSERT_DOUBLE_EQ(6.0 / 100.0, first->next1D());
  }

  TEST(SamplerStream, BuiltInAppendStreamMatchesSharedStream) {
    RegularSampler sampler;
    sampler.setup(4, 16);
    SampleStreamStorage storage;

    SampleStream* retained = sampler.appendStream(storage, /*sampleIndex*/ 2,
                                                  /*pixelHash*/ 7);
    auto shared = sampler.sharedStream(/*sampleIndex*/ 2, /*pixelHash*/ 7);

    ASSERT_NE(nullptr, retained);
    ASSERT_NE(nullptr, shared);
    ASSERT_EQ(shared->next2D(), retained->next2D());
    ASSERT_DOUBLE_EQ(shared->next1D(), retained->next1D());
    ASSERT_EQ(shared->sample2D(SampleDimension::BSDF, 1),
              retained->sample2D(SampleDimension::BSDF, 1));
  }

  TEST(SamplerStream, AppendStreamPreservesCustomStreamOverrides) {
    CustomStreamSampler sampler;
    sampler.setup(4, 8);
    SampleStreamStorage storage;

    SampleStream* stream = sampler.appendStream(storage, /*sampleIndex*/ 2,
                                                /*pixelHash*/ 3);

    ASSERT_NE(nullptr, stream);
    EXPECT_EQ(1, sampler.streamCalls());
    ASSERT_EQ(Vector2d(0.25, 0.75), stream->next2D());
    ASSERT_DOUBLE_EQ(0.125, stream->next1D());
    ASSERT_EQ(Vector2d(0.5, 0.625), stream->sample2D(SampleDimension::BSDF));
    ASSERT_DOUBLE_EQ(0.875, stream->sample1D(SampleDimension::Light));
  }

  TEST(SamplerStream, ConsecutiveDimensionsReadFromDifferentSets) {
    IndexedSampler sampler;
    sampler.setup(4, 8);

    // pixelHash=0; first next2D consumes dim 0 → set 0 → sample[0] = 0/100.
    // Second next2D consumes dim 1 → set 1 → sample[0] = (1+0)/100 = 1/100.
    auto stream = sampler.stream(0, 0);
    auto first = stream->next2D();
    auto second = stream->next2D();
    ASSERT_DOUBLE_EQ(0.0 / 100.0, first.x());
    ASSERT_DOUBLE_EQ(1.0 / 100.0, second.x());
  }

  TEST(SamplerStream, PixelHashOffsetsSetLookup) {
    IndexedSampler sampler;
    sampler.setup(4, 8);

    // pixelHash=3, dim=0 → set 3 → sample[0] = 3/100.
    auto stream = sampler.stream(0, 3);
    auto sample = stream->next2D();
    ASSERT_DOUBLE_EQ(3.0 / 100.0, sample.x());
  }

  TEST(SamplerStream, PixelHashWrapsModNumSets) {
    IndexedSampler sampler;
    sampler.setup(4, 8);

    // pixelHash=11 wraps to set 3 (11 mod 8 = 3).
    auto stream = sampler.stream(0, 11);
    auto sample = stream->next2D();
    ASSERT_DOUBLE_EQ(3.0 / 100.0, sample.x());
  }

  TEST(SamplerStream, Next1DUsesXCoordinate) {
    IndexedSampler sampler;
    sampler.setup(4, 8);

    auto stream = sampler.stream(2, 0);
    double v = stream->next1D();
    // Same lookup as the next2D test above — set 0, sample 2 → x = 2/100.
    ASSERT_DOUBLE_EQ(2.0 / 100.0, v);
  }

  TEST(SamplerStream, MixingNext2DAndNext1DAdvancesDimension) {
    IndexedSampler sampler;
    sampler.setup(4, 8);

    auto stream = sampler.stream(0, 0);
    stream->next2D();              // dim 0
    stream->next1D();              // dim 1
    auto third = stream->next2D(); // dim 2 → set 2 → sample[0] = 2/100
    ASSERT_DOUBLE_EQ(2.0 / 100.0, third.x());
  }

  TEST(SamplerStream, NamedDimensionsMatchLegacyCameraDimensionOrder) {
    IndexedSampler sampler;
    sampler.setup(4, 8);

    auto stream = sampler.stream(0, 0);

    ASSERT_DOUBLE_EQ(0.0 / 100.0, stream->sample2D(SampleDimension::Pixel).x());
    ASSERT_DOUBLE_EQ(1.0 / 100.0, stream->sample1D(SampleDimension::Time));
    ASSERT_DOUBLE_EQ(2.0 / 100.0, stream->sample2D(SampleDimension::Lens).x());
  }

  TEST(SamplerStream, PathTracingDimensionsDoNotReuseTheSamePattern) {
    IndexedSampler sampler;
    sampler.setup(4, 16);

    auto stream = sampler.stream(0, 0);

    ASSERT_DOUBLE_EQ(3.0 / 100.0, stream->sample2D(SampleDimension::BSDF, 0).x());
    ASSERT_DOUBLE_EQ(4.0 / 100.0, stream->sample2D(SampleDimension::Light, 0).x());
    ASSERT_DOUBLE_EQ(5.0 / 100.0, stream->sample1D(SampleDimension::Continuation, 0));

    ASSERT_DOUBLE_EQ(6.0 / 100.0, stream->sample2D(SampleDimension::BSDF, 1).x());
    ASSERT_DOUBLE_EQ(7.0 / 100.0, stream->sample2D(SampleDimension::Light, 1).x());
    ASSERT_DOUBLE_EQ(8.0 / 100.0, stream->sample1D(SampleDimension::Continuation, 1));
  }

  TEST(RegularSampler, CameraDimensionsRemainRegularAcrossPixels) {
    RegularSampler sampler;
    sampler.setup(1, 8);

    auto first = sampler.stream(/*sampleIndex=*/0, /*pixelHash=*/0);
    auto second = sampler.stream(/*sampleIndex=*/0, /*pixelHash=*/17);

    ASSERT_EQ(Vector2d(0.5, 0.5), first->sample2D(SampleDimension::Pixel));
    ASSERT_DOUBLE_EQ(0.5, first->sample1D(SampleDimension::Time));
    ASSERT_EQ(Vector2d(0.5, 0.5), first->sample2D(SampleDimension::Lens));

    ASSERT_EQ(Vector2d(0.5, 0.5), second->sample2D(SampleDimension::Pixel));
    ASSERT_DOUBLE_EQ(0.5, second->sample1D(SampleDimension::Time));
    ASSERT_EQ(Vector2d(0.5, 0.5), second->sample2D(SampleDimension::Lens));
  }

  TEST(RegularSampler, PathTracingDimensionsAreScrambledAcrossPixels) {
    RegularSampler sampler;
    sampler.setup(1, 8);

    auto first = sampler.stream(/*sampleIndex=*/0, /*pixelHash=*/0);
    auto second = sampler.stream(/*sampleIndex=*/0, /*pixelHash=*/17);

    const Vector2d firstBsdf = first->sample2D(SampleDimension::BSDF);
    const Vector2d secondBsdf = second->sample2D(SampleDimension::BSDF);

    EXPECT_NE(Vector2d(0.5, 0.5), firstBsdf);
    EXPECT_NE(Vector2d(0.5, 0.5), secondBsdf);
    EXPECT_NE(firstBsdf, secondBsdf);
  }

  TEST(RegularSampler, PathTracingDimensionScrambleIsStable) {
    RegularSampler sampler;
    sampler.setup(1, 8);

    auto first = sampler.stream(/*sampleIndex=*/0, /*pixelHash=*/17);
    auto second = sampler.stream(/*sampleIndex=*/0, /*pixelHash=*/17);

    EXPECT_EQ(first->sample2D(SampleDimension::BSDF, 2),
              second->sample2D(SampleDimension::BSDF, 2));
    EXPECT_DOUBLE_EQ(first->sample1D(SampleDimension::Continuation, 3),
                     second->sample1D(SampleDimension::Continuation, 3));
  }

  TEST(SamplerStream, NamedDimensionReadsDoNotAdvanceSequentialCursor) {
    IndexedSampler sampler;
    sampler.setup(4, 8);

    auto stream = sampler.stream(0, 0);

    ASSERT_DOUBLE_EQ(3.0 / 100.0, stream->sample2D(SampleDimension::BSDF).x());
    ASSERT_DOUBLE_EQ(0.0 / 100.0, stream->next2D().x());
    ASSERT_DOUBLE_EQ(1.0 / 100.0, stream->next1D());
    ASSERT_DOUBLE_EQ(2.0 / 100.0, stream->next2D().x());
  }

  TEST(SamplingSeed, DerivesStableHierarchicalSeeds) {
    const std::uint64_t tile = SamplingSeed::tileSeed(1234, 5);
    const std::uint64_t pixel = SamplingSeed::pixelSeed(tile, 17, 23);
    const std::uint64_t sample = SamplingSeed::sampleSeed(pixel, 3);

    EXPECT_EQ(tile, SamplingSeed::tileSeed(1234, 5));
    EXPECT_EQ(pixel, SamplingSeed::pixelSeed(tile, 17, 23));
    EXPECT_EQ(sample, SamplingSeed::sampleSeed(pixel, 3));

    EXPECT_NE(tile, SamplingSeed::tileSeed(1234, 6));
    EXPECT_NE(pixel, SamplingSeed::pixelSeed(tile, 18, 23));
    EXPECT_NE(sample, SamplingSeed::sampleSeed(pixel, 4));
  }

  TEST(SamplerStream, SeededPixelHashesSelectStableSets) {
    IndexedSampler sampler;
    sampler.setup(4, 64);

    const auto tileSeed = SamplingSeed::tileSeed(1234, 2);
    const auto pixelSeed = SamplingSeed::pixelSeed(tileSeed, 4, 9);

    auto first = sampler.stream(0, pixelSeed)->next2D();
    auto second = sampler.stream(0, pixelSeed)->next2D();

    EXPECT_EQ(first, second);
  }
}
