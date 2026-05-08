#pragma once

#include "render/brdf/BRDF.h"
#include "core/math/Range.h"

namespace render {
  /**
    * @brief Ideal mirror BRDF used by recursive reflective materials.
    *
    * `sample()` has no random choice to make: it mirrors the outgoing
    * view direction around the hit normal and writes that direction
    * into `in`. Equivalently, for an incoming ray direction `d` and a
    * unit normal `n`, the traced mirror direction is
    * `d - 2(d dot n)n`.
    *
    * `ReflectiveMaterial` uses that sampled direction to call
    * `raycaster->rayColor(...)` recursively. The widget below shows
    * the geometric mirror calculation and the recursive call tree
    * that lets a reflection contain more scene information than the
    * first surface hit.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="reflective_material_recursion.js"></script>
    * @endhtmlonly
    *
    * @see ReflectiveMaterial
    */
  class PerfectSpecular : public BRDF {
  public:
    inline PerfectSpecular()
        : m_reflectionCoefficient(1) {
    }

    Colord sample(const HitPoint& hitPoint, const Vector3d& out, Vector3d& in) const override;

    /// BSDF::sample for the delta lobe — sets `pdf = 1` to flag the
    /// delta to MIS-aware integrators, then delegates to the
    /// geometric `sample(hp, out, in)` above.
    Colord sample(const HitPoint& hitPoint, const Vector3d& wi, Vector3d& wo,
                  double& pdf) const override {
      pdf = 1.0;
      return sample(hitPoint, wi, wo);
    }

    int flags() const override {
      return BSDF::Specular | BSDF::Reflection;
    }

    inline const Colord& reflectionColor() const {
      return m_reflectionColor;
    }

    inline void setReflectionColor(const Colord& color) {
      m_reflectionColor = color;
    }

    inline double reflectionCoefficient() const {
      return m_reflectionCoefficient;
    }

    inline void setReflectionCoefficient(double coeff) {
      m_reflectionCoefficient = Ranged(0, 1).clamp(coeff);
    }

  private:
    Colord m_reflectionColor;
    double m_reflectionCoefficient;
  };
}
