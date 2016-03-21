#pragma once

#include "raytracer/cameras/Camera.h"

namespace raytracer {
  /**
    * Pinhole cameras produce a perspective projection of the scene onto the
    * view plane.
    * 
    * @image html pinhole_camera_cube.png "Pinhole with distance=5 and zoom=1"
    */
  class PinholeCamera : public Camera {
  public:
    /**
      * Constructs a default pinhole camera with a zoom factor of 1 and an
      * eye-viewplane distance of 5, looking at the origin
      */
    inline PinholeCamera()
      : Camera(),
        m_distance(5),
        m_zoom(1)
    {
    }
    
    /**
      * Constructs a camera at position lookint at target.
      */
    inline explicit PinholeCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target),
        m_distance(5),
        m_zoom(1)
    {
    }
    
    using Camera::render;
    virtual Rayd rayForPixel(double x, double y) const;

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
      m_zoom = zoom;
      viewPlane()->setPixelSize(1.0 / m_zoom);
    }

    virtual void setViewPlane(std::shared_ptr<ViewPlane> plane);
    
  private:
    double m_distance;
    double m_zoom;
  };
}
