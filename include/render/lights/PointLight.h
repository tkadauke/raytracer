#pragma once

#include "render/lights/Light.h"
#include "core/Color.h"

namespace render {
  class PointLight : public Light {
  public:
    inline explicit PointLight(const Vector3d& position, const Colord& color)
        : Light(),
          m_position(position),
          m_color(color) {
    }

    inline const Vector3d& position() const {
      return m_position;
    }

    inline const Colord& color() const {
      return m_color;
    }

    Vector3d direction(const Vector3d& point) const override;
    Colord radiance() const override;
    LightSample sample(const Vector3d& point) const override;
    std::optional<Colord> power() const override;
    const char* fingerprintType() const override;
    void writeFingerprint(std::ostream& out, const std::string& prefix) const override;
    std::optional<Vector3d> positionalLightPosition() const override;

  private:
    Vector3d m_position;
    Colord m_color;
  };
}
