#pragma once

#include "render/lights/Light.h"
#include "core/Color.h"

namespace render {
  class DirectionalLight : public Light {
  public:
    inline explicit DirectionalLight(const Vector3d& direction, const Colord& color)
        : Light(),
          m_direction(direction.normalized()),
          m_color(color) {
    }

    inline const Vector3d& direction() const {
      return m_direction;
    }

    inline const Colord& color() const {
      return m_color;
    }

    Vector3d direction(const Vector3d& point) const override;
    Colord radiance() const override;
    const char* fingerprintType() const override;
    void writeFingerprint(std::ostream& out, const std::string& prefix) const override;
    std::optional<Vector3d> directionalShadowMapDirection() const override;

  private:
    Vector3d m_direction;
    Colord m_color;
  };
}
