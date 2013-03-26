#ifndef RAYTRACER_POINT_INTERLACED_VIEW_PLANE_H
#define RAYTRACER_POINT_INTERLACED_VIEW_PLANE_H

#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  class PointInterlacedViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Rect& rect) const;
  };
}

#endif
