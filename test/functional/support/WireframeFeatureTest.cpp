#include "test/functional/support/WireframeFeatureTest.h"

#include "engine/wireframe/Wireframe.h"

namespace testing {
  std::shared_ptr<render::RenderEngine> WireframeFeatureTest::createEngine() {
    return std::make_shared<engine::wireframe::Wireframe>(m_camera, m_scene);
  }

  Colord WireframeFeatureTest::primaryColor() const {
    return Colord::white();
  }
}
