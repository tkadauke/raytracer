#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  typedef Singleton<Factory<ViewPlane>> ViewPlaneFactory;
}
