#ifndef FISH_EYE_CAMERA_H
#define FISH_EYE_CAMERA_H

#include "raytracer/cameras/Camera.h"

namespace raytracer {
  class ViewPlane;

  class FishEyeCamera : public Camera {
  public:
    FishEyeCamera()
      : m_fieldOfView(120) {}
    FishEyeCamera(float fieldOfView)
      : m_fieldOfView(fieldOfView) {}
    FishEyeCamera(const Vector3d& position, const Vector3d& target)
      : Camera(position, target), m_fieldOfView(120) {}

    using Camera::render;
    virtual void render(Raytracer* raytracer, Buffer<unsigned int>& buffer, const Rect& rect);
    virtual Ray rayForPixel(int x, int y);

    inline void setFieldOfView(float fieldOfView) { m_fieldOfView = fieldOfView; }
    inline float fieldOfView() const { return m_fieldOfView; }

  private:
    Vector3d direction(const ViewPlane& plane, int x, int y);

    float m_fieldOfView;
  };
}

#endif
