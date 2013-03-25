#ifndef POINT_SHUFFLED_VIEW_PLANE_H
#define POINT_SHUFFLED_VIEW_PLANE_H

#include "raytracer/viewplanes/ViewPlane.h"

class PointShuffledViewPlane : public ViewPlane {
public:
  virtual Iterator begin(const Rect& rect) const;
};

#endif
