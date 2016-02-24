#include "gtest.h"
#include "raytracer/samplers/Sampler.h"

namespace SamplerTest {
  using namespace ::testing;
  using namespace raytracer;
  
  class ConcreteSampler : public Sampler {
  public:
    ConcreteSampler(int numSamples, int numSets)
      : Sampler(numSamples, numSets) {}

  protected:
    virtual std::vector<Vector2d> generateSet() const {
      std::vector<Vector2d> result;
      for (int i = 0; i != numSamples(); ++i)
        result.push_back(Vector2d::null());
      return result;
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
