#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "engine/raytracer/Raytracer.h"
#include "render/RenderEngine.h"
#include "render/lights/PointLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/tonemap/TonemapFactory.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>

using namespace testing;
using namespace render;

namespace TonemapMonotonicityTest {

  // Subclass exposes a tonemap-by-name knob that the engine factory
  // applies to the freshly built Raytracer. Each render() recreates
  // the engine, so each `then` step picks up the latest selection.
  struct TonemapMonotonicityTest : public RaytracerFeatureTest {
    std::string tonemapName;
    std::map<std::string, unsigned int> recordedMax;

    std::shared_ptr<RenderEngine> createEngine() override {
      auto engine = RaytracerFeatureTest::createEngine();
      if (!tonemapName.empty()) {
        if (auto* rt = dynamic_cast<engine::raytracer::Raytracer*>(engine.get())) {
          rt->setTonemap(TonemapFactory::self().createShared(tonemapName));
        }
      }
      return engine;
    }

    unsigned int maxChannel() const {
      unsigned int maxVal = 0;
      for (int y = 0; y < buffer().height(); ++y) {
        for (int x = 0; x < buffer().width(); ++x) {
          const unsigned int packed = colorAt(x, y);
          maxVal = std::max(maxVal, (packed >> 16) & 0xffu);
          maxVal = std::max(maxVal, (packed >> 8) & 0xffu);
          maxVal = std::max(maxVal, packed & 0xffu);
        }
      }
      return maxVal;
    }
  };

  GIVEN(EngineFeatureTest, "a unit white sphere at the origin with no ambient") {
    auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);
    sphere->setMaterial(std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(1.0, 1.0, 1.0))));
    test->add(sphere);
    test->scene()->setAmbient(Colord(0, 0, 0));
  }

  GIVEN(EngineFeatureTest,
        "a bright white point light at \\(([\\-\\d.]+), ([\\-\\d.]+), ([\\-\\d.]+)\\)") {
    test->scene()->addLight(std::make_shared<PointLight>(
      Vector3d(std::stod(match[1]), std::stod(match[2]), std::stod(match[3])),
      Colord(5, 5, 5)));
  }

  WHEN(EngineFeatureTest, "i select the (Linear|ACES|Reinhard) tonemap") {
    if (auto* tm = dynamic_cast<TonemapMonotonicityTest*>(test)) {
      tm->tonemapName = match[1];
    }
  }

  THEN(EngineFeatureTest, "the (Linear|ACES|Reinhard) render saturates the max channel") {
    auto* tm = dynamic_cast<TonemapMonotonicityTest*>(test);
    ASSERT_NE(nullptr, tm);
    const std::string name = match[1];
    tm->recordedMax[name] = tm->maxChannel();
    EXPECT_EQ(255u, tm->recordedMax[name])
      << name << " pass-through must expose an over-1.0 HDR pixel "
                 "by clamping to the 8-bit display maximum.";
  }

  THEN(EngineFeatureTest,
       "the (Linear|ACES|Reinhard) render max is at most that of (Linear|ACES|Reinhard)") {
    auto* tm = dynamic_cast<TonemapMonotonicityTest*>(test);
    ASSERT_NE(nullptr, tm);
    const std::string name = match[1];
    const std::string ref = match[2];
    tm->recordedMax[name] = tm->maxChannel();
    ASSERT_TRUE(tm->recordedMax.count(ref))
      << "no recorded max for " << ref << "; sequence the scenario so " << ref
      << " is rendered before " << name;
    EXPECT_GE(tm->recordedMax[ref], tm->recordedMax[name])
      << ref << " max=" << tm->recordedMax[ref] << " must be >= " << name
      << " max=" << tm->recordedMax[name];
  }

  THEN(EngineFeatureTest, "the (Linear|ACES|Reinhard) render compresses below saturation") {
    auto* tm = dynamic_cast<TonemapMonotonicityTest*>(test);
    ASSERT_NE(nullptr, tm);
    const std::string name = match[1];
    tm->recordedMax[name] = tm->maxChannel();
    EXPECT_GT(tm->recordedMax[name], 0u) << "scene must render at least one non-black pixel.";
    EXPECT_LT(tm->recordedMax[name], 255u)
      << name << " must compress this HDR scene below 255.";
  }

  // Three stacked white point lights make the unmapped surface
  // contribution exceed 1.0, so each operator's max-channel sits at a
  // distinguishable point on the [0, 255] axis. The shipped Narkowicz
  // ACES fit has brighter midtones than Reinhard, so the pinned
  // ordering is Linear ≥ ACES ≥ Reinhard.
  TEST_F(TonemapMonotonicityTest, BuiltInOperatorsHaveMonotoneMaxChannel) {
    given("a unit white sphere at the origin with no ambient");
    given("a bright white point light at (0, 3, -3)");
    given("a bright white point light at (3, 0, -3)");
    given("a bright white point light at (-3, 0, -3)");
    when("i look at the origin");
    when("i select the Linear tonemap");
    then("the Linear render saturates the max channel");
    when("i select the ACES tonemap");
    then("the ACES render max is at most that of Linear");
    when("i select the Reinhard tonemap");
    then("the Reinhard render max is at most that of ACES");
    then("the Reinhard render compresses below saturation");
  }

}
