#ifndef TILED_VIEW_PLANE_H
#define TILED_VIEW_PLANE_H

#include "ViewPlane.h"

class TiledViewPlane : public ViewPlane {
public:
  virtual Iterator begin() const;
};

#endif
