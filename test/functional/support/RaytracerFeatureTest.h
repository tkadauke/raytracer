#ifndef RAYTRACER_FEATURE_TEST_H
#define RAYTRACER_FEATURE_TEST_H

#include "test/functional/support/FeatureTest.h"

#include "core/math/Vector.h"
#include "core/Color.h"
#include "core/Buffer.h"

namespace raytracer {
  class Primitive;
  class Camera;
  class Raytracer;
  class Material;
  class Scene;
}

namespace testing {
  class RaytracerFeatureTest : public FeatureTest<RaytracerFeatureTest> {
  protected:
    virtual void SetUp();
    virtual void TearDown();

    virtual void beforeThen();

  public:
    RaytracerFeatureTest();
    
    void add(std::shared_ptr<raytracer::Primitive> primitive);
    raytracer::Scene* scene();
    raytracer::Camera* camera();
    void setCamera(raytracer::Camera* camera);
    void setCamera(const Vector3d& position, const Vector3d& lookAt);
    void setView(const Vector3d& position, const Vector3d& lookAt);
    void render();
    void cancel();
    
    const Buffer<unsigned int>& buffer() const;
    void clear();
  
    bool colorPresent(const Colord& color);
    int colorCount(const Colord& color);
    unsigned int colorAt(int x, int y);
    void show();
  
    raytracer::Material* redDiffuse();
    void lookAtOrigin();
    void lookAway();
    void goFarAway();
    bool objectVisible();
    int objectSize();
    
    int previousObjectSize;

  private:
    raytracer::Scene* m_scene;
    raytracer::Camera* m_camera;
    std::shared_ptr<raytracer::Raytracer> m_raytracer;
    Buffer<unsigned int> m_buffer;
  };
}

#endif
