#pragma once

#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  class PointInterlacedViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Rect& rect) const;
  };
}
