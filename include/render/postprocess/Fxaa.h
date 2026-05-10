#pragma once

#include "core/Color.h"

template<class T>
class Buffer;

namespace render::postprocess {
  /**
    * Applies a small FXAA-style postprocess pass in place.
    *
    * This is deliberately image-space anti-aliasing: it reads only the final
    * colour buffer, detects high-contrast luminance edges, and blends along
    * those edges. It does not need geometry, depth, or per-sample coverage, so
    * preview engines can use it after their normal render path.
    */
  void applyFxaa(Buffer<Colord>& buffer);
}
