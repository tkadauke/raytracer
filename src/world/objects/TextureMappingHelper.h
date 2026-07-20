#pragma once
#include "render/textures/mappings/PlanarMapping2D.h"
#include "render/textures/mappings/UVMapping2D.h"

#include <QString>

namespace world {
  inline render::TextureMapping2D* makeTextureMapping2D(const QString& mapping, double uScale,
                                                        double vScale) {
    if (mapping == "uv")
      return new render::UVMapping2D(uScale, vScale);
    return new render::PlanarMapping2D;
  }
}
