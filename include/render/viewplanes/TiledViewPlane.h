#pragma once

#include "render/viewplanes/ViewPlane.h"

namespace render {
  class TiledViewPlane : public ViewPlane {
  public:
    std::shared_ptr<ViewPlane> clone() const override {
      return std::make_shared<TiledViewPlane>(*this);
    }

    Iterator begin(const Recti& rect) const override;
  };
}
