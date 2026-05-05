#pragma once
#include <memory>

#include "world/objects/Camera.h"

/**
  * Thin-lens cameras produce a perspective projection like a pinhole, but
  * with a finite-radius aperture that creates **depth of field** —
  * objects at the focal distance render sharp while everything else
  * blurs into a circular bokeh disc.
  *
  * @image html thin_lens_camera_dof.png "Thin-lens camera with apertureRadius=0.2, focalDistance=8"
  *
  * Editable wrapper for `raytracer::ThinLensCamera`. Exposes the four
  * tunable parameters as `Q_PROPERTY`s so the SceneBrowser /
  * GeneratedRayTracer property editors build double-spinbox controls for
  * them automatically.
  *
  * See `raytracer::ThinLensCamera` for the underlying physical model and
  * the geometric derivation of the focal-plane convergence guarantee.
  *
  * ### Empirical exploration
  *
  * Open `examples/GeneratedRayTracer/scenes/dof_demo.json` in
  * GeneratedRayTracer (File → Open) for a pre-built three-sphere scene,
  * or pick **Edit → Add Camera → Thin Lens Camera (DOF)** in a fresh
  * scene. Selecting the camera node exposes the four sliders below;
  * dragging them re-renders the preview live.
  *
  * For headless renders, use `rendercli` against the same scene file:
  * `rendercli --width 800 --height 600 --sampler MultiJittered
  * --samples_per_pixel 64 examples/GeneratedRayTracer/scenes/dof_demo.json
  * out.png`. Bump the sample count to 64+ for clean DOF — 1-sample
  * renders look like shifted pinholes rather than blurred photographs.
  */
class ThinLensCamera : public Camera {
  Q_OBJECT;
  Q_PROPERTY(double distance READ distance WRITE setDistance);
  Q_PROPERTY(double zoom READ zoom WRITE setZoom);
  Q_PROPERTY(double apertureRadius READ apertureRadius WRITE setApertureRadius);
  Q_PROPERTY(double focalDistance READ focalDistance WRITE setFocalDistance);

public:
  /**
    * Constructs a ThinLensCamera with the standard defaults: distance 5,
    * zoom 1, apertureRadius 0.1, focalDistance 5. Mild bokeh that's
    * visible without overwhelming a typical scene.
    */
  explicit ThinLensCamera(Element* parent = nullptr);

  /**
    * @returns the eye-to-viewplane distance. Defaults to 5.
    */
  inline double distance() const { return m_distance; }

  /**
    * Sets the distance between the eye and the viewplane. A lower distance
    * results in a higher field of view. Affects framing only; depth of
    * field is controlled separately via `setApertureRadius` and
    * `setFocalDistance`.
    */
  inline void setDistance(double d) { m_distance = d; }

  /**
    * @returns the viewplane magnification factor. Defaults to 1.
    */
  inline double zoom() const { return m_zoom; }

  /**
    * Sets the zoom of the camera. Setter coerces zero or negative values
    * to 1 (the math is undefined at zero zoom).
    */
  inline void setZoom(double z) { m_zoom = z <= 0 ? 1 : z; }

  /**
    * @returns the lens aperture radius in scene units. Larger →
    * shallower depth of field. Defaults to 0.1.
    */
  inline double apertureRadius() const { return m_apertureRadius; }

  /**
    * Sets the aperture radius. The setter coerces negative values to 0
    * (which renders as a pinhole — no DOF at all).
    *
    * <table><tr>
    * <td>@image html thin_lens_camera_dof_aperture_0.0.png "apertureRadius=0 (pinhole — everything sharp)"</td>
    * <td>@image html thin_lens_camera_dof_aperture_0.2.png "apertureRadius=0.2"</td>
    * <td>@image html thin_lens_camera_dof_aperture_0.4.png "apertureRadius=0.4"</td>
    * <td>@image html thin_lens_camera_dof_aperture_0.6.png "apertureRadius=0.6"</td>
    * <td>@image html thin_lens_camera_dof_aperture_0.8.png "apertureRadius=0.8 (heavy bokeh)"</td>
    * </tr></table>
    */
  inline void setApertureRadius(double r) { m_apertureRadius = r < 0 ? 0 : r; }

  /**
    * @returns the focal distance — how far in front of the lens the
    * in-focus plane sits, in scene units. Defaults to 5.
    */
  inline double focalDistance() const { return m_focalDistance; }

  /**
    * Sets the focal distance. Setter ignores non-positive values rather
    * than clamping (zero or negative would put the focus at or behind
    * the lens, which is geometrically degenerate); the previous valid
    * value is kept.
    *
    * <table><tr>
    * <td>@image html thin_lens_camera_dof_focal_5.5.png "focalDistance=5.5 (front sphere sharp)"</td>
    * <td>@image html thin_lens_camera_dof_focal_6.75.png "focalDistance=6.75"</td>
    * <td>@image html thin_lens_camera_dof_focal_8.png "focalDistance=8 (middle sphere sharp)"</td>
    * <td>@image html thin_lens_camera_dof_focal_9.25.png "focalDistance=9.25"</td>
    * <td>@image html thin_lens_camera_dof_focal_10.5.png "focalDistance=10.5 (back sphere sharp)"</td>
    * </tr></table>
    */
  inline void setFocalDistance(double d) { m_focalDistance = d > 0 ? d : m_focalDistance; }

  /**
    * Convert this editable camera into the runtime `raytracer::ThinLensCamera`
    * used by the renderer. Copies all four parameters across.
    */
  virtual std::shared_ptr<raytracer::Camera> toRaytracer() const;

private:
  double m_distance;
  double m_zoom;
  double m_apertureRadius;
  double m_focalDistance;
};
