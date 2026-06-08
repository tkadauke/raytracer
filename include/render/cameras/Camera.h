#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "core/math/Vector.h"
#include "core/math/Matrix.h"

#include <optional>
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
  class RayCaster;

  class Camera : public Object {
  public:
    Camera();
    explicit Camera(const Vector3d& position, const Vector3d& target);

    virtual ~Camera();

    struct PrimaryRaySample {
      Rayd ray;
      double timeSample{0.0};
      std::shared_ptr<render::SampleStream> sampleStream;
    };

    struct PrimaryRay {
      Rayd ray;
      double timeSample{0.0};
    };

    class PrimaryRayGenerator {
    public:
      virtual ~PrimaryRayGenerator();

      virtual std::optional<PrimaryRay> sample(const render::ViewPlane::Iterator& pixel,
                                               render::SampleStream& stream) const = 0;
    };

    /// Clone camera state for an isolated render job. The clone gets
    /// its own view-plane instance so render-thread setup does not race
    /// with the interactive camera being moved by the UI.
    virtual std::shared_ptr<Camera> clone() const = 0;

    /// Stable type name for deterministic fingerprints and diagnostics.
    virtual const char* fingerprintType() const = 0;

    inline void setPosition(const Vector3d& position) {
      m_matrix.reset();
      m_inverseMatrix.reset();
      m_position = position;
    }

    inline void setTarget(const Vector3d& target) {
      m_matrix.reset();
      m_inverseMatrix.reset();
      m_target = target;
    }

    /**
      * @returns the current camera position in world space.
      */
    [[nodiscard]] inline const Vector3d& position() const {
      return m_position;
    }

    /**
      * @returns the world-space point the camera is looking at.
      */
    [[nodiscard]] inline const Vector3d& target() const {
      return m_target;
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

    /**
      * Return the part of `rect` that this camera should actively sample.
      *
      * In `FitExact` aspect mode, the outer letterbox/pillarbox area remains
      * untouched; all other modes return `rect` unchanged.
      */
    Recti renderableRect(const Recti& rect) const;

    /**
      * Number of samples emitted for each iterated pixel footprint.
      */
    int samplesPerPixel() const;

    /**
      * Generate the `sampleIndex`-th primary ray sample for an iterator pixel.
      *
      * The camera consumes the renderer-owned pixel and time dimensions before
      * delegating to `rayForPixel(...)`, then returns the remaining stream so
      * the integrator can consume BSDF/light/continuation dimensions.
      */
    std::optional<PrimaryRaySample> primaryRaySample(const render::ViewPlane::Iterator& pixel,
                                                     int sampleIndex,
                                                     std::optional<std::uint64_t> tileSeed) const;

    /**
      * Generate a primary ray sample from a caller-owned stream.
      *
      * Batch renderers can keep stream storage outside the returned value and
      * pass non-owning pointers to integrators, avoiding per-sample heap
      * allocation while preserving the same pixel/time/camera dimension
      * consumption as the retained-stream overload above.
      */
    std::optional<PrimaryRay> primaryRaySample(const render::ViewPlane::Iterator& pixel,
                                               render::SampleStream& stream) const;

    /**
      * Create a per-render primary-ray generator.
      *
      * Wavefront batch renderers call this once per tile so concrete cameras can
      * precompute camera-local constants while preserving the same sample-stream
      * ownership as `primaryRaySample(pixel, stream)`.
      */
    virtual std::unique_ptr<PrimaryRayGenerator> primaryRayGenerator() const;

    std::uint64_t primaryRayPixelHash(const render::ViewPlane::Iterator& pixel,
                                      std::optional<std::uint64_t> tileSeed) const;

    /**
      * Set the aspect-ratio fit mode for this camera's view plane.
      *
      * The mode is propagated to the view plane immediately and
      * persists across `setViewPlane()` calls (the new plane inherits
      * the camera's stored mode). Defaults to `Stretch`, which
      * preserves the pre-AspectMode behavior exactly.
      *
      * Non-rectilinear cameras (FishEye, Spherical, Equirectangular)
      * accept any mode, but `FitWidth` and `FitHeight` have the same
      * pixel-squaring effect as for pinhole cameras — the concept of a
      * "horizontal FOV" is less meaningful for equirectangular
      * projections, but the output is still undistorted.
      */
    void setAspectMode(render::AspectMode mode);

    /// @returns the current aspect-ratio fit mode.
    render::AspectMode aspectMode() const;

    /**
      * Set the intrinsic aspect ratio (width / height) used by
      * `FitExact` mode. Ignored in other modes. Values ≤ 0 default
      * to 4:3 inside the view plane.
      */
    void setAspectRatio(double ratio);

    /// @returns the intrinsic aspect ratio passed to `FitExact` mode.
    double aspectRatio() const;

    const Matrix4d& matrix() const;

    /**
      * Cached inverse of `matrix()`, mapping world-space points into
      * camera space. Projection-heavy renderers call this for every
      * vertex; caching keeps them from recomputing a 4x4 inverse per
      * projected point.
      */
    const Matrix4d& inverseMatrix() const;

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
    void render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& buffer) const;
    virtual void render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& buffer,
                        const Rect<int>& rect) const;
    void render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& buffer,
                const Rect<int>& rect, std::uint64_t tileSeed) const;

    /**
      * Render into a packed-RGB display buffer with inline
      * tonemapping. This is the path used by interactive display
      * widgets (`RenderWidget` / `QtDisplay` / `Modeler`):
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
    virtual void render(std::shared_ptr<render::RayCaster> raycaster, Buffer<unsigned int>& buffer,
                        std::shared_ptr<render::Tonemap> tonemap, const Rect<int>& rect) const;
    void render(std::shared_ptr<render::RayCaster> raycaster, Buffer<unsigned int>& buffer,
                std::shared_ptr<render::Tonemap> tonemap, const Rect<int>& rect,
                std::uint64_t tileSeed) const;

    /**
      * Render one tile into both an HDR graph resource and a packed-RGB display
      * buffer. This preserves the progressive-display property of the LDR path
      * while still keeping the HDR color data needed by downstream graph passes
      * such as FXAA/SMAA.
      */
    virtual void render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& hdrBuffer,
                        Buffer<unsigned int>& displayBuffer,
                        std::shared_ptr<render::Tonemap> tonemap, const Rect<int>& rect) const;
    void render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& hdrBuffer,
                Buffer<unsigned int>& displayBuffer, std::shared_ptr<render::Tonemap> tonemap,
                const Rect<int>& rect, std::uint64_t tileSeed) const;

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
      * (`Wireframe`, future `OpenGLEngine`) that need to map
      * mesh vertices onto the display.
      *
      * Returns `Vector2d::undefined` if:
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
      * Like `projectPoint` but additionally returns a depth value in
      * `result.z()` — the eye-relative distance along the camera's
      * forward axis. Smaller depth = closer to the eye; depth is
      * always positive for points in front of the eye. Returns
      * `Vector3d::undefined` when projection is undefined (point
      * behind eye / camera has no closed-form inverse).
      *
      * Consumed by the software rasterizer for Z-buffer depth tests
      * and for perspective-correct interpolation of per-vertex
      * attributes across a triangle (the standard `1/z` trick:
      * linear in screen space, hyperbolic in view space).
      *
      * Default implementation returns `(projectPoint.x, .y, 0)` —
      * usable for orthogonal-style cameras with no perspective, but
      * subclasses with perspective foreshortening should override
      * to populate `z` with the meaningful eye-relative distance.
      */
    virtual Vector3d projectPointWithDepth(const Vector3d& worldPoint) const;

    /**
      * Forward projection before the perspective divide, in the
      * software rasterizer's clip-space convention.
      *
      * `x / w` and `y / w` are normalized viewport coordinates in
      * `[-1, 1]`, where `(-1, -1)` maps to the framebuffer's
      * top-left corner and `(1, 1)` maps to the bottom-right corner.
      * `z` is the positive eye-relative depth used by the rasterizer
      * Z-buffer, and `w` is the perspective divisor. Orthographic
      * cameras use `w = 1`.
      *
      * Unlike `projectPoint` / `projectPointWithDepth`, this method
      * can return defined values for points behind the eye so the
      * rasterizer can clip straddling triangles in homogeneous space
      * before the divide. The base implementation returns undefined;
      * cameras without a closed-form projection stay unsupported by
      * software rasterization.
      *
      * The widget below shows the same world point through the two
      * camera projections currently supported by the rasterizer. In
      * pinhole mode, `w` is the signed eye-relative depth, so the
      * projected pixel is only known after the `x / w`, `y / w`
      * perspective divide. In orthographic mode, rays stay parallel
      * and `w = 1`, so clip coordinates are already linear in screen
      * space. Drag the world point through and behind the camera to
      * see why the rasterizer clips in homogeneous space before that
      * divide.
      *
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="camera_forward_projection.js"></script>
      * @endhtmlonly
      */
    virtual Vector4d projectPointToClipSpace(const Vector3d& worldPoint) const;

    /**
      * @returns the world-space → OpenGL clip-space matrix for cameras
      * whose projection is a single 4×4 affine-plus-frustum transform
      * (pinhole, orthographic, and inheritors thereof), or `std::nullopt`
      * for cameras whose forward projection cannot be expressed as a
      * matrix (thin-lens, fish-eye, spherical, equirectangular). GPU
      * backends that can perform per-vertex projection consult this
      * method to install a uniform; backends fall back to per-vertex CPU
      * projection when this returns `std::nullopt`. The matrix that is
      * returned must produce clip-space output compatible with the
      * OpenGL pipeline (perspective divide gives NDC in [-1, 1] for x/y
      * and the depth-buffer range for z).
      *
      * Implementations require the camera's view plane to have been set
      * up first (`viewPlane()->setup(...)`); callers that have not yet
      * done so should pass through `projectPointToClipSpace` instead.
      */
    virtual std::optional<Matrix4d> worldToClipMatrix() const;

    /**
      * Eye-relative depth scalar for the world-space point — positive
      * in front of the camera, negative behind it. Used by the
      * software rasterizer's Sutherland-Hodgman clipper to trim
      * triangles straddling the configured near/far planes (otherwise such
      * triangles get dropped entirely, since `projectPointWithDepth`
      * returns undefined for behind-eye points).
      *
      * Default returns 0 — projection is undefined for cameras
      * without a closed-form forward projection. Subclasses with
      * meaningful depth (Pinhole, Orthographic, and inheritors)
      * override.
      */
    virtual double eyeRelativeDepth(const Vector3d& worldPoint) const;

    /**
      * Convenience overload that uses a `NullSampleStream` returning
      * the centre of every dimension. Useful for tests and ad-hoc
      * callers (e.g. UI pixel-pick paths) that don't have a
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
      m_cancelled.store(true, std::memory_order_release);
    }

    inline bool isCancelled() const {
      return m_cancelled.load(std::memory_order_acquire);
    }

    inline void uncancel() {
      m_cancelled.store(false, std::memory_order_release);
    }

  protected:
    void copyBaseStateTo(Camera& camera) const;

    /// Write `color` (already divided by sample count) into every
    /// pixel of the iterator's footprint — single pixel for the
    /// regular iterator, the size×size block for interlaced
    /// iterators that haven't refined yet.
    void plot(Buffer<Colord>& buffer, const Recti& rect, const render::ViewPlane::Iterator& pixel,
              const Colord& color) const;

    /// LDR variant — writes a packed-RGB pixel value (the result of
    /// `tonemap->apply(color).rgb()` from the LDR camera path). The
    /// footprint logic matches `plot(Buffer<Colord>&, ...)` so
    /// interlaced iterators show the same coarse-then-refine
    /// progression in either output buffer.
    void plotRGB(Buffer<unsigned int>& buffer, const Recti& rect,
                 const render::ViewPlane::Iterator& pixel, unsigned int rgb) const;

  private:
    std::optional<Colord> sampleRayColor(std::shared_ptr<render::RayCaster> raycaster,
                                         const render::ViewPlane::Iterator& pixel, int sampleIndex,
                                         std::optional<std::uint64_t> tileSeed) const;
    std::size_t accumulationIndex(const Rect<int>& rect,
                                  const render::ViewPlane::Iterator& pixel) const;
    void renderProgressiveSamples(std::shared_ptr<render::RayCaster> raycaster,
                                  Buffer<Colord>& buffer, const Rect<int>& rect,
                                  std::optional<std::uint64_t> tileSeed) const;
    void renderProgressiveSamples(std::shared_ptr<render::RayCaster> raycaster,
                                  Buffer<unsigned int>& buffer,
                                  std::shared_ptr<render::Tonemap> tonemap, const Rect<int>& rect,
                                  std::optional<std::uint64_t> tileSeed) const;
    void renderProgressiveSamples(std::shared_ptr<render::RayCaster> raycaster,
                                  Buffer<Colord>& hdrBuffer, Buffer<unsigned int>& displayBuffer,
                                  std::shared_ptr<render::Tonemap> tonemap, const Rect<int>& rect,
                                  std::optional<std::uint64_t> tileSeed) const;

    void render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& buffer,
                const Rect<int>& rect, std::optional<std::uint64_t> tileSeed) const;
    void render(std::shared_ptr<render::RayCaster> raycaster, Buffer<unsigned int>& buffer,
                std::shared_ptr<render::Tonemap> tonemap, const Rect<int>& rect,
                std::optional<std::uint64_t> tileSeed) const;
    void render(std::shared_ptr<render::RayCaster> raycaster, Buffer<Colord>& hdrBuffer,
                Buffer<unsigned int>& displayBuffer, std::shared_ptr<render::Tonemap> tonemap,
                const Rect<int>& rect, std::optional<std::uint64_t> tileSeed) const;

    std::atomic<bool> m_cancelled;
    bool m_showProgressIndicators;
    render::AspectMode m_aspectMode;
    double m_aspectRatio;
    Vector3d m_position, m_target;
    mutable MemoizedValue<Matrix4d> m_matrix;
    mutable MemoizedValue<Matrix4d> m_inverseMatrix;
    std::shared_ptr<render::ViewPlane> m_viewPlane;
  };
}
