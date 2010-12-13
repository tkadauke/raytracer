#ifndef ROW_INTERLACED_VIEW_PLANE_H
#define ROW_INTERLACED_VIEW_PLANE_H

#include "ViewPlane.h"

class RowInterlacedViewPlane : public ViewPlane {
public:
  virtual Iterator begin() const;
};

#endif
