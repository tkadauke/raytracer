#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "Scene.h"
#include "Light.h"
#include "test/mocks/MockSurface.h"
#include "test/mocks/MockLight.h"

namespace SceneTest {
  using namespace ::testing;
  
  TEST(Scene, ShouldInitializeAmbientColor) {
    Scene scene(Colord::white);
    ASSERT_EQ(Colord::white, scene.ambient());
  }
  
  TEST(Scene, ShouldHaveNoLightByDefault) {
    Scene scene(Colord::white);
    ASSERT_TRUE(scene.lights().empty());
  }
  
  TEST(Scene, ShouldAddLight) {
    Scene scene(Colord::white);
    Light* light = new Light(Vector3d(), Colord::white);
    scene.addLight(light);
    ASSERT_FALSE(scene.lights().empty());
    ASSERT_EQ(light, scene.lights().front());
  }
  
  TEST(Scene, ShouldDeleteLights) {
    Scene* scene = new Scene(Colord::white);
    MockLight* light = new MockLight(Vector3d(), Colord::white);
    scene->addLight(light);
    EXPECT_CALL(*light, destructorCall());
    delete scene;
  }
  
  TEST(Scene, ShouldReturnIntersectedSurface) {
    Scene scene(Colord::white);
    MockSurface* surface = new MockSurface;
    scene.add(surface);
    EXPECT_CALL(*surface, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(surface)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = scene.intersect(ray, hitPoints);
    
    ASSERT_EQ(surface, result);
  }
  
  TEST(Scene, ShouldNotReturnAnySurfaceIfThereIsNoIntersection) {
    Scene scene(Colord::white);
    MockSurface* surface = new MockSurface;
    scene.add(surface);
    EXPECT_CALL(*surface, intersect(_, _)).WillOnce(Return(static_cast<Surface*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = scene.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Scene, ShouldReturnClosestIntersectedSurfaceIfThereIsMoreThanOneCandidate) {
    Scene scene(Colord::white);
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    scene.add(surface1);
    scene.add(surface2);
    EXPECT_CALL(*surface1, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(5.0, Vector3d(), Vector3d())), Return(surface1)));
    EXPECT_CALL(*surface2, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(surface2)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = scene.intersect(ray, hitPoints);
    
    ASSERT_EQ(surface2, result);
  }
}
