#pragma once

#include "core/math/Constants.h"
#include "core/math/Vector.h"

#include <algorithm>
#include <cmath>

namespace render {

  /// Build an orthogonal tangent vector from a surface normal.
  inline Vector3d tangentFor(const Vector3d& n) {
    const Vector3d helper = std::abs(n.y()) < 0.999 ? Vector3d::up() : Vector3d::right();
    return (helper ^ n).normalized();
  }

  /// Clamp a scalar BSDF sampling coordinate to the unit interval [0, 1].
  inline double clampUnit(double value) {
    return std::clamp(value, 0.0, 1.0);
  }

  /// Clamp both components of a 2D BSDF sampling coordinate to [0, 1].
  inline Vector2d clampUnitSquare(const Vector2d& sample) {
    return Vector2d(clampUnit(sample.x()), clampUnit(sample.y()));
  }

  /// Sample a direction on the cosine-weighted hemisphere around normal `n`.
  inline Vector3d cosineHemisphereDirection(const Vector3d& n, const Vector2d& sample) {
    const Vector2d clamped = clampUnitSquare(sample);
    const double u0 = clamped.x();
    const double u1 = clamped.y();
    const double r = std::sqrt(u0);
    const double phi = TAU * u1;
    const double x = r * std::cos(phi);
    const double y = r * std::sin(phi);
    const double z = std::sqrt(std::max(0.0, 1.0 - u0));

    const Vector3d tangent = tangentFor(n);
    const Vector3d bitangent = n ^ tangent;
    return (tangent * x + bitangent * y + n * z).normalized();
  }

  /// Sample a direction from a Phong lobe around the given reflection `axis`.
  inline Vector3d phongLobeDirection(const Vector3d& axis, const Vector2d& sample,
                                     double exponent) {
    const Vector2d clamped = clampUnitSquare(sample);
    const double u0 = clamped.x();
    const double u1 = clamped.y();
    const double cosTheta = std::pow(u0, 1.0 / (exponent + 1.0));
    const double sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
    const double phi = TAU * u1;

    const Vector3d tangent = tangentFor(axis);
    const Vector3d bitangent = axis ^ tangent;
    return (tangent * (sinTheta * std::cos(phi)) + bitangent * (sinTheta * std::sin(phi)) +
            axis * cosTheta)
      .normalized();
  }
}
