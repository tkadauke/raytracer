#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "raytracer/cameras/Camera.h"

namespace raytracer {
  typedef Singleton<Factory<Camera>> CameraFactory;
}
