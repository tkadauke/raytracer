#pragma once

#include "core/math/Vector.h"
#include "core/Color.h"

#include "render/Object.h"

#include <iosfwd>
#include <optional>
#include <string>

namespace render {
  /**
    * @brief Result of sampling a light from one shading point.
    *
    * `direction` and `radiance` intentionally mirror the legacy direct-lighting
    * API so existing materials can keep using `Light::direction()` and
    * `Light::radiance()` until an integrator opts into sampling. `pdf` is the
    * probability density of this sample in the light's native sampling measure.
    * Delta lights (point and directional lights) report `pdf == 1` for their
    * deterministic sample and `Light::pdf()` returns zero for ordinary
    * solid-angle queries.
    */
  struct LightSample {
    Vector3d direction;
    Colord radiance;
    double distance;
    double pdf;
    bool delta;
  };

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
    * work for area lights would also subclass here. New integrators
    * can call `sample(point)` to get the same legacy contribution
    * plus sampling metadata for soft shadows, direct-light sampling,
    * and MIS. Delta lights document that explicitly through the
    * returned sample and through `isDelta()`.
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
      * Samples this light from `point`.
      *
      * The default implementation preserves legacy direct-lighting behavior by
      * returning `direction(point)`, `radiance()`, infinite distance, `pdf == 1`,
      * and `delta == true`. Finite positional lights override this to report the
      * actual distance. Future area lights should override this with stochastic
      * samples and a non-delta PDF.
      */
    virtual LightSample sample(const Vector3d& point) const;

    /**
      * Evaluates the probability density for sampling `direction` from `point`
      * in solid angle. Delta lights return zero here because their contribution
      * is a discrete distribution rather than an ordinary density; use
      * `sample(point).pdf` when evaluating the deterministic delta sample.
      */
    virtual double pdf(const Vector3d& point, const Vector3d& direction) const;

    /**
      * @returns whether this light is represented by a delta distribution.
      * Point and directional lights are delta lights; area lights should return
      * false.
      */
    virtual bool isDelta() const;

    /**
      * @returns an emission/intensity value suitable for light-selection
      * heuristics. This is metadata only; `radiance()` remains the value used by
      * the legacy direct-lighting path.
      */
    virtual Colord emission() const;

    /**
      * @returns a finite power estimate when the light has a bounded emitter.
      * Infinite emitters such as directional lights return `std::nullopt`.
      */
    virtual std::optional<Colord> power() const;

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
