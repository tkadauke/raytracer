#pragma once

#include "world/objects/Camera.h"
#include "core/math/Angle.h"

/**
  * Spherical cameras produce a spherical projection of the scene onto the
  * view plane.
  * 
  * @image html spherical_camera_cube.png "Spherical camera with horizontalFieldOfView=180° and verticalFieldOfView=120°"
  */
class SphericalCamera : public Camera {
  Q_OBJECT;
  Q_PROPERTY(Angled horizontalFieldOfView READ horizontalFieldOfView WRITE setHorizontalFieldOfView);
  Q_PROPERTY(Angled verticalFieldOfView READ verticalFieldOfView WRITE setVerticalFieldOfView);
  
public:
  /**
    * Constructs a default spherical camera with a horizontal field of view of
    * 180 degrees and a vertical field of view if 120 degrees, looking at the
    * origin.
    */
  explicit SphericalCamera(Element* parent = nullptr);
  
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
  
  virtual std::shared_ptr<raytracer::Camera> toRaytracer() const;
  
private:
  Angled m_horizontalFieldOfView;
  Angled m_verticalFieldOfView;
};
