#pragma once
#include <memory>

#include "render/cameras/ThinLensCamera.h"
#include "core/math/Angle.h"
#include "core/math/Vector.h"

namespace render {
  /**
    * @brief Thin-lens camera with a tilted focal plane and a
    *        shiftable lens — the optical setup used by tilt-shift
    *        photographers and Scheimpflug architectural cameras.
    *
    * @image html tilt_shift_camera_miniature.png "TiltShift with tilt=20° produces the iconic 'miniature' effect — only a narrow band of the scene is in focus, the rest blurs as if shot from extreme close-up."
    *
    * Where `ThinLensCamera`'s focal plane is always perpendicular to
    * the camera's forward axis, `TiltShiftCamera` lets the focal
    * plane *rotate* off-perpendicular. Two consequences flow from
    * that one geometric change:
    *
    *  - **The miniature / fake-tilt-shift effect.** Tilting the
    *    focal plane to be near-horizontal puts only a narrow strip
    *    of the actual ground in focus; the foreground and
    *    background blur as steeply as if the camera were 10cm from
    *    a tabletop diorama. A real architectural tilt-shift lens at
    *    moderate tilt produces this; the effect is dramatic and
    *    immediately recognisable.
    *
    *  - **Tilted-plane focus for landscapes / tabletops.** Tilting
    *    the focal plane to *match* a real plane in the scene (e.g.
    *    a long horizontal table or a distant landscape) keeps the
    *    entire surface in focus from foreground to background
    *    *simultaneously*, even at wide apertures — which is
    *    impossible with a normal lens.
    *
    * ### The Scheimpflug principle
    *
    * In a real tilt-shift lens, the *image plane*, the *lens plane*,
    * and the *focal plane* must all meet at a common line for
    * everything on the focal plane to be in focus. That's the
    * Scheimpflug principle (named after Theodor Scheimpflug, 1904):
    * if the three planes intersect, the focus is sharp; if they
    * don't, focus is uneven across the frame.
    *
    * This implementation makes a small simplification: only the
    * focal plane rotates; the image plane and lens plane stay
    * perpendicular to the forward axis. The visible effect (band
    * of focus, miniature look) is correct; the per-pixel sharpness
    * across the tilted plane is *idealised* — every point on the
    * tilted plane projects to a single ray through every lens
    * sample, so they're all in focus regardless of whether the
    * physical Scheimpflug condition would say so.
    *
    * For the canonical "physically accurate Scheimpflug" with all
    * three planes obeying the convergence rule, see the future
    * `KolbCamera` implementation — that's the right place for
    * full physical lens modelling.
    *
    * The widget below makes the geometric idea concrete. Use the
    * tilt slider: at `tilt = 0` the focal plane is perpendicular to
    * forward and *every* pixel focuses at the same depth (pure
    * `ThinLensCamera` behavior). As the slider changes, the focal
    * plane rotates and the focal points for pixels above vs below
    * the optical axis separate — that's the entire reason a single
    * tilted focal plane can keep a long horizontal surface sharp
    * end-to-end (or, with steep tilt, compress the focus to a
    * narrow band for the miniature look).
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="tilt_shift_camera_scheimpflug.js"></script>
    * @endhtmlonly
    *
    * ### Lens shift (the other half of "tilt-shift")
    *
    * Beyond tilt, real tilt-shift lenses also support **shift** —
    * sliding the lens parallel to the image plane to correct
    * converging verticals (e.g., shooting a tall building from
    * below without the verticals converging in the image). This
    * implementation models shift as a per-pixel offset to the
    * pinhole direction: `setShift({x, y})` shifts the principal
    * ray's direction by `(x, y)` in the camera's right/up basis,
    * which is geometrically equivalent to shifting the lens
    * laterally relative to the sensor.
    *
    * Tilt and shift are independent — set either or both. With
    * both at zero, the camera behaves identically to the parent
    * `ThinLensCamera`.
    *
    * ### Sampling
    *
    * Same per-ray sample stream as `ThinLensCamera` — the lens
    * disc sample is pulled from `stream.next2D()`, decorrelated
    * from the renderer's sub-pixel jitter (dim 0) and time sample
    * (dim 1). The stratification properties from §SampleStream
    * apply unchanged.
    *
    * @see ThinLensCamera — the perpendicular-focal-plane parent.
    */
  class TiltShiftCamera : public ThinLensCamera {
  public:
    /**
      * Construct a tilt-shift camera with the same defaults as
      * `ThinLensCamera`, plus zero tilt and zero shift — i.e.,
      * geometrically identical to a `ThinLensCamera` until you
      * call `setTilt` or `setShift`.
      */
    inline TiltShiftCamera()
        : ThinLensCamera(),
          m_tilt(0_degrees),
          m_shift(0, 0) {
    }

    inline explicit TiltShiftCamera(const Vector3d& position, const Vector3d& target)
        : ThinLensCamera(position, target),
          m_tilt(0_degrees),
          m_shift(0, 0) {
    }

    /**
      * Generate a primary ray for pixel `(x, y)` with an explicit
      * lens-disc sample. Overrides `ThinLensCamera::rayForPixelWithLens`
      * to:
      *   1. Apply lens `shift` to the pinhole direction (laterally
      *      offsets the field of view without rotating the camera).
      *   2. Compute the focal point against the tilted focal plane
      *      instead of the perpendicular one — that's what produces
      *      the miniature / Scheimpflug-style focus.
      *
      * The lens-disc origin offset (the part responsible for DOF
      * blur) is unchanged from the parent.
      */
    Rayd rayForPixelWithLens(double x, double y, double lensU, double lensV) const override;
    std::shared_ptr<Camera> clone() const override;
    const char* fingerprintType() const override;

    /// @returns the focal-plane tilt angle. Zero means
    /// perpendicular to the forward axis (parent ThinLens
    /// behaviour); positive values tilt the *top* of the focal
    /// plane toward the camera (i.e., the focal plane leans
    /// downward into the foreground), which is the canonical
    /// "miniature" direction.
    inline Angled tilt() const {
      return m_tilt;
    }

    /**
      * Sets the focal-plane tilt angle. Tilt rotates the focal-
      * plane normal around the camera's *right* axis. The image
      * plane and lens plane stay perpendicular to forward — the
      * full Scheimpflug condition (all three planes converging at
      * a line) is approximated, not enforced; see the class
      * docstring for the trade-off.
      *
      * <table><tr>
      * <td>@image html tilt_shift_camera_tilt_0.png "tilt=0° — equivalent to ThinLensCamera"</td>
      * <td>@image html tilt_shift_camera_tilt_10.png "tilt=10°"</td>
      * <td>@image html tilt_shift_camera_tilt_20.png "tilt=20°"</td>
      * <td>@image html tilt_shift_camera_tilt_30.png "tilt=30°"</td>
      * <td>@image html tilt_shift_camera_tilt_45.png "tilt=45° — extreme"</td>
      * </tr></table>
      */
    inline void setTilt(const Angled& angle) {
      m_tilt = angle;
    }

    /// @returns the lens shift vector (in camera right/up basis).
    inline const Vector2d& shift() const {
      return m_shift;
    }

    /**
      * Sets the lens shift — slides the lens parallel to the
      * sensor, geometrically equivalent to off-axis perspective
      * projection. Used in real photography to correct converging
      * verticals when shooting tall structures from below. Unlike
      * tilt, shift doesn't change the depth of field — only the
      * apparent direction the camera "looks" through the frame.
      *
      * Components are in camera-local right/up units; typical
      * values are in `[-0.5, 0.5]`.
      */
    inline void setShift(const Vector2d& shift) {
      m_shift = shift;
    }

  private:
    Angled m_tilt;
    Vector2d m_shift;
  };
}
