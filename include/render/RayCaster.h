#pragma once

#include "core/Color.h"
#include "core/math/Ray.h"

namespace render {
  class State;

  /**
    * @brief Single-ray callback interface — what a `Material` or
    *        `Camera` needs to recursively trace through the scene.
    *
    * Materials call `rayColor(ray, state)` to integrate reflected /
    * transmitted contributions; cameras call it once per pixel
    * sample to evaluate the primary ray. The interface deliberately
    * exposes nothing else — no scene access, no recursion-depth
    * control, no thread-pool hooks — so the same callers work
    * against any future ray-based engine (Whitted raytracer today,
    * path tracer later) without coupling to a concrete engine type.
    *
    * `raytracer::Raytracer` implements this interface; the previous
    * design passed a raw `Raytracer*` directly through the call
    * chain, leaking the concrete engine type into `render::Material`
    * and `render::Camera` — which then forced those (engine-agnostic)
    * subsystems to live in `raytracer::` or hold a cross-namespace
    * dependency on it. The interface is the architectural seam that
    * lets `render::` be the actually-engine-agnostic namespace its
    * name promises.
    *
    * Future engines that don't have a sensible "trace one ray" notion
    * (wireframe, software raster, GL) just don't implement
    * `RayCaster`; their cameras and materials work through engine-
    * specific paths instead.
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
