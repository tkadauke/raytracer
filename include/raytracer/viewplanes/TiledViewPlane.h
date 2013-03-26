#ifndef RAYTRACER_TILED_VIEW_PLANE_H
#define RAYTRACER_TILED_VIEW_PLANE_H

#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  class TiledViewPlane : public ViewPlane {
  public:
    virtual Iterator begin(const Rect& rect) const;
  };
}

#endif
