#pragma once

#include "core/formats/molecule/Molecule.h"
#include "core/math/Quaternion.h"
#include "world/objects/Cylinder.h"

#include <QString>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

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

  inline std::unique_ptr<Cylinder>
  makeBondCylinder(const molecule::Atom& first, const molecule::Atom& second, double bondRadius) {
    auto cylinder = std::make_unique<Cylinder>();
    cylinder->setName(
      QStringLiteral("Bond %1-%2").arg(first.serialNumber).arg(second.serialNumber));
    cylinder->setRadius(bondRadius);
    cylinder->setHeight(first.position.distanceTo(second.position));
    cylinder->setMatrix(bondTransform(first.position, second.position));
    return cylinder;
  }

}
