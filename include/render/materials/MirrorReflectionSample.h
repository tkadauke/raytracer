#pragma once

#include "render/brdf/PerfectSpecular.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

namespace render {
  /// Result of importance-sampling a perfect-mirror BRDF at a hit point:
  /// the sampled incoming direction, the BRDF value at that draw, and the
  /// ray traced along it from the hit point. Shared by `ReflectiveMaterial`
  /// and `TransparentMaterial`, whose `shade`/`shadeWhitted` both spawn a
  /// mirror-reflection continuation this way.
  struct MirrorReflectionSample {
    Vector3d in;
    Colord value;
    Rayd ray;
  };

  inline MirrorReflectionSample sampleMirrorReflection(const PerfectSpecular& brdf,
                                                        const HitPoint& hitPoint,
                                                        const Vector3d& out) {
    Vector3d in;
    Colord value = brdf.sample(hitPoint, out, in);
    return MirrorReflectionSample{in, value, Rayd(hitPoint.point(), in)};
  }
}
