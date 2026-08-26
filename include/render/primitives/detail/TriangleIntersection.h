#pragma once

#include "core/math/Vector.h"
#include "core/math/Ray.h"

namespace render::detail {
  /**
    * Raw result of solving the Moller-Trumbore ray-triangle system for one
    * ray against the triangle (point0, point1, point2). Callers interpret
    * `denominator`/`beta`/`gamma`/`distance` against their own acceptance
    * rules (minimum hit distance, degenerate-denominator handling, ...)
    * since those differ slightly across the primitives that share this math.
    */
  struct TriangleBarycentricSolution {
    double denominator;
    double beta;
    double gamma;
    double distance;
  };

  inline TriangleBarycentricSolution solveTriangleBarycentric(const Vector3d& point0,
                                                              const Vector3d& point1,
                                                              const Vector3d& point2,
                                                              const Rayd& ray) {
    const double a = point0.x() - point1.x(), b = point0.x() - point2.x(),
                 c = ray.direction().x(), d = point0.x() - ray.origin().x();
    const double e = point0.y() - point1.y(), f = point0.y() - point2.y(),
                 g = ray.direction().y(), h = point0.y() - ray.origin().y();
    const double i = point0.z() - point1.z(), j = point0.z() - point2.z(),
                 k = ray.direction().z(), l = point0.z() - ray.origin().z();

    const double m = f * k - g * j, n = h * k - g * l, p = f * l - h * j;
    const double q = g * i - e * k, r = e * l - h * i, s = e * j - f * i;

    const double denominator = a * m + b * q + c * s;
    const double invDenom = 1.0 / denominator;

    const double beta = (d * m - b * n - c * p) * invDenom;
    const double gamma = (a * n + d * q + c * r) * invDenom;
    const double distance = (a * p - b * r + d * s) * invDenom;

    return {denominator, beta, gamma, distance};
  }
}
