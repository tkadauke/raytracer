#pragma once
#include <memory>

#include "core/math/Vector.h"
#include "core/math/Matrix.h"
#include "core/math/Ray.h"
#include "core/Color.h"
#include "core/MemoizedValue.h"
#include "render/viewplanes/ViewPlane.h"
#include "render/samplers/SampleStream.h"
#include "render/Object.h"

template<class T>
class Buffer;
template<class T>
class Rect;

namespace render {
  class Tonemap;
}

namespace raytracer { class Raytracer; }
namespace render {

  class Camera : public render::Object {
  public:
    Camera();
    explicit Camera(const Vector3d& position, const Vector3d& target);

    virtual ~Camera();

    inline void setPosition(const Vector3d& position) {
      m_matrix.reset();
      m_position = position;
    }

    inline void setTarget(const Vector3d& target) {
      m_matrix.reset();
      m_target = target;
    }

    inline std::shared_ptr<render::ViewPlane> viewPlane() const {
      return m_viewPlane;
    }

    virtual void setViewPlane(std::shared_ptr<render::ViewPlane> plane);

    inline void setShowProgressIndicators(bool show) {
      m_showProgressIndicators = show;
    }

    inline bool showProgressIndicators() const {
      return m_showProgressIndicators;
    }

    const Matrix4d& matrix() const;

    /**
      * Render into an HDR `Buffer<Colord>`. Each pixel accumulates
      * the ray-traced colour as a Colord (averaged across the
      * per-pixel sample count) — the raw radiance the camera saw,
      * with no LDR clamping.
      *
      * Tonemapping happens one level up, in
      * `Raytracer::render(Buffer<unsigned int>&)`, which allocates
      * a Colord buffer, calls this, and applies the configured
      * `Tonemap` to produce 8-bit output. Direct callers (EXR
      * writers, motion-blur compositors, future path-tracing
      * accumulators) can skip tonemapping and consume the HDR
      * buffer directly.
      */
    void render(std::shared_ptr<raytracer::Raytracer> raytracer, Buffer<Colord>& buffer) const;
    virtual void render(std::shared_ptr<raytracer::Raytracer> raytracer, Buffer<Colord>& buffer, const Rect<int>& rect) const;

    /**
      * Render into a packed-RGB display buffer with inline
      * tonemapping. This is the path used by interactive display
      * widgets (`RenderWidget` / `QtDisplay` / `GeneratedRayTracer`):
      * each tile worker tonemaps its pixels and writes packed
      * `unsigned int` values to the display buffer as it goes, so
      * the GUI's progress timer sees partial output during the
      * render rather than an empty buffer until completion.
      *
      * The `tonemap` argument is the operator the engine has
      * configured (defaults to `LinearTonemap` if the engine never
      * called `setTonemap`). It must outlive the call.
      *
      * The HDR `Buffer<Colord>` overload above is the path used by
      * non-display consumers (EXR writers, motion-blur compositors,
      * future path-tracing accumulators) — that one keeps the raw
      * radiance values around, no clamping.
      */
    virtual void render(std::shared_ptr<raytracer::Raytracer> raytracer, Buffer<unsigned int>& buffer,
                        std::shared_ptr<render::Tonemap> tonemap, const Rect<int>& rect) const;

    /**
      * Generate a primary ray for pixel `(x, y)`.
      *
      * The `stream` argument supplies stratified Monte-Carlo samples
      * for any extra stochastic dimensions the camera needs (lens
      * disc for thin-lens; shutter time for motion-blur cameras;
      * analyser angle for polarised cameras; ...). Cameras that don't
      * need extra dimensions ignore the parameter.
      *
      * The renderer threads a fresh stream per `(pixel, sample)` pair
      * — see `Camera::render`. Two consecutive `stream.next2D()` calls
      * within a single `rayForPixel` invocation return *independent*
      * stratified dimensions, so a future Kolb lens model that needs
      * both a lens-element sample and a wavelength sample can pull
      * both without correlation.
      */
    virtual Rayd rayForPixel(double x, double y, render::SampleStream& stream) const = 0;

    /**
      * Forward projection: world point → screen pixel `(x, y)`.
      *
      * The inverse of `rayForPixel`: given a 3D point in world
      * space, return the pixel through which a primary ray would
      * pass on its way to that point. Used by non-raytracing engines
      * (`WireframeEngine`, future `OpenGLEngine`) that need to map
      * mesh vertices onto the display.
      *
      * Returns `Vector2d::undefined()` if:
      *
      *  - the point is behind the eye (camera-space z ≤ -distance
      *    on a pinhole / thin-lens camera), or
      *  - the camera doesn't have a meaningful inverse — e.g. a
      *    fish-eye projection collapses an entire ray bundle to one
      *    pixel and isn't trivially invertible. The base
      *    implementation returns undefined; only cameras with a
      *    closed-form inverse override.
      *
      * Result coordinates are in the same window-relative pixel
      * frame as `rayForPixel`'s `(x, y)` arguments — non-integer is
      * fine (callers like the wireframe rasterizer want sub-pixel
      * precision for line endpoints).
      */
    virtual Vector2d projectPoint(const Vector3d& worldPoint) const;

    /**
      * Convenience overload that uses a `NullSampleStream` returning
      * the centre of every dimension. Useful for tests and ad-hoc
      * callers (e.g. `SceneBrowser`'s pixel-pick) that don't have a
      * sampler at hand. Don't use in production rendering — the
      * camera loses access to stratification.
      */
    inline Rayd rayForPixel(double x, double y) const {
      render::NullSampleStream stream;
      return rayForPixel(x, y, stream);
    }

    inline Rayd rayForPixel(const Vector2d& pixel) const {
      return rayForPixel(pixel.x(), pixel.y());
    }

    inline void cancel() {
      m_cancelled = true;
    }

    inline bool isCancelled() const {
      return m_cancelled;
    }

    inline void uncancel() {
      m_cancelled = false;
    }

  protected:
    /// Write `color` (already divided by sample count) into every
    /// pixel of the iterator's footprint — single pixel for the
    /// regular iterator, the size×size block for interlaced
    /// iterators that haven't refined yet.
    void plot(Buffer<Colord>& buffer, const Recti& rect, const render::ViewPlane::Iterator& pixel, const Colord& color) const;

    /// LDR variant — writes a packed-RGB pixel value (the result of
    /// `tonemap->apply(color).rgb()` from the LDR camera path). The
    /// footprint logic matches `plot(Buffer<Colord>&, ...)` so
    /// interlaced iterators show the same coarse-then-refine
    /// progression in either output buffer.
    void plotRGB(Buffer<unsigned int>& buffer, const Recti& rect, const render::ViewPlane::Iterator& pixel, unsigned int rgb) const;

  private:
    bool m_cancelled;
    bool m_showProgressIndicators;
    Vector3d m_position, m_target;
    mutable MemoizedValue<Matrix4d> m_matrix;
    std::shared_ptr<render::ViewPlane> m_viewPlane;
  };
}
