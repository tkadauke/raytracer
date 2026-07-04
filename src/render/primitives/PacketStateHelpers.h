#pragma once
#include "core/SimdFeatures.h"
#include "render/State.h"
#include "render/primitives/Primitive.h"

#include <string>

#if RAYTRACER_SIMD_SSE || RAYTRACER_SIMD_NEON
namespace render {
  inline void packetHit(State& state, const Primitive* primitive, const std::string& reason) {
    if (state.traceEvents) {
      state.hit(primitive, reason);
    } else {
      ++state.intersectionHits;
    }
  }

  inline void packetMiss(State& state, const Primitive* primitive, const std::string& reason) {
    if (state.traceEvents) {
      state.miss(primitive, reason);
    } else {
      ++state.intersectionMisses;
    }
  }
}
#endif
