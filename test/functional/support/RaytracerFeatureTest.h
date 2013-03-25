#ifndef RAYTRACER_FEATURE_TEST_H
#define RAYTRACER_FEATURE_TEST_H

#include "test/functional/support/FeatureTest.h"

#include "math/Vector.h"
#include "Color.h"
#include "Buffer.h"

class Primitive;
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
    
    void add(Primitive* primitive);
    Scene* scene();
    Camera* camera();
    void setCamera(Camera* camera);
    void setCamera(const Vector3d& position, const Vector3d& lookAt);
    void setView(const Vector3d& position, const Vector3d& lookAt);
    void render();
    void cancel();
    
    const Buffer<unsigned int>& buffer() const;
    void clear();
  
    bool colorPresent(const Colord& color);
    int colorCount(const Colord& color);
    int colorAt(int x, int y);
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
    Buffer<unsigned int> m_buffer;
  };
}

#endif
