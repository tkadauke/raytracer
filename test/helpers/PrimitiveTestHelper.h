#pragma once

#include "core/math/Vector.h"
#include "render/primitives/Box.h"

namespace test::helpers {
  inline render::Box* unitBox() {
    static render::Box box(Vector3d::null, Vector3d::one);
    return &box;
  }
}
