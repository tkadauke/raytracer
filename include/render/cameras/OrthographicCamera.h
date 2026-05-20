#pragma once
#include <memory>

#include "render/cameras/Camera.h"

namespace render {
  /**
    * @brief An orthographic camera projects the scene orthographically
    *        onto the view plane.
    *
    * Same camera and scene through the supported engines — note how the
    * orthographic projection's lack of perspective foreshortening
    * is faithfully preserved:
    *
    * <table>
    *   <tr>
    *     <th>Raytracer</th>
    *     <th>Software rasterizer</th>
    *     <th>Wireframe</th>
    *   </tr>
    *   <tr>
    *     <td>@image html orthographic_camera_cube__raytracer.png ""</td>
    *     <td>@image html orthographic_camera_cube__raster.png ""</td>
    *     <td>@image html orthographic_camera_cube__wireframe.png ""</td>
    *   </tr>
    * </table>
    */
  class OrthographicCamera : public Camera {
  public:
    using Camera::rayForPixel;

    /**
      * Constructs a default orthographic camera with a zoom factor of 1,
      * looking at the origin.
      */
    inline OrthographicCamera()
        : m_zoom(1) {
    }

    /**
      * Constructs an orthographic camera at position, looking at target with
      * a zoom value of 1.
      */
    inline explicit OrthographicCamera(const Vector3d& position, const Vector3d& target)
        : Camera(position, target),
          m_zoom(1) {
    }

    Rayd rayForPixel(double x, double y, render::SampleStream& stream) const override;
    std::shared_ptr<Camera> clone() const override;

    /**
      * Closed-form orthographic inverse of `rayForPixel`. Drops the
      * camera-forward axis component (orthographic = parallel
      * projection) and converts the remaining camera-space x/y to
      * pixel coordinates.
      *
      * Returns `Vector2d::undefined()` if the point is at or behind
      * the camera plane (`z_cam < 0`) — orthographic projection has
      * no perspective divide so doesn't diverge, but a point behind
      * the camera still shouldn't appear.
      */
    Vector2d projectPoint(const Vector3d& worldPoint) const override;

    /**
      * Like `projectPoint` but additionally returns the eye-relative
      * depth in `result.z()` for Z-buffer testing in the software
      * rasterizer. For orthographic projection the depth is the
      * camera-space `z` directly — there's no perspective divide,
      * so `1/z` interpolation also degenerates to plain linear
      * interpolation across the screen.
      */
    Vector3d projectPointWithDepth(const Vector3d& worldPoint) const override;

    /**
      * Homogeneous orthographic projection. `x / w` and `y / w`
      * are normalized viewport coordinates, `z` is camera-space
      * depth, and `w` is always 1 because orthographic projection
      * has no perspective divide.
      *
      * @see Camera::projectPointToClipSpace for an interactive
      *      comparison with pinhole projection.
      */
    Vector4d projectPointToClipSpace(const Vector3d& worldPoint) const override;

    /// Signed eye-relative depth — positive in front of the camera
    /// plane, negative behind. For ortho this is the camera-space z
    /// directly (no perspective eye point).
    double eyeRelativeDepth(const Vector3d& worldPoint) const override;

    /**
      * @returns the orthographic projection matrix that maps camera-space
      *   points into clip space. Built from `Matrix4::orthographic` using the
      *   current view-plane dimensions.
      *
      *   `x` and `y` of the result are normalized viewport coordinates in
      *   `[-1, 1]`; `w` is always 1 (no perspective divide). `z` maps the
      *   camera-space depth range `[0, 1e6]` to NDC `[-1, +1]`.
      *
      *   Callers that need the camera-space-z clip convention used by the
      *   software rasterizer should use `projectPointToClipSpace` instead.
      */
    Matrix4d projectionMatrix() const;

    /**
      * @returns the camera'z zoom.
      */
    inline double zoom() const {
      return m_zoom;
    }

    /**
      * Sets zoom of the camera.
      * 
      * <table><tr>
      * <td>@image html orthographic_camera_cube_zoom_1.0.png "zoom=1"</td>
      * <td>@image html orthographic_camera_cube_zoom_1.25.png "zoom=1.25"</td>
      * <td>@image html orthographic_camera_cube_zoom_1.5.png "zoom=1.5"</td>
      * <td>@image html orthographic_camera_cube_zoom_1.75.png "zoom=1.75"</td>
      * <td>@image html orthographic_camera_cube_zoom_2.0.png "zoom=2"</td>
      * </tr></table>
      */
    inline void setZoom(double zoom) {
      m_zoom = zoom;
      viewPlane()->setPixelSize(1.0 / m_zoom);
    }

    void setViewPlane(std::shared_ptr<render::ViewPlane> plane) override;

  private:
    double m_zoom;
  };
}
