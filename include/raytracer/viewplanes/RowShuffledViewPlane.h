#ifndef RAYTRACER_ROW_SHUFFLED_VIEW_PLANE_H
#define RAYTRACER_ROW_SHUFFLED_VIEW_PLANE_H

#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  class RowShuffledViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Rect& rect) const;
  };
}

#endif
