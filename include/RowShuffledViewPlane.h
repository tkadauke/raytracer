#ifndef ROW_SHUFFLED_VIEW_PLANE_H
#define ROW_SHUFFLED_VIEW_PLANE_H

#include "ViewPlane.h"

class RowShuffledViewPlane : public ViewPlane {
public:
  virtual Iterator begin() const;
};

#endif
