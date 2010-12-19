#ifndef ROW_INTERLACED_VIEW_PLANE_H
#define ROW_INTERLACED_VIEW_PLANE_H

#include "viewplanes/ViewPlane.h"

class RowInterlacedViewPlane : public ViewPlane {
public:
  virtual Iterator begin(const Rect& rect) const;
};

#endif
