#include "engine/wavefront/detail/WavefrontTileTypes.h"

namespace engine::wavefront::detail {
  WavefrontDenoiserFeatureSet::WavefrontDenoiserFeatureSet(int width, int height)
      : albedo(width, height),
        normal(width, height),
        depth(width, height) {
    albedo.clear(Colord::black());
    normal.clear(Vector3d::null);
    depth.clear(0.0);
  }
}
