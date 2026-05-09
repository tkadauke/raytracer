#pragma once

#include "test/functional/support/FeatureTest.h"

#include "core/math/Vector.h"
#include "core/Color.h"
#include "core/Buffer.h"

namespace render {
  class Camera;
  class Material;
  class Primitive;
  class RenderEngine;
  class Scene;
}

namespace testing {
  /**
    * @brief Engine-agnostic functional-test fixture.
    *
    * Owns the scene / camera / buffer / step registry. Subclasses
    * (`RaytracerFeatureTest`, `WireframeFeatureTest`, future
    * `PathTracerFeatureTest`) plug in the engine type via
    * `createEngine()` and adapt the visibility assertions
    * (`objectVisible` / `objectSize`) to the engine's output style:
    * Raytracer renders solid red pixels for the test material,
    * Wireframe renders white silhouette edges, etc.
    *
    * `GIVEN/WHEN/THEN` steps are registered against
    * `EngineFeatureTest` directly, so the same step text works for
    * any engine subclass — `FeatureTest<EngineFeatureTest>::Steps`
    * is one shared registry. Engine-specific behaviour goes through
    * the virtual hooks here.
    */
  class EngineFeatureTest : public FeatureTest<EngineFeatureTest> {
  protected:
    virtual void SetUp() override;
    virtual void TearDown() override;
    virtual void beforeThen() override;

  public:
    EngineFeatureTest();
    virtual ~EngineFeatureTest() = default;

    /// Subclass hook — build the engine for this test's scene +
    /// camera. Called from `render()`.
    virtual std::shared_ptr<render::RenderEngine> createEngine() = 0;

    /// `objectVisible` / `objectSize` adapt to the engine's output
    /// style. Default counts pixels matching `primaryColor()` —
    /// Raytracer subclass leaves it as red; Wireframe overrides to
    /// white (edge colour).
    virtual Colord primaryColor() const;
    virtual bool objectVisible() const;
    virtual int objectSize() const;

    void add(std::shared_ptr<render::Primitive> primitive);
    render::Scene* scene() const;
    std::shared_ptr<render::Camera> camera();
    void setCamera(std::shared_ptr<render::Camera> camera);
    void setCamera(const Vector3d& position, const Vector3d& lookAt);
    void setView(const Vector3d& position, const Vector3d& lookAt);
    void render();
    void cancel();
    void uncancel();

    const Buffer<unsigned int>& buffer() const;
    void clear();

    bool colorPresent(const Colord& color) const;
    int colorCount(const Colord& color) const;
    unsigned int colorAt(int x, int y) const;
    void show() const;

    /// Default red Lambertian material. Used by GIVEN steps that
    /// add coloured primitives. Wireframe ignores the colour, so a
    /// shared default works for both engines.
    std::shared_ptr<render::Material> redDiffuse() const;

    void lookAtOrigin();
    void lookAway();
    void goFarAway();

    int previousObjectSize;
    int previousEdgeCount;

  protected:
    std::shared_ptr<render::Scene> m_scene;
    std::shared_ptr<render::Camera> m_camera;
    std::shared_ptr<render::RenderEngine> m_engine;
    Buffer<unsigned int> m_buffer;
    bool m_cancelled;
  };
}
