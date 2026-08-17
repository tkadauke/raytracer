#pragma once

#include "core/math/Matrix.h"
#include "core/math/Vector.h"

// Composes a local transformation matrix from separate position, rotation
// (Euler angles, in radians), and scale vectors: translate * rotate * scale.
inline Matrix4d composePositionRotationScale(const Vector3d& position, const Vector3d& rotation,
                                             const Vector3d& scale) {
  return Matrix4d::translate(position) * Matrix3d::rotate(rotation) * Matrix3d::scale(scale);
}
