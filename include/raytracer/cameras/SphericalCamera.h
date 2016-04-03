#pragma once

#include "raytracer/cameras/Camera.h"

namespace raytracer {
  class ViewPlane;

  /**
    * Spherical cameras produce a spherical projection of the scene onto the
    * view plane.
    * 
    * @image html spherical_camera_cube.png "Spherical camera with horizontalFieldOfView=180° and verticalFieldOfView=120°"
    */
  class SphericalCamera : public Camera {
  public:
    /**
      * Constructs a default spherical camera with a horizontal field of view of
      * 180 degrees and a vertical field of view if 120 degrees, looking at the
      * origin.
      */
    inline SphericalCamera()
      : m_horizontalFieldOfView(180_degrees),
        m_verticalFieldOfView(120_degrees)
    {
    }
    
    /**
      * Constructs a spherical camera with the specified horizontalFieldOfView
      * and verticalFieldOfView, looking at the origin.
      */
    inline explicit SphericalCamera(const Angled& horizontalFieldOfView, const Angled& verticalFieldOfView)
      : m_horizontalFieldOfView(horizontalFieldOfView),
        m_verticalFieldOfView(verticalFieldOfView)
    {
    }
    
    /**
      * Constructs a spherical camera with a horizontal field of view of 180
      * degrees and a vertical field of view if 120 degrees, at position and
      * looking at target.
      */
    inline explicit SphericalCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target),
        m_horizontalFieldOfView(180_degrees),
        m_verticalFieldOfView(120_degrees)
    {
    }
    
    virtual Rayd rayForPixel(double x, double y) const;

    /**
      * @returns the horizontal field of view of the camera.
      */
    inline const Angled& horizontalFieldOfView() const {
      return m_horizontalFieldOfView;
    }
  
    /**
      * Sets the horizontal field of view of the camera.
      * 
      * <table><tr>
      * <td>@image html spherical_camera_cube_horz_fov_90.png "horizontalFieldOfView=90"</td>
      * <td>@image html spherical_camera_cube_horz_fov_150.png "horizontalFieldOfView=150"</td>
      * <td>@image html spherical_camera_cube_horz_fov_210.png "horizontalFieldOfView=210"</td>
      * <td>@image html spherical_camera_cube_horz_fov_270.png "horizontalFieldOfView=270"</td>
      * <td>@image html spherical_camera_cube_horz_fov_330.png "horizontalFieldOfView=330"</td>
      * </tr></table>
      */
    inline void setHorizontalFieldOfView(Angled fov) {
      m_horizontalFieldOfView = fov;
    }
    
    /**
      * @returns the vertical field of view of the camera.
      */
    inline const Angled& verticalFieldOfView() const {
      return m_verticalFieldOfView;
    }
    
    /**
      * Sets the vertical field of view of the camera.
      * 
      * <table><tr>
      * <td>@image html spherical_camera_cube_vert_fov_30.png "verticalFieldOfView=30"</td>
      * <td>@image html spherical_camera_cube_vert_fov_60.png "verticalFieldOfView=60"</td>
      * <td>@image html spherical_camera_cube_vert_fov_90.png "verticalFieldOfView=90"</td>
      * <td>@image html spherical_camera_cube_vert_fov_120.png "verticalFieldOfView=120"</td>
      * <td>@image html spherical_camera_cube_vert_fov_150.png "verticalFieldOfView=150"</td>
      * </tr></table>
      */
    inline void setVerticalFieldOfView(Angled fov) {
      m_verticalFieldOfView = fov;
    }
    
    /**
      * Sets both the horizontal and vertical field of view.
      */
    inline void setFieldOfView(const Angled& horizontal, const Angled& vertical) {
      setHorizontalFieldOfView(horizontal);
      setVerticalFieldOfView(vertical);
    }

  private:
    Vector3d direction(double x, double y) const;

    Angled m_horizontalFieldOfView;
    Angled m_verticalFieldOfView;
  };
}
