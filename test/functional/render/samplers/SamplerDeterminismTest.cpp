#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/cameras/Camera.h"
#include "render/samplers/RegularSampler.h"
#include "render/viewplanes/ViewPlane.h"

#include <vector>

using namespace testing;
using namespace render;

namespace SamplerDeterminismTest {

  struct SamplerDeterminismTest : public RaytracerFeatureTest {
    static std::vector<unsigned int> snapshot(const Buffer<unsigned int>& buf) {
      std::vector<unsigned int> pixels;
      pixels.reserve(static_cast<size_t>(buf.width()) * static_cast<size_t>(buf.height()));
      for (int y = 0; y < buf.height(); ++y)
        for (int x = 0; x < buf.width(); ++x)
          pixels.push_back(buf[y][x]);
      return pixels;
    }
  };

  GIVEN(EngineFeatureTest, "a regular sampler with ([0-9]+) samples") {
    auto sampler = std::make_shared<RegularSampler>();
    sampler->setup(std::stoi(match[1]), 83);
    test->camera()->viewPlane()->setSampler(sampler);
  }

  // beforeThen() already rendered once. Re-render in place and compare
  // the two pixel snapshots — RegularSampler is purely deterministic, so
  // the buffers must match bit-for-bit.
  THEN(EngineFeatureTest, "rendering twice produces bit-identical buffers") {
    auto* fixture = dynamic_cast<SamplerDeterminismTest*>(test);
    ASSERT_NE(nullptr, fixture);
    auto first = fixture->snapshot(test->buffer());
    test->render();
    auto second = fixture->snapshot(test->buffer());
    EXPECT_EQ(first, second);
  }

  TEST_F(SamplerDeterminismTest, RegularSamplerProducesBitIdenticalRenders) {
    given("a centered sphere");
    given("a regular sampler with 9 samples");
    when("i look at the origin");
    then("rendering twice produces bit-identical buffers");
  }

}
