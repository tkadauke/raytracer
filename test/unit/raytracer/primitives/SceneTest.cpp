#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "raytracer/primitives/Scene.h"
#include "raytracer/lights/Light.h"
#include "test/mocks/raytracer/MockLight.h"

namespace SceneTest {
  using namespace ::testing;
  using namespace raytracer;
  
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
    auto light = new Light(Vector3d(), Colord::white());
    scene.addLight(light);
    ASSERT_FALSE(scene.lights().empty());
    ASSERT_EQ(light, scene.lights().front());
  }
  
  TEST(Scene, ShouldDeleteLights) {
    auto scene = new Scene(Colord::white());
    auto light = new MockLight(Vector3d(), Colord::white());
    scene->addLight(light);
    EXPECT_CALL(*light, destructorCall());
    delete scene;
  }
}
