#ifndef TILED_VIEW_PLANE_H
#define TILED_VIEW_PLANE_H

#include "raytracer/viewplanes/ViewPlane.h"

class TiledViewPlane : public ViewPlane {
public:
  virtual Iterator begin(const Rect& rect) const;
};

#endif
