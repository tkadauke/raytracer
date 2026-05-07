#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "render/viewplanes/ViewPlane.h"

namespace render {
  typedef Singleton<Factory<ViewPlane>> ViewPlaneFactory;
}
