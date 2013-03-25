#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "primitives/Scene.h"
#include "Light.h"
#include "test/mocks/MockLight.h"

namespace SceneTest {
  using namespace ::testing;
  
  TEST(Scene, ShouldInitialize) {
    Scene scene;
    ASSERT_EQ(Colord::white(), scene.ambient());
  }
  
  TEST(Scene, ShouldInitializeAmbientColor) {
    Scene scene(Colord::white());
    ASSERT_EQ(Colord::white(), scene.ambient());
  }
  
  TEST(Scene, ShouldHaveNoLightByDefault) {
    Scene scene(Colord::white());
    ASSERT_TRUE(scene.lights().empty());
  }
  
  TEST(Scene, ShouldAddLight) {
    Scene scene(Colord::white());
    Light* light = new Light(Vector3d(), Colord::white());
    scene.addLight(light);
    ASSERT_FALSE(scene.lights().empty());
    ASSERT_EQ(light, scene.lights().front());
  }
  
  TEST(Scene, ShouldDeleteLights) {
    Scene* scene = new Scene(Colord::white());
    MockLight* light = new MockLight(Vector3d(), Colord::white());
    scene->addLight(light);
    EXPECT_CALL(*light, destructorCall());
    delete scene;
  }
}
