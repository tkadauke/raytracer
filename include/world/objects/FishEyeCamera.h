#pragma once

#include "world/objects/Camera.h"
#include "core/math/Angle.h"

/**
  * Fish eye cameras produce a hemispherical projection of the scene onto the
  * view plane.
  * 
  * @image html fish_eye_camera_cube.png "Fish eye with fieldOfView=180°"
  */
class FishEyeCamera : public Camera {
  Q_OBJECT;
  Q_PROPERTY(Angled fieldOfView READ fieldOfView WRITE setFieldOfView);
  
public:
  /**
    * Creates a default fish eye camera with a 120 degree field of view,
    * looking at the origin.
    */
  FishEyeCamera(Element* parent = nullptr);
  
  /**
    * @returns the camera's field of view.
    */
  inline const Angled& fieldOfView() const {
    return m_fieldOfView;
  }
  
  /**
    * Sets the field of view of the camera.
    * 
    * <table><tr>
    * <td>@image html fish_eye_camera_cube_fov_90.png "fieldOfView=90"</td>
    * <td>@image html fish_eye_camera_cube_fov_150.png "fieldOfView=150"</td>
    * <td>@image html fish_eye_camera_cube_fov_210.png "fieldOfView=210"</td>
    * <td>@image html fish_eye_camera_cube_fov_270.png "fieldOfView=270"</td>
    * <td>@image html fish_eye_camera_cube_fov_330.png "fieldOfView=330"</td>
    * </tr></table>
    */
  inline void setFieldOfView(const Angled& fieldOfView) {
    m_fieldOfView = fieldOfView;
  }
  
  virtual std::shared_ptr<raytracer::Camera> toRaytracer() const;
  
private:
  Angled m_fieldOfView;
};
