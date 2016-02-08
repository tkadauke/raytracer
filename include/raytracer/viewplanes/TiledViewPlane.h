#pragma once

#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  class TiledViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Rect& rect) const;
  };
}
