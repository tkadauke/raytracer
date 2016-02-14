#pragma once

#include "core/math/Angle.h"
#include "raytracer/cameras/Camera.h"

namespace raytracer {
  class ViewPlane;

  class FishEyeCamera : public Camera {
  public:
    FishEyeCamera()
      : m_fieldOfView(Angled::fromDegrees(120)) {}
    FishEyeCamera(const Angled& fieldOfView)
      : m_fieldOfView(fieldOfView) {}
    FishEyeCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target), m_fieldOfView(Angled::fromDegrees(120)) {}

    using Camera::render;
    virtual void render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect);
    virtual Ray rayForPixel(int x, int y);

    inline void setFieldOfView(const Angled& fieldOfView) { m_fieldOfView = fieldOfView; }
    inline Angled fieldOfView() const { return m_fieldOfView; }

  private:
    Vector3d direction(const ViewPlane& plane, int x, int y);

    Angled m_fieldOfView;
  };
}
