#pragma once

#include "render/viewplanes/ViewPlane.h"

namespace render {
  class PointInterlacedViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Recti& rect) const;
  };
}
