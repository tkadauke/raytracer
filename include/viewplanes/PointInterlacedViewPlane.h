#ifndef POINT_INTERLACED_VIEW_PLANE_H
#define POINT_INTERLACED_VIEW_PLANE_H

#include "viewplanes/ViewPlane.h"

class PointInterlacedViewPlane : public ViewPlane {
public:
  virtual Iterator begin() const;
};

#endif