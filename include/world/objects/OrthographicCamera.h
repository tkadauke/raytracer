#pragma once
#include <memory>

#include "world/objects/Camera.h"

/**
  * An orthographic camera projects the scene orthographically onto the view
  * plane.
  * 
  * <table><tr>
  * <td>@image html orthographic_camera_cube__raytracer.png "Raytracer"</td>
  * <td>@image html orthographic_camera_cube__raster.png "Rasterizer"</td>
  * <td>@image html orthographic_camera_cube__wireframe.png "Wireframe"</td>
  * </tr></table>
  */
class OrthographicCamera : public Camera {
  Q_OBJECT
  Q_PROPERTY(double zoom READ zoom WRITE setZoom)

public:
  /**
    * Constructs a default orthographic camera with a zoom factor of 1,
    * looking at the origin.
    */
  explicit OrthographicCamera(Element* parent = nullptr);

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
    if (zoom <= 0) {
      m_zoom = 1;
    } else {
      m_zoom = zoom;
    }
  }

  virtual std::shared_ptr<render::Camera> toRaytracer() const;

private:
  double m_zoom;
};
