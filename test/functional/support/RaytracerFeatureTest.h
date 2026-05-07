#ifndef RAYTRACER_FEATURE_TEST_H
#define RAYTRACER_FEATURE_TEST_H

#include "test/functional/support/FeatureTest.h"

#include "core/math/Vector.h"
#include "core/Color.h"
#include "core/Buffer.h"

namespace render {
  class Material;
}

namespace render {
  class Camera;
}

namespace render {
  class Primitive;
  class Scene;
}

namespace engine::raytracer {
  class Raytracer;
}

namespace testing {
  class RaytracerFeatureTest : public FeatureTest<RaytracerFeatureTest> {
  protected:
    virtual void SetUp();
    virtual void TearDown();

    virtual void beforeThen();

  public:
    RaytracerFeatureTest();

    void add(std::shared_ptr<render::Primitive> primitive);
    render::Scene* scene() const;
    std::shared_ptr<render::Camera> camera();
    void setCamera(std::shared_ptr<render::Camera> camera);
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

    std::shared_ptr<render::Material> redDiffuse() const;
    void lookAtOrigin();
    void lookAway();
    void goFarAway();
    bool objectVisible() const;
    int objectSize() const;

    int previousObjectSize;

  private:
    std::shared_ptr<render::Scene> m_scene;
    std::shared_ptr<render::Camera> m_camera;
    std::shared_ptr<engine::raytracer::Raytracer> m_raytracer;
    Buffer<unsigned int> m_buffer;
  };
}

#endif
