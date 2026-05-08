#pragma once

#include "render/textures/mappings/TextureMapping2D.h"

namespace render {
  class UVMapping2D : public TextureMapping2D {
  public:
    inline explicit UVMapping2D(double uScale = 1.0, double vScale = 1.0)
      : m_uScale(uScale),
        m_vScale(vScale)
    {
    }

    virtual void map(const HitPoint& hitPoint, double& s, double& t) const;

  private:
    double m_uScale;
    double m_vScale;
  };
}
