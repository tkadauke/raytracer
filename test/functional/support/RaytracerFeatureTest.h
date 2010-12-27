#ifndef RAYTRACER_FEATURE_TEST_H
#define RAYTRACER_FEATURE_TEST_H

#include "test/functional/support/FeatureTest.h"

#include "math/Vector.h"
#include "Color.h"
#include "Buffer.h"

class Surface;
class Camera;
class Raytracer;
class Material;
class Scene;

namespace testing {
  class RaytracerFeatureTest : public FeatureTest<RaytracerFeatureTest> {
  protected:
    virtual void SetUp();
    virtual void TearDown();

    virtual void beforeThen();

  public:
    RaytracerFeatureTest();
    
    void add(Surface* surface);
    Camera* camera();
    void setCamera(Camera* camera);
    void setCamera(const Vector3d& position, const Vector3d& lookAt);
    void setView(const Vector3d& position, const Vector3d& lookAt);
    void render();
  
    bool colorPresent(const Colord& color);
    int colorCount(const Colord& color);
    void show();
  
    Material* redDiffuse();
    void lookAtOrigin();
    void lookAway();
    void goFarAway();
    bool objectVisible();
    int objectSize();
    
    int previousObjectSize;

  private:
    Scene* m_scene;
    Camera* m_camera;
    Raytracer* m_raytracer;
    Buffer m_buffer;
  };
}

#endif
