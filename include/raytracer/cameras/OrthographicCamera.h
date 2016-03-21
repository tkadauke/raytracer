#pragma once

#include "raytracer/cameras/Camera.h"

namespace raytracer {
  /**
    * An orthographic camera projects the scene orthographically onto the view
    * plane.
    * 
    * @image html orthographic_camera_cube.png "Orthographic camera with zoom=1"
    */
  class OrthographicCamera : public Camera {
  public:
    /**
      * Constructs a default orthographic camera with a zoom factor of 1,
      * looking at the origin.
      */
    inline OrthographicCamera()
      : m_zoom(1)
    {
    }
    
    /**
      * Constructs an orthographic camera at position, looking at target with
      * a zoom value of 1.
      */
    inline explicit OrthographicCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target),
        m_zoom(1)
    {
    }

    using Camera::render;
    virtual Rayd rayForPixel(double x, double y) const;

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
    
    virtual void setViewPlane(std::shared_ptr<ViewPlane> plane);
    
  private:
    double m_zoom;
  };
}
