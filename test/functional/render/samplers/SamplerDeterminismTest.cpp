#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/cameras/Camera.h"
#include "render/samplers/RandomSampler.h"
#include "render/samplers/RegularSampler.h"
#include "render/viewplanes/ViewPlane.h"
#include "engine/raytracer/Raytracer.h"

#include <cstdint>
#include <optional>
#include <vector>

using namespace testing;
using namespace render;

namespace SamplerDeterminismTest {

  struct SamplerDeterminismTest : public RaytracerFeatureTest {
    std::optional<std::uint64_t> renderSeed;

    std::shared_ptr<render::RenderEngine> createEngine() override {
      auto engine = RaytracerFeatureTest::createEngine();
      if (renderSeed) {
        auto* raytracer = dynamic_cast<engine::raytracer::Raytracer*>(engine.get());
        if (raytracer)
          raytracer->setSamplingSeed(*renderSeed);
      }
      return engine;
    }

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

  GIVEN(EngineFeatureTest, "a seeded random sampler with ([0-9]+) samples and seed ([0-9]+)") {
    auto sampler = std::make_shared<RandomSampler>();
    sampler->setup(std::stoi(match[1]), 83, std::stoull(match[2]));
    test->camera()->viewPlane()->setSampler(sampler);
  }

  GIVEN(EngineFeatureTest, "raytracer sampling seed ([0-9]+)") {
    auto* fixture = dynamic_cast<SamplerDeterminismTest*>(test);
    ASSERT_NE(nullptr, fixture);
    fixture->renderSeed = std::stoull(match[1]);
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

  TEST_F(SamplerDeterminismTest, SeededRandomSamplerProducesBitIdenticalRenders) {
    given("a centered sphere");
    given("a seeded random sampler with 16 samples and seed 9001");
    given("raytracer sampling seed 424242");
    when("i look at the origin");
    then("rendering twice produces bit-identical buffers");
  }

}
