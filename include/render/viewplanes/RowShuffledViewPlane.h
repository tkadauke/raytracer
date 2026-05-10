#pragma once

#include "render/viewplanes/ViewPlane.h"

namespace render {
  class RowShuffledViewPlane : public ViewPlane {
  public:
    std::shared_ptr<ViewPlane> clone() const override {
      return std::make_shared<RowShuffledViewPlane>(*this);
    }

    virtual Iterator begin(const Recti& rect) const;
  };
}
