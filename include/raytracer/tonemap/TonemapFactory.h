#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "raytracer/tonemap/Tonemap.h"

namespace raytracer {
  typedef Singleton<Factory<Tonemap>> TonemapFactory;
}
