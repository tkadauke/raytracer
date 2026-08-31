#pragma once

#include "core/math/Vector.h"

namespace world_objects {
namespace detail {

inline Vector3d absoluteComponentsAtLeast(const Vector3d& vector, double minimum) {
  return vector.abs().cwiseMax(Vector3d(minimum, minimum, minimum));
}

}
}
