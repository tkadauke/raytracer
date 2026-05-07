#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "render/cameras/Camera.h"

namespace render {
  typedef Singleton<Factory<Camera>> CameraFactory;
}
