#pragma once

#include "world/objects/Camera.h"

class OrthographicCamera : public Camera {
  Q_OBJECT
  
public:
  OrthographicCamera(Element* parent = nullptr);
  
  virtual std::shared_ptr<raytracer::Camera> toRaytracer() const;
};
