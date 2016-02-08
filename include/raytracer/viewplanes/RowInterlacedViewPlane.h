#pragma once

#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  class RowInterlacedViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Rect& rect) const;
  };
}
