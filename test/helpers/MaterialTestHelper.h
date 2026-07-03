#ifndef MATERIAL_TEST_HELPER_H
#define MATERIAL_TEST_HELPER_H

#include "core/Color.h"
#include "render/materials/MatteMaterial.h"
#include "render/textures/ConstantColorTexture.h"

#include <memory>

namespace test {
  namespace helpers {
    inline std::shared_ptr<render::MatteMaterial> matte(const Colord& color) {
      return std::make_shared<render::MatteMaterial>(
        std::make_shared<render::ConstantColorTexture>(color));
    }
  }
}

#endif
