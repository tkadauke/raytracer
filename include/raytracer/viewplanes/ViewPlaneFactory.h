#ifndef RAYTRACER_VIEW_PLANE_FACTORY_H
#define RAYTRACER_VIEW_PLANE_FACTORY_H

#include "core/Factory.h"
#include "core/Singleton.h"
#include "raytracer/viewplanes/ViewPlane.h"

namespace raytracer {
  typedef Singleton<Factory<ViewPlane> > ViewPlaneFactory;
}

#endif
