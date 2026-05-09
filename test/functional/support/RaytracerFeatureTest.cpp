#include "test/functional/support/RaytracerFeatureTest.h"

#include "engine/raytracer/Raytracer.h"

namespace testing {
  std::shared_ptr<render::RenderEngine> RaytracerFeatureTest::createEngine() {
    return std::make_shared<engine::raytracer::Raytracer>(camera(), m_scene);
  }
}
