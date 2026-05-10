#pragma once

#include "render/viewplanes/ViewPlane.h"

namespace render {
  class PointInterlacedViewPlane : public ViewPlane {
  public:
    std::shared_ptr<ViewPlane> clone() const override {
      return std::make_shared<PointInterlacedViewPlane>(*this);
    }

    Iterator begin(const Recti& rect) const override;
  };
}
