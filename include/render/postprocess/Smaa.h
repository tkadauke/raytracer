#pragma once

#include "core/Color.h"

template<class T>
class Buffer;

namespace render::postprocess {
  /**
    * Applies a small SMAA-style postprocess pass in place.
    *
    * This CPU approximation keeps the same data contract as FXAA: it reads only
    * the completed color buffer and writes a filtered result back in place. It
    * follows the first SMAA stages at preview scale by detecting luminance
    * discontinuities, estimating whether the edge is mainly horizontal or
    * vertical, and blending the two sides of that edge without rerunning
    * geometry coverage.
    */
  void applySmaa(Buffer<Colord>& buffer);
}
