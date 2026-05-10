#pragma once

#include "render/viewplanes/ViewPlane.h"

namespace render {
  class RowInterlacedViewPlane : public ViewPlane {
  public:
    std::shared_ptr<ViewPlane> clone() const override {
      return std::make_shared<RowInterlacedViewPlane>(*this);
    }

    Iterator begin(const Recti& rect) const override;
  };
}
