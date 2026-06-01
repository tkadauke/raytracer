#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "core/math/Ray.h"
#include "render/State.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/lights/PointLight.h"

namespace SceneTest {
  using namespace ::testing;
  using namespace render;
  using namespace render;
  using namespace render;

  TEST(Scene, ShouldInitialize) {
    Scene scene;
    ASSERT_EQ(Colord::white(), scene.ambient());
  }

  TEST(Scene, ShouldInitializeAmbientColor) {
    Scene scene(Colord::white());
    ASSERT_EQ(Colord::white(), scene.ambient());
  }

  TEST(Scene, ShouldInitializeBackgroundColor) {
    Scene scene(Colord::white());
    ASSERT_EQ(Colord::white(), scene.background());
  }

  TEST(Scene, ShouldHaveNoLightByDefault) {
    Scene scene(Colord::white());
    ASSERT_TRUE(scene.lights().empty());
  }

  TEST(Scene, ShouldAddLight) {
    Scene scene(Colord::white());
    auto light = std::make_shared<PointLight>(Vector3d(), Colord::white());
    scene.addLight(light);
    ASSERT_FALSE(scene.lights().empty());
    ASSERT_EQ(light, scene.lights().front());
  }

  TEST(Scene, ShouldOnlyOccludeFiniteLightDistanceBeforeTheLight) {
    Scene scene(Colord::white());
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 5), 0.5));

    State state;
    ASSERT_FALSE(scene.occludes(Rayd(Vector3d::null, Vector3d(0, 0, 1)), state, 4.0));
  }

  TEST(Scene, ShouldOccludeFiniteLightDistanceBeforeAnOccluder) {
    Scene scene(Colord::white());
    scene.add(std::make_shared<Sphere>(Vector3d(0, 0, 5), 0.5));

    State state;
    ASSERT_TRUE(scene.occludes(Rayd(Vector3d::null, Vector3d(0, 0, 1)), state, 5.0));
  }
}
