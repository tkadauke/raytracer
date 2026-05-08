#pragma once

#include "render/textures/mappings/TextureMapping2D.h"

namespace render {
  /**
    * Maps a HitPoint's world-space position onto 2D texture coordinates.
    *
    * The planar mapper reads the hit point's `x` coordinate as `s` and
    * forward-axis coordinate as `t`. CheckerBoardTexture then uses those
    * mapped coordinates for its `floor(s) + floor(t)` parity lookup.
    *
    * @see CheckerBoardTexture
    */
  class PlanarMapping2D : public TextureMapping2D {
  public:
    virtual void map(const HitPoint& hitPoint, double& s, double& t) const;
  };
}
