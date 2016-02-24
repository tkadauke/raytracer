#pragma once

#include "raytracer/cameras/Camera.h"

namespace raytracer {
  class ViewPlane;

  class SphericalCamera : public Camera {
  public:
    SphericalCamera()
      : m_horizontalFieldOfView(Angled::fromDegrees(180)), m_verticalFieldOfView(Angled::fromDegrees(120)) {}
    SphericalCamera(const Angled& horizontalFieldOfView, const Angled& verticalFieldOfView)
      : m_horizontalFieldOfView(horizontalFieldOfView), m_verticalFieldOfView(verticalFieldOfView) {}
    SphericalCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target), m_horizontalFieldOfView(Angled::fromDegrees(180)), m_verticalFieldOfView(Angled::fromDegrees(120)) {}

    virtual Ray rayForPixel(double x, double y);

    inline void setHorizontalFieldOfView(Angled fov) { m_horizontalFieldOfView = fov; }
    inline const Angled& horizontalFieldOfView() const { return m_horizontalFieldOfView; }

    inline void setVerticalFieldOfView(Angled fov) { m_verticalFieldOfView = fov; }
    inline const Angled& verticalFieldOfView() const { return m_verticalFieldOfView; }

    inline void setFieldOfView(const Angled& horizontal, const Angled& vertical) {
      setHorizontalFieldOfView(horizontal);
      setVerticalFieldOfView(vertical);
    }

  private:
    Vector3d direction(double x, double y);

    Angled m_horizontalFieldOfView;
    Angled m_verticalFieldOfView;
  };
}
