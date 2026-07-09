#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "test/helpers/MaterialTestHelper.h"

#include <memory>

namespace test::helpers {
  inline std::shared_ptr<render::Scene> highContrastScene() {
    auto scene = std::make_shared<render::Scene>();
    scene->setBackground(Colord::black());
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 1.25);
    sphere->setMaterial(matte(Colord::white()));
    scene->add(sphere);
    return scene;
  }
}
