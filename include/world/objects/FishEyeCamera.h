#pragma once

#include "world/objects/Camera.h"
#include "core/math/Angle.h"

class FishEyeCamera : public Camera {
  Q_OBJECT
  Q_PROPERTY(Angled fieldOfView READ fieldOfView WRITE setFieldOfView);
  
public:
  FishEyeCamera(Element* parent = nullptr);
  
  inline const Angled& fieldOfView() const { return m_fieldOfView; }
  inline void setFieldOfView(const Angled& fieldOfView) { m_fieldOfView = fieldOfView; }

  virtual std::shared_ptr<raytracer::Camera> toRaytracer() const;
  
private:
  Angled m_fieldOfView;
};
