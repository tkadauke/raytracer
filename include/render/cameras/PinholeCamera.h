#pragma once
#include <memory>

#include "render/cameras/Camera.h"

namespace render {
  /**
    * Pinhole cameras produce a perspective projection of the scene onto the
    * view plane.
    * 
    * <table><tr>
    * <td>@image html pinhole_camera_cube__raytracer.png "Raytracer"</td>
    * <td>@image html pinhole_camera_cube__raster.png "Rasterizer"</td>
    * <td>@image html pinhole_camera_cube__wireframe.png "Wireframe"</td>
    * </tr></table>
    */
  class PinholeCamera : public Camera {
  public:
    using Camera::rayForPixel;

    /**
      * Constructs a default pinhole camera with a zoom factor of 1 and an
      * eye-viewplane distance of 5, looking at the origin
      */
    inline PinholeCamera()
        : Camera(),
          m_distance(5),
          m_zoom(1) {
    }

    /**
      * Constructs a camera at position lookint at target.
      */
    inline explicit PinholeCamera(const Vector3d& position, const Vector3d& target)
        : Camera(position, target),
          m_distance(5),
          m_zoom(1) {
    }

    Rayd rayForPixel(double x, double y, render::SampleStream& stream) const override;
    std::shared_ptr<Camera> clone() const override;

    /**
      * Closed-form pinhole inverse of `rayForPixel`. Transforms
      * `worldPoint` into camera space, projects it through the
      * pinhole at `(0, 0, -distance)` onto the view plane at z=0,
      * then converts the camera-space plane coordinates back into
      * pixel coordinates using the same `topLeft` / `right` / `down`
      * basis the renderer uses forward.
      *
      * Returns `Vector2d::undefined()` if the point is at or behind
      * the eye (`z_cam ≤ -distance`), since perspective projection
      * is undefined there. Off-screen points project to valid (but
      * out-of-range) pixel coordinates — callers that need a
      * visibility check do their own bounds test.
      */
    Vector2d projectPoint(const Vector3d& worldPoint) const override;

    /**
      * Same projection math as `projectPoint` but additionally
      * returns the eye-relative distance along the camera's forward
      * axis in `result.z()`. Used by the software rasterizer's
      * Z-buffer for depth tests.
      */
    Vector3d projectPointWithDepth(const Vector3d& worldPoint) const override;

    /**
      * Homogeneous form of the same projection. `x / w` and `y / w`
      * are normalized viewport coordinates; `z` and `w` both carry
      * the positive eye-relative depth. Points behind the eye return
      * finite clip coordinates with `w <= 0` so the rasterizer can
      * clip edges before the perspective divide.
      *
      * @see Camera::projectPointToClipSpace for an interactive
      *      comparison with orthographic projection.
      */
    Vector4d projectPointToClipSpace(const Vector3d& worldPoint) const override;

    /// Signed eye-relative depth — positive in front of the eye,
    /// negative behind. Used by the rasterizer's near-plane clipper
    /// to trim triangles straddling the near plane.
    double eyeRelativeDepth(const Vector3d& worldPoint) const override;

    /**
      * @returns the distance between the eye and the viewplane. Defaults to 5.
      */
    inline double distance() const {
      return m_distance;
    }

    /**
      * @returns the perspective projection matrix that maps eye-space points
      *   (translated so the eye is at the origin) into clip space. The matrix
      *   is built from `Matrix4::frustum` using the current view-plane
      *   dimensions and `distance` as the near-plane distance.
      *
      *   `x/w` and `y/w` of the result are normalized viewport coordinates
      *   in `[-1, 1]`; `w` is the eye-relative depth. `z` maps the depth
      *   range `[distance, far]` to NDC `[-1, +1]`.
      *
      *   Callers that need the raw-depth clip convention used by the
      *   software rasterizer should use `projectPointToClipSpace` instead.
      */
    Matrix4d projectionMatrix() const;

    /**
      * Sets the distance between the eye and the viewplane. A lower distance
      * results in a higher field of view.
      * 
      * <table><tr>
      * <td>@image html pinhole_camera_cube_distance_1.png "distance=1"</td>
      * <td>@image html pinhole_camera_cube_distance_2.png "distance=2"</td>
      * <td>@image html pinhole_camera_cube_distance_3.png "distance=3"</td>
      * <td>@image html pinhole_camera_cube_distance_4.png "distance=4"</td>
      * <td>@image html pinhole_camera_cube_distance_5.png "distance=5"</td>
      * </tr></table>
      */
    inline void setDistance(double distance) {
      m_distance = distance;
    }

    /**
      * @returns the zoom of the camera. Defaults to 1.
      */
    inline double zoom() const {
      return m_zoom;
    }

    /**
      * Sets the zoom of the camera.
      * 
      * <table><tr>
      * <td>@image html pinhole_camera_cube_zoom_1.0.png "zoom=1"</td>
      * <td>@image html pinhole_camera_cube_zoom_1.25.png "zoom=1.25"</td>
      * <td>@image html pinhole_camera_cube_zoom_1.5.png "zoom=1.5"</td>
      * <td>@image html pinhole_camera_cube_zoom_1.75.png "zoom=1.75"</td>
      * <td>@image html pinhole_camera_cube_zoom_2.0.png "zoom=2"</td>
      * </tr></table>
      */
    inline void setZoom(double zoom) {
      m_zoom = zoom;
      viewPlane()->setPixelSize(1.0 / m_zoom);
    }

    void setViewPlane(std::shared_ptr<render::ViewPlane> plane) override;

  private:
    double m_distance;
    double m_zoom;
  };
}
