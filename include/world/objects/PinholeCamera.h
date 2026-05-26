#pragma once
#include <memory>

#include "core/math/BoundingBox.h"
#include "world/objects/Camera.h"

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
  Q_OBJECT
  Q_PROPERTY(double distance READ distance WRITE setDistance)
  Q_PROPERTY(double zoom READ zoom WRITE setZoom)

public:
  /**
    * Constructs a default pinhole camera with a zoom factor of 1 and an
    * eye-viewplane distance of 5, looking at the origin
    */
  explicit PinholeCamera(Element* parent = nullptr);

  /**
    * @returns the distance between the eye and the viewplane. Defaults to 5.
    */
  inline double distance() const {
    return m_distance;
  }

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
    if (zoom <= 0) {
      m_zoom = 1;
    } else {
      m_zoom = zoom;
    }
  }

  /**
    * Repositions the camera so @p bounds fit in the image from the current
    * viewing direction. Degenerate directions fall back to a three-quarter
    * view suitable for imported models.
    */
  [[nodiscard]] bool frame(const BoundingBoxd& bounds);

  /**
    * Repositions the camera so @p bounds fit in the image from
    * @p targetToEyeDirection.
    */
  [[nodiscard]] bool frameFrom(const BoundingBoxd& bounds, const Vector3d& targetToEyeDirection);

  virtual std::shared_ptr<render::Camera> toRaytracer() const;

private:
  double m_distance;
  double m_zoom;
};
