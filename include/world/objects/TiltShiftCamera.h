#pragma once
#include <memory>

#include "world/objects/ThinLensCamera.h"
#include "core/math/Angle.h"
#include "core/math/Vector.h"

/**
  * Tilt-shift / Scheimpflug camera — a thin-lens camera with a tilted
  * focal plane and an optionally shifted lens. See
  * `render::TiltShiftCamera` for the geometric derivation and the
  * canonical use cases (miniature effect, tilted-plane focus,
  * converging-vertical correction).
  *
  * @image html tilt_shift_camera_miniature.png "TiltShift with tilt=20° produces the miniature effect"
  *
  * Editable wrapper. Inherits the four `ThinLensCamera` Q_PROPERTYs
  * (distance, zoom, apertureRadius, focalDistance) and adds two new
  * ones: `tilt` (the focal-plane rotation angle) and `shift` (lens
  * lateral offset). The property editor builds a spinbox / vector
  * widget for each automatically.
  *
  * ### Empirical exploration
  *
  * Open `scenes/tilt_shift_demo.json` for
  * a pre-built scene, or pick **Edit → Add Camera → Tilt-Shift Camera**
  * in a fresh scene. Adjust the `tilt` control through `0..45°` to watch
  * the focal plane rotate; the focus band sweeps across the scene
  * accordingly.
  */
class TiltShiftCamera : public ThinLensCamera {
  Q_OBJECT
  Q_PROPERTY(Angled tilt READ tilt WRITE setTilt)
  Q_PROPERTY(double shiftX READ shiftX WRITE setShiftX)
  Q_PROPERTY(double shiftY READ shiftY WRITE setShiftY)

public:
  /**
    * Constructs a TiltShiftCamera with the same defaults as
    * `ThinLensCamera`, plus zero tilt and zero shift — geometrically
    * identical to a `ThinLensCamera` until the user dials tilt or
    * shift.
    */
  explicit TiltShiftCamera(Element* parent = nullptr);

  /**
    * @returns the focal-plane tilt angle. Zero is "perpendicular
    * focal plane" (parent ThinLens behaviour).
    */
  inline const Angled& tilt() const {
    return m_tilt;
  }

  /**
    * Sets the focal-plane tilt angle. Rotates the focal plane around
    * the camera's local right axis; positive values lean the *top*
    * of the focal plane toward the camera (the miniature direction).
    *
    * <table><tr>
    * <td>@image html tilt_shift_camera_tilt_0.png "tilt=0° (equivalent to ThinLens)"</td>
    * <td>@image html tilt_shift_camera_tilt_10.png "tilt=10°"</td>
    * <td>@image html tilt_shift_camera_tilt_20.png "tilt=20°"</td>
    * <td>@image html tilt_shift_camera_tilt_30.png "tilt=30°"</td>
    * <td>@image html tilt_shift_camera_tilt_45.png "tilt=45° — extreme"</td>
    * </tr></table>
    */
  inline void setTilt(const Angled& angle) {
    m_tilt = angle;
  }

  /// @returns the X component of the lens shift (camera-local
  /// right basis). Split from a single Vector2d into two scalar
  /// Q_PROPERTYs purely for property-editor plumbing — the world
  /// layer doesn't yet have a Vector2d parameter widget.
  inline double shiftX() const {
    return m_shiftX;
  }

  /**
    * Sets the X component of the lens shift. Slides the principal
    * ray's apparent direction laterally — the geometric equivalent
    * of physically shifting a real tilt-shift lens parallel to the
    * sensor. Used in architectural photography to correct converging
    * verticals when shooting tall structures from below.
    *
    * Independent of `tilt`. Doesn't affect depth of field; only the
    * apparent framing.
    */
  inline void setShiftX(double value) {
    m_shiftX = value;
  }

  /// @returns the Y component of the lens shift (camera-local
  /// up basis).
  inline double shiftY() const {
    return m_shiftY;
  }

  /// Sets the Y component of the lens shift. See `setShiftX` for
  /// the geometry.
  inline void setShiftY(double value) {
    m_shiftY = value;
  }

  /**
    * Convert this editable camera into the runtime
    * `render::TiltShiftCamera`. Copies the inherited ThinLens
    * parameters plus the two new tilt-shift parameters.
    */
  virtual std::shared_ptr<render::Camera> toRaytracer() const;

private:
  Angled m_tilt;
  double m_shiftX;
  double m_shiftY;
};
