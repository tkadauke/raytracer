#pragma once

#include "core/Color.h"
#include "render/lights/Light.h"

namespace render {
  /**
    * Constant-radiance one-sided rectangular area light.
    *
    * The rectangle is defined by its center plus two full edge vectors. Samples
    * are mapped as `center + (u - 0.5) * edgeU + (v - 0.5) * edgeV`. The
    * emitting side is the side pointed to by `edgeU x edgeV`.
    */
  class RectangularAreaLight : public Light {
  public:
    RectangularAreaLight(const Vector3d& center, const Vector3d& edgeU, const Vector3d& edgeV,
                         const Colord& radiance);

    const Vector3d& center() const;
    const Vector3d& edgeU() const;
    const Vector3d& edgeV() const;
    const Vector3d& normal() const;
    const Colord& color() const;

    double area() const;

    Vector3d direction(const Vector3d& point) const override;
    Colord radiance() const override;
    LightSample sample(const Vector3d& point) const override;
    LightSample sample(const Vector3d& point, const Vector2d& sample) const override;
    double pdf(const Vector3d& point, const Vector3d& direction) const override;
    bool isDelta() const override;
    Colord emission() const override;
    std::optional<Colord> power() const override;
    std::shared_ptr<render::Primitive> emitterPrimitive() const override;
    const char* fingerprintType() const override;
    void writeFingerprint(std::ostream& out, const std::string& prefix) const override;

  private:
    static constexpr double tolerance = 1e-9;

    Vector3d samplePoint(const Vector2d& sample) const;
    bool containsPoint(const Vector3d& point) const;
    double surfaceCosine(const Vector3d& directionToLight) const;

    Vector3d m_center;
    Vector3d m_edgeU;
    Vector3d m_edgeV;
    Vector3d m_normal;
    Colord m_radiance;
    double m_area;
  };
}
