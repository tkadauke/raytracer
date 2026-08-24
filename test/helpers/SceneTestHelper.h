#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "render/lights/DirectionalLight.h"
#include "render/primitives/Box.h"
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

  /**
    * A single unit sphere at the origin, lit by a directional light facing
    * -Z. Used by raster/GPU comparison tests that only need a simple,
    * predictable scene with a custom background and sphere color.
    */
  inline std::shared_ptr<render::Scene> litSphereScene(const Colord& background,
                                                        const Colord& sphereColor) {
    auto scene = std::make_shared<render::Scene>(background);
    auto sphere = std::make_shared<render::Sphere>(Vector3d::null, 1.0);
    sphere->setMaterial(matte(sphereColor));
    scene->add(sphere);
    scene->addLight(
      std::make_shared<render::DirectionalLight>(Vector3d(0, 0, -1), Colord::white()));
    return scene;
  }

  /**
    * A unit cube at the origin against a solid background. Used by
    * wireframe/rasterizer tests that only need a simple, predictable scene.
    */
  inline std::shared_ptr<render::Scene> sceneWithBox(const Colord& background) {
    auto scene = std::make_shared<render::Scene>(background);
    scene->add(std::make_shared<render::Box>(Vector3d::null, Vector3d(1, 1, 1)));
    return scene;
  }
}
