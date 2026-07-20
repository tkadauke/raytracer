#ifndef MATERIAL_TEST_HELPER_H
#define MATERIAL_TEST_HELPER_H

#include "core/Color.h"
#include "core/math/HitPoint.h"
#include "render/materials/MatteMaterial.h"
#include "render/textures/ConstantColorTexture.h"

#include <memory>

namespace test {
  namespace helpers {
    inline std::shared_ptr<render::MatteMaterial> matte(const Colord& color = Colord::white()) {
      return std::make_shared<render::MatteMaterial>(
        std::make_shared<render::ConstantColorTexture>(color));
    }

    inline HitPoint hitPointAtOrigin() {
      return HitPoint(nullptr, 1.0, Vector4d(0, 0, 0, 1), Vector3d(0, 1, 0));
    }
  }
}

#endif
