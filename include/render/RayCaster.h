#pragma once

#include "core/Color.h"
#include "core/math/Ray.h"

namespace render {
  class Integrator;
  class State;

  /**
    * @brief Single-ray callback interface — what a `Material` or
    *        `Camera` needs to ask the active ray evaluator for radiance.
    *
    * Materials call `rayColor(ray, state)` to integrate reflected /
    * transmitted contributions; cameras call it once per pixel
    * sample to evaluate the primary ray. The interface deliberately
    * exposes nothing else — no scene ownership, no framebuffer, no
    * recursion-depth control, no thread-pool hooks — so callers do
    * not couple to a concrete engine type.
    *
    * `engine::raytracer::Raytracer` implements this interface for
    * compatibility with the existing camera and material APIs. The
    * radiance policy itself belongs behind `render::Integrator`:
    * `RayCaster` is the callback handle used to request another ray,
    * not the owner of the full integration algorithm.
    *
    * Future engines that don't have a sensible "trace one ray" notion
    * (wireframe, software raster, GL) just don't implement
    * `RayCaster`; their cameras and materials work through engine-
    * specific paths instead.
    *
    * @see Integrator — owns the single-ray radiance evaluation policy.
    */
  class RayCaster {
  public:
    virtual ~RayCaster() = default;

    /**
      * Trace `ray` and return the integrated colour at its first
      * hit (or the scene background on a miss). `state` carries the
      * recursion depth, hit-point cache, intersection counters, and
      * the optional event log; it is mutated in place.
      */
    virtual Colord rayColor(const Rayd& ray, State& state) const = 0;
  };
}
