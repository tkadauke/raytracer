#pragma once
#include <memory>

#include "render/cameras/Camera.h"

namespace render {
  /**
    * @brief Pinhole camera with a finite aperture — produces depth of field.
    *
    * The thin-lens camera is the simplest physically motivated camera that
    * exhibits **depth of field**: objects at the focal distance render
    * sharp, while everything closer or farther blurs into a circular
    * **bokeh** disc whose size scales with both the aperture radius and
    * the out-of-focus distance.
    *
    * @image html thin_lens_camera_dof.png "ThinLens with apertureRadius=0.2, focalDistance=8 — green sphere is in focus, red and blue are blurred"
    *
    * ### How it works
    *
    * The pinhole camera fires a single ray per pixel from a single point
    * (the eye) through the pixel on the viewplane. Every ray is sharp by
    * construction — there's no way for a scene point to land in multiple
    * pixels.
    *
    * The thin-lens camera replaces the single-point eye with a **disc** of
    * radius `apertureRadius` centred at the same eye location. For each
    * pixel sample, a fresh point on that disc is chosen as the ray origin.
    * The ray is then bent so that, regardless of which lens point we
    * picked, **all rays for the same pixel pass through the same point on
    * the focal plane** — the plane perpendicular to the camera's forward
    * axis at distance `focalDistance` in front of the lens. That's the
    * geometric definition of "in focus".
    *
    * The interactive figure below shows that convergence in a side-view
    * cross-section. Click and drag horizontally to slide the focal
    * plane: the rays still converge at it regardless of where it sits.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="thin_lens_camera_convergence.js"></script>
    * @endhtmlonly
    *
    * For scene points that lie *off* the focal plane, the rays from
    * different lens samples hit the scene at different positions. Average
    * those samples together and you get the circle-of-confusion blur that
    * we recognise as defocus blur. The blur diameter at depth `d` is
    * approximately:
    *
    *     blurDiameter ≈ apertureRadius × |1 - focalDistance / d|
    *
    * which is zero at the focal plane and grows linearly with the
    * out-of-focus distance.
    *
    * ### What this model leaves out
    *
    * The "thin lens" approximation collapses every real-lens element down
    * to a single ideal disc. That means **no chromatic aberration** (no
    * per-wavelength refraction), **no geometric distortion** (no barrel /
    * pincushion / fisheye behaviour from non-paraxial rays), **no
    * vignetting** (no light falloff at the image edges), and **no flare**
    * (no internal reflections between elements). For physically accurate
    * modelling of those effects, see Kolb et al. 1995, "A Realistic
    * Camera Model for Computer Graphics".
    *
    * ### Sampling caveat
    *
    * Each call to `rayForPixel(x, y, stream)` pulls a single 2D
    * sample on the lens disc from `stream.next2D()`. The renderer
    * has already consumed dimension 0 for sub-pixel jitter, so the
    * lens sample lives on dimension 1 — *independently stratified*
    * from the sub-pixel jitter. The sample is routed through the
    * concentric square-to-disc mapping (Shirley 1997) below; the
    * resulting disc points inherit whatever stratification the
    * active sampler provides (jittered, multi-jittered, future
    * Sobol). At N samples per pixel you get N stratified lens
    * points, not N random ones, which drops the dominant bokeh-noise
    * term from `O(1/√N)` to `O(1/N)`.
    *
    * The widget below visualises that mapping. Drag horizontally to
    * change the grid density (N×N samples). The disc plot on the right
    * shows the resulting lens-sample distribution: stratified, no
    * rejection, no clustering at the centre.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="thin_lens_camera_disc_sampling.js"></script>
    * @endhtmlonly
    *
    * To make this work without surprising the user, `setViewPlane`
    * detects when the incoming viewplane has the factory-default 1-spp
    * sampler and replaces it with a `JitteredSampler` at 16 samples
    * per pixel — that's enough to keep the SceneBrowser preview from
    * showing confetti at the default settings. If the caller has
    * already attached a multi-sample sampler (rendercli does this for
    * `--samples_per_pixel`; GeneratedRayTracer's RenderWindow does it
    * for the GUI's "Samples per pixel" field), the auto-install is
    * skipped and the caller's choice wins.
    *
    * @see PinholeCamera — the zero-aperture limit case.
    * @see rayForPixelWithLens — deterministic-aperture overload for tests.
    */
  class ThinLensCamera : public Camera {
  public:
    using Camera::rayForPixel;

    /**
      * Construct a thin-lens camera with reasonable defaults: distance 5,
      * zoom 1, apertureRadius 0.1, focalDistance 5. The defaults give
      * mild bokeh that's clearly visible without overwhelming the scene.
      */
    inline ThinLensCamera()
      : Camera(),
        m_distance(5),
        m_zoom(1),
        m_apertureRadius(0.1),
        m_focalDistance(5)
    {
    }

    /**
      * Construct a thin-lens camera at the given world position looking
      * at the given target. Aperture/focal-distance defaults match the
      * no-arg constructor.
      */
    inline explicit ThinLensCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target),
        m_distance(5),
        m_zoom(1),
        m_apertureRadius(0.1),
        m_focalDistance(5)
    {
    }

    /**
      * Generate a primary ray for pixel `(x, y)`, pulling the lens-disc
      * sample from `stream.next2D()`. Called by the per-pixel render
      * loop; multiple invocations for the same pixel see different
      * `sampleIndex` values and therefore different lens points,
      * which is what produces the DOF blur in the final image.
      *
      * The renderer guarantees the stream is positioned past
      * dimension 0 (sub-pixel jitter is consumed by the renderer
      * itself), so the `next2D` call here returns a fresh
      * stratified dimension that's *independent* of the sub-pixel
      * jitter — fixing the residual correlation that the previous
      * "reuse the sub-pixel sample as the lens sample" hack lived
      * with.
      *
      * @see rayForPixelWithLens for the deterministic-aperture overload
      *      used by tests.
      */
    virtual Rayd rayForPixel(double x, double y, render::SampleStream& stream) const;

    /**
      * Generate a primary ray for pixel `(x, y)` with an explicit
      * lens-disc sample at `(lensU, lensV)` ∈ unit disc.
      *
      * The caller is responsible for keeping `lensU² + lensV² ≤ 1` — out-
      * of-bounds samples produce rays from outside the lens and break the
      * focal-plane convergence guarantee. Useful for deterministic tests
      * where you want repeatable lens samples; production code calls
      * `rayForPixel(x, y)` which generates a fresh random sample.
      *
      * Virtual so subclasses with different focal-plane geometry
      * (`TiltShiftCamera`) can keep the parent's `rayForPixel`
      * concentric-mapping wrapper and override only the
      * pinhole-to-focal-point math.
      */
    virtual Rayd rayForPixelWithLens(double x, double y, double lensU, double lensV) const;

    /**
      * @returns the eye-to-viewplane distance.
      */
    inline double distance() const { return m_distance; }

    /**
      * Sets the distance between the eye and the viewplane. A lower
      * distance results in a higher field of view — same role as
      * `PinholeCamera::distance`. Doesn't affect the depth of field
      * directly; for that, see `setApertureRadius` and `setFocalDistance`.
      */
    inline void setDistance(double distance) { m_distance = distance; }

    /**
      * @returns the magnification factor applied to the viewplane.
      */
    inline double zoom() const { return m_zoom; }

    /**
      * Sets the zoom of the camera.
      */
    inline void setZoom(double zoom) {
      m_zoom = zoom;
      viewPlane()->setPixelSize(1.0 / m_zoom);
    }

    /**
      * @returns the aperture radius in scene units.
      */
    inline double apertureRadius() const { return m_apertureRadius; }

    /**
      * Sets the aperture radius. Larger aperture → more defocus blur on
      * out-of-focus geometry. Zero degenerates to a pinhole (no DOF).
      * Negative values are coerced to zero.
      *
      * <table><tr>
      * <td>@image html thin_lens_camera_dof_aperture_0.0.png "apertureRadius=0 (pinhole — everything sharp)"</td>
      * <td>@image html thin_lens_camera_dof_aperture_0.2.png "apertureRadius=0.2"</td>
      * <td>@image html thin_lens_camera_dof_aperture_0.4.png "apertureRadius=0.4"</td>
      * <td>@image html thin_lens_camera_dof_aperture_0.6.png "apertureRadius=0.6"</td>
      * <td>@image html thin_lens_camera_dof_aperture_0.8.png "apertureRadius=0.8 (heavy bokeh)"</td>
      * </tr></table>
      */
    inline void setApertureRadius(double radius) {
      m_apertureRadius = radius < 0 ? 0 : radius;
    }

    /**
      * @returns the focal distance in scene units, measured along the
      * camera's forward axis from the lens to the in-focus plane.
      */
    inline double focalDistance() const { return m_focalDistance; }

    /**
      * Sets the focal distance. Slides the in-focus plane through the
      * scene; objects whose distance from the lens matches `focalDistance`
      * render sharp, others blur. The setter ignores zero or negative
      * inputs (which would put the focus plane at or behind the lens —
      * a degenerate case where the math breaks down) rather than
      * clamping to a tiny epsilon, which would silently shift the user's
      * intended focus.
      *
      * <table><tr>
      * <td>@image html thin_lens_camera_dof_focal_5.5.png "focalDistance=5.5 (front sphere sharp)"</td>
      * <td>@image html thin_lens_camera_dof_focal_6.75.png "focalDistance=6.75"</td>
      * <td>@image html thin_lens_camera_dof_focal_8.png "focalDistance=8 (middle sphere sharp)"</td>
      * <td>@image html thin_lens_camera_dof_focal_9.25.png "focalDistance=9.25"</td>
      * <td>@image html thin_lens_camera_dof_focal_10.5.png "focalDistance=10.5 (back sphere sharp)"</td>
      * </tr></table>
      */
    inline void setFocalDistance(double distance) {
      m_focalDistance = distance > 0 ? distance : m_focalDistance;
    }

    /**
      * Override of `Camera::setViewPlane` that also installs a
      * `JitteredSampler` at 16 samples per pixel. ThinLens is fundamentally
      * a multi-sample camera — with one sample per pixel the lens-disc
      * randomness produces noise instead of blur, so the auto-installed
      * sampler is what makes interactive tools like SceneBrowser usable
      * out of the box. Callers (e.g. `rendercli`) can override by setting
      * a different sampler on the viewplane after construction.
      */
    virtual void setViewPlane(std::shared_ptr<render::ViewPlane> plane);

  private:
    double m_distance;
    double m_zoom;
    double m_apertureRadius;
    double m_focalDistance;
  };
}
