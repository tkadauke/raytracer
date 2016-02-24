#include "gtest.h"
#include "raytracer/samplers/Sampler.h"

namespace SamplerTest {
  using namespace ::testing;
  using namespace raytracer;
  
  class ConcreteSampler : public Sampler {
  public:
    ConcreteSampler(int numSamples, int numSets)
      : Sampler(numSamples, numSets)
    {
      for (int s = 0; s != numSets; s++) {
        std::vector<Vector2d> result;
        for (int i = 0; i != numSamples; ++i)
          result.push_back(Vector2d::null());
        addSet(result);
      }
    }
  };
  
  TEST(Sampler, ShouldConstructWithParameters) {
    ConcreteSampler sampler(4, 123);
    ASSERT_EQ(4, sampler.numSamples());
  }
  
  TEST(Sampler, ShouldReturnSampleSet) {
    ConcreteSampler sampler(4, 123);
    auto set = sampler.sampleSet();
    ASSERT_EQ(4u, set.size());
  }
}
