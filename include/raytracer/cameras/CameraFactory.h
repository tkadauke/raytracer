#ifndef CAMERA_FACTORY_H
#define CAMERA_FACTORY_H

#include "core/Factory.h"
#include "core/Singleton.h"
#include "raytracer/cameras/Camera.h"

namespace raytracer {
  typedef Singleton<Factory<Camera> > CameraFactory;
}

#endif
