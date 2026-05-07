#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "render/tonemap/Tonemap.h"

namespace render {
  typedef Singleton<Factory<Tonemap>> TonemapFactory;
}
