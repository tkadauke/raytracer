#include "test/functional/support/WireframeFeatureTest.h"

#include "engine/wireframe/Wireframe.h"

namespace testing {
  std::shared_ptr<render::RenderEngine> WireframeFeatureTest::createEngine() {
    auto engine = std::make_shared<engine::wireframe::Wireframe>(camera(), m_scene);
    engine->setLod(m_lod);
    return engine;
  }

  Colord WireframeFeatureTest::primaryColor() const {
    return Colord::white();
  }
}
