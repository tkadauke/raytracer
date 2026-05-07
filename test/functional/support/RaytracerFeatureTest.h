#pragma once

#include "test/functional/support/EngineFeatureTest.h"

namespace render {
  class RenderEngine;
}

namespace testing {
  /**
    * @brief `EngineFeatureTest` specialisation that constructs a
    *        Whitted raytracer for `render()`.
    *
    * Existing 91 functional tests inherit from this; default
    * behaviour matches the historical fixture exactly. The step
    * registry now lives on the shared `EngineFeatureTest` base, so
    * the same steps are reachable from `WireframeFeatureTest` (and
    * any future engine fixture) without duplication.
    */
  class RaytracerFeatureTest : public EngineFeatureTest {
  public:
    std::shared_ptr<render::RenderEngine> createEngine() override;
  };
}
