#pragma once
#include <memory>

#include "render/cameras/Camera.h"

namespace render {
  /**
    * @brief Full-sphere panorama camera using the canonical
    *        equirectangular (latitude/longitude) projection.
    *
    * Maps every pixel in a 2:1-aspect output image to a point on the
    * unit sphere, then traces a ray from the camera position in that
    * direction. The result is the standard 360°×180° panorama format
    * used by Google Street View, VR environment maps, HDR sky
    * textures, and reflection probes.
    *
    * @image html equirectangular_camera.png "Equirectangular render — coloured spheres at every cardinal direction wrap around the full panorama"
    *
    * ### How it works
    *
    * Each pixel `(x, y)` in the output image gets a longitude/latitude
    * pair via a linear map from image coordinates to sphere
    * coordinates:
    *
    *     x = 0       → longitude = -π     (left edge,    behind camera)
    *     x = width   → longitude = +π     (right edge,   behind camera; same point as left edge — image wraps)
    *     y = 0       → latitude  = +π/2   (top edge,     north pole)
    *     y = height  → latitude  = -π/2   (bottom edge,  south pole)
    *
    * The `(longitude, latitude)` pair becomes a unit-sphere direction
    * via the standard parameterisation
    * \f$(\sin\phi\cos\theta,\ \sin\theta,\ \cos\phi\cos\theta)\f$
    * where φ = longitude and θ = latitude. The camera's local-to-world
    * matrix rotates that direction into world space; the resulting ray
    * has its origin at the camera position and that direction.
    *
    * Implementation note: the codebase's `Vector3d::up()` is
    * `(0, -1, 0)` (i.e. world-up is negative y), so the y component
    * of the unit-sphere direction is **negated** in the actual code
    * (`-sin(lat)` rather than `+sin(lat)`). Without that flip the
    * rendered panorama comes out upside-down. The mathematical
    * projection itself is unchanged.
    *
    * ### Aspect ratio
    *
    * For pixels to be roughly square at the equator, the output image
    * **must be 2:1 aspect** (e.g. 720×360, 2048×1024). Other aspect
    * ratios still render — just with anisotropic angular resolution
    * (longitude pixels denser/sparser than latitude pixels).
    *
    * ### Pole stretching and the seam
    *
    * The rendered output has two visual artefacts inherent to the
    * projection itself:
    *
    * - **Pole stretching**: the top and bottom rows correspond to a
    *   single point each (the north and south pole), so a single
    *   scene point at the pole gets smeared across the entire row.
    *   You can see this in the rendered example above: the floor
    *   directly below the camera covers the entire bottom strip.
    * - **The seam**: longitude is discontinuous across the left and
    *   right edges (-π and +π map to the same direction). A scene
    *   feature that sits behind the camera will appear cut in half:
    *   half of it on the left edge, half on the right.
    *
    * Both are properties of the equirectangular projection, not bugs.
    * Cubemap and stereographic cameras avoid them at the cost of
    * other distortions.
    *
    * ### Distinct from `SphericalCamera`
    *
    * `SphericalCamera` exposes `horizontalFieldOfView` and
    * `verticalFieldOfView` knobs and covers a *partial* sphere. This
    * camera always covers the full sphere with the canonical 2:1
    * mapping — there's nothing to tweak. If you need a partial
    * panorama with adjustable FOV, use `SphericalCamera`.
    *
    * @see SphericalCamera — partial-sphere panorama with FOV controls.
    */
  class EquirectangularCamera : public Camera {
  public:
    using Camera::rayForPixel;

    /**
      * Construct an equirectangular camera at the world origin looking
      * toward `(0, 0, 1)` (forward is +z). Combine with `setPosition`
      * and `setTarget` from the base class to point it elsewhere.
      */
    inline EquirectangularCamera()
      : Camera()
    {
    }

    /**
      * Construct at a specified position looking at a specified target.
      * The "up" axis is implicit and follows the same convention as
      * `Camera::matrix` (world-up via `Vector3d::up()`).
      */
    inline explicit EquirectangularCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target)
    {
    }

    /**
      * Generate the primary ray for pixel `(x, y)`. Origin is the camera
      * position; direction is the unit-sphere point at the longitude
      * /latitude implied by the pixel's image coordinates (see the
      * class-level mapping table).
      */
    virtual Rayd rayForPixel(double x, double y, render::SampleStream& stream) const;

  private:
    Vector3d direction(double x, double y) const;
  };
}
