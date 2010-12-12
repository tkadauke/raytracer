#ifndef POINT_SHUFFLED_VIEW_PLANE_H
#define POINT_SHUFFLED_VIEW_PLANE_H

#include "ViewPlane.h"

class PointShuffledViewPlane : public ViewPlane {
public:
  virtual Iterator begin() const;
};

#endif
