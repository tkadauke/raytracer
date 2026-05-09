#pragma once

#include "test/functional/support/EngineFeatureTest.h"

namespace render {
  class RenderEngine;
}

namespace testing {
  /**
    * @brief `EngineFeatureTest` specialisation that constructs a
    *        `engine::wireframe::Wireframe` for `render()`.
    *
    * Wireframe renders edges in white over the configured background,
    * so visibility assertions key off white pixels rather than the
    * Raytracer's red. The shared step bodies that go through
    * `test->primaryColor()` for `ShapeRecognition` adapt
    * automatically — the silhouette circle of a sphere is detected
    * the same way whether the primary colour fills the disk
    * (Raytracer) or just outlines it (Wireframe).
    */
  class WireframeFeatureTest : public EngineFeatureTest {
  public:
    inline void setLod(int lod) {
      m_lod = lod;
    }

    std::shared_ptr<render::RenderEngine> createEngine() override;

    /// Wireframe edges are white by default.
    Colord primaryColor() const override;

  private:
    int m_lod{0};
  };
}
