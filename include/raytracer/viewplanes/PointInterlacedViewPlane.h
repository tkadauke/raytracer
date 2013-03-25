#ifndef POINT_INTERLACED_VIEW_PLANE_H
#define POINT_INTERLACED_VIEW_PLANE_H

#include "raytracer/viewplanes/ViewPlane.h"

class PointInterlacedViewPlane : public ViewPlane {
public:
  virtual Iterator begin(const Rect& rect) const;
};

#endif
