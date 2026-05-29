#pragma once

#include "core/math/Vector.h"
#include "core/Color.h"

#include "render/Object.h"

#include <iosfwd>
#include <optional>
#include <string>

namespace render {
  /**
    * @brief Abstract base for scene light sources.
    *
    * Materials iterate `Scene::lights()` and call two methods on
    * each light during direct-lighting evaluation:
    *
    *  - `direction(point)` — the unit vector from `point` toward
    *    the light. For a `PointLight` this is `(position - point).
    *    normalized()`; for a `DirectionalLight` (sun, etc.) it's a
    *    constant independent of `point`.
    *  - `radiance()` — the colour × intensity arriving along that
    *    direction. Already pre-multiplied so materials can simply
    *    multiply their BRDF result by `radiance()`.
    *
    * Lights are *not* part of the geometric `Composite` tree —
    * they live on a separate `Scene::lights()` list because the
    * shading pipeline iterates them independently of any geometry
    * traversal. Editable scene-graph wrappers (`world::Light` and
    * subclasses) attach to the world-side tree but their
    * `toRaytracer()` factory produces a runtime `Light` that goes
    * onto this flat list.
    *
    * Concrete subclasses: `PointLight`, `DirectionalLight`. Future
    * work for area lights would also subclass here — the interface
    * is already shaped right (the area-light case picks a sample
    * point per shading call and returns its `direction` from there).
    *
    * @see PointLight, DirectionalLight — concrete subclasses.
    * @see Scene::lights() — where materials read these from.
    */
  class Light : public render::Object {
  public:
    inline Light() {
    }

    inline virtual ~Light() {
    }

    /**
      * @returns the unit vector from `point` toward the light. For
      * directional lights this is constant; for positional
      * (point / area) lights it depends on `point`.
      *
      * Materials use this as the `in` argument to BRDF evaluation
      * (`brdf.calculate(hitPoint, out, direction)`).
      */
    virtual Vector3d direction(const Vector3d& point) const = 0;

    /**
      * @returns the radiance arriving along `direction(point)` —
      * already includes the light's colour and intensity. Materials
      * multiply their BRDF result by this to obtain the direct-
      * lighting contribution from this source.
      *
      * The `world::Light` editor side bakes `colour * intensity`
      * into this single value at `toRaytracer()` time, so the
      * runtime light only carries one Colord.
      */
    virtual Colord radiance() const = 0;

    /**
      * Stable type name used by deterministic fingerprints. Unlike RTTI names,
      * this is controlled by the concrete light class and remains stable across
      * compilers.
      */
    virtual const char* fingerprintType() const = 0;

    /**
      * Writes the light-specific state that can influence a render. The render
      * graph cache uses this through the Light hierarchy instead of switching on
      * concrete light types.
      */
    virtual void writeFingerprint(std::ostream& out, const std::string& prefix) const;

    /**
      * @returns the light direction used by a cascaded directional shadow-map
      * builder when this light can be represented that way, or `std::nullopt`
      * for light types that need another shadow-map shape.
      */
    virtual std::optional<Vector3d> directionalShadowMapDirection() const;

    /**
      * @returns the finite light position when this light can be represented as
      * a positional shader light, or `std::nullopt` for light types that are
      * direction-only or need a different sampling model.
      */
    virtual std::optional<Vector3d> positionalLightPosition() const;

  protected:
    void writeCommonFingerprint(std::ostream& out, const std::string& prefix) const;
    static void writeFingerprintColor(std::ostream& out, const std::string& name,
                                      const Colord& color);
    static void writeFingerprintVector(std::ostream& out, const std::string& name,
                                       const Vector3d& vector);
  };
}
