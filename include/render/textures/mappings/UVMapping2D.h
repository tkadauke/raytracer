#pragma once

#include "render/textures/mappings/TextureMapping2D.h"

namespace render {
  /**
    * Maps a HitPoint's stored UV coordinates onto 2D texture coordinates.
    *
    * `uScale` and `vScale` multiply the incoming UV values before texture
    * lookup. CheckerBoardTexture then uses those scaled coordinates for its
    * `floor(s) + floor(t)` parity lookup.
    *
    * @see CheckerBoardTexture
    */
  class UVMapping2D : public TextureMapping2D {
  public:
    inline explicit UVMapping2D(double uScale = 1.0, double vScale = 1.0)
        : m_uScale(uScale),
          m_vScale(vScale) {
    }

    /**
      * Returns the multiplier applied to incoming `u` coordinates.
      */
    inline double uScale() const {
      return m_uScale;
    }

    /**
      * Returns the multiplier applied to incoming `v` coordinates.
      */
    inline double vScale() const {
      return m_vScale;
    }

    virtual void map(const HitPoint& hitPoint, double& s, double& t) const;

  private:
    double m_uScale;
    double m_vScale;
  };
}
