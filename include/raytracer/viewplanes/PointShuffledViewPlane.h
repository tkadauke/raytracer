#pragma once

#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  class PointShuffledViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Recti& rect) const;
  };
}
