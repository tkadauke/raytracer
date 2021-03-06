#pragma once

#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  class RowShuffledViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Recti& rect) const;
  };
}
