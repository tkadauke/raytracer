#pragma once

#include "render/viewplanes/ViewPlane.h"

namespace render {
  class PointShuffledViewPlane : public ViewPlane {
  public:
    std::shared_ptr<ViewPlane> clone() const override {
      return std::make_shared<PointShuffledViewPlane>(*this);
    }

    Iterator begin(const Recti& rect) const override;
  };
}
