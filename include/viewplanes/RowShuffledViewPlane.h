#ifndef ROW_SHUFFLED_VIEW_PLANE_H
#define ROW_SHUFFLED_VIEW_PLANE_H

#include "viewplanes/ViewPlane.h"

class RowShuffledViewPlane : public ViewPlane {
public:
  virtual Iterator begin(const Rect& rect) const;
};

#endif
