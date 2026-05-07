#include <gtest/gtest.h>
#include "render/samplers/Sampler.h"
#include "render/samplers/SampleStream.h"

namespace SamplerTest {
  using namespace ::testing;
  using namespace render;

  class ConcreteSampler : public Sampler {
  protected:
    virtual std::vector<Vector2d> generateSet() {
      std::vector<Vector2d> result;
      for (int i = 0; i != numSamples(); ++i)
        result.push_back(Vector2d::null());
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

  TEST(SamplerStream, DefaultStreamReturnsSamplesFromCorrectSet) {
    IndexedSampler sampler;
    sampler.setup(4, 8);  // 4 samples per set, 8 sets

    // With pixelHash=0, dim=0, the lookup is set 0 → samples are
    // (0/100, 0), (1/100, 0), (2/100, 0), (3/100, 0).
    auto stream = sampler.stream(/*sampleIndex*/ 2, /*pixelHash*/ 0);
    auto sample = stream->next2D();
    ASSERT_DOUBLE_EQ(2.0 / 100.0, sample.x());
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
    stream->next2D();             // dim 0
    stream->next1D();             // dim 1
    auto third = stream->next2D();// dim 2 → set 2 → sample[0] = 2/100
    ASSERT_DOUBLE_EQ(2.0 / 100.0, third.x());
  }
}
