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
    raytracer::Scene* scene() const;
    std::shared_ptr<raytracer::Camera> camera();
    void setCamera(std::shared_ptr<raytracer::Camera> camera);
    void setCamera(const Vector3d& position, const Vector3d& lookAt);
    void setView(const Vector3d& position, const Vector3d& lookAt);
    void render();
    void cancel();

    const Buffer<unsigned int>& buffer() const;
    void clear();

    bool colorPresent(const Colord& color) const;
    int colorCount(const Colord& color) const;
    unsigned int colorAt(int x, int y) const;
    void show() const;

    std::shared_ptr<raytracer::Material> redDiffuse() const;
    void lookAtOrigin();
    void lookAway();
    void goFarAway();
    bool objectVisible() const;
    int objectSize() const;

    int previousObjectSize;

  private:
    raytracer::Scene* m_scene;
    std::shared_ptr<raytracer::Camera> m_camera;
    std::shared_ptr<raytracer::Raytracer> m_raytracer;
    Buffer<unsigned int> m_buffer;
  };
}

#endif
