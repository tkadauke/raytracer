#pragma once

#include "core/math/Quaternion.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace world {

  inline Matrix4d bondTransform(const Vector3d& first, const Vector3d& second) {
    const auto center = (first + second) * 0.5;
    const auto delta = second - first;
    const auto length = delta.length();
    if (length <= std::numeric_limits<double>::epsilon())
      return Matrix4d::translate(center);

    const auto direction = delta / length;
    const auto up = Vector3d::up();
    const auto dot = std::max(-1.0, std::min(1.0, up * direction));

    Matrix4d rotation;
    if (dot > 1.0 - 1e-9) {
      rotation = Matrix4d();
    } else if (dot < -1.0 + 1e-9) {
      rotation = Quaterniond::fromAxisAngle(Vector3d(1, 0, 0), std::acos(-1.0)).toMatrix4();
    } else {
      const auto axis = (up ^ direction).normalized();
      rotation = Quaterniond::fromAxisAngle(axis, std::acos(dot)).toMatrix4();
    }

    return Matrix4d::translate(center) * rotation;
  }

}
