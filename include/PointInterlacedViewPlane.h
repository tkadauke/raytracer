#ifndef POINT_INTERLACED_VIEW_PLANE_H
#define POINT_INTERLACED_VIEW_PLANE_H

#include "ViewPlane.h"

class PointInterlacedViewPlane : public ViewPlane {
public:
  virtual Iterator begin() const;
};

#endif
