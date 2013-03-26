#ifndef RAYTRACER_POINT_SHUFFLED_VIEW_PLANE_H
#define RAYTRACER_POINT_SHUFFLED_VIEW_PLANE_H

#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  class PointShuffledViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Rect& rect) const;
  };
}

#endif
