#ifndef RAYTRACER_ROW_INTERLACED_VIEW_PLANE_H
#define RAYTRACER_ROW_INTERLACED_VIEW_PLANE_H

#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  class RowInterlacedViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Rect& rect) const;
  };
}

#endif
