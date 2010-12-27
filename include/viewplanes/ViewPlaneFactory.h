#ifndef VIEW_PLANE_FACTORY_H
#define VIEW_PLANE_FACTORY_H

#include "core/Factory.h"
#include "core/Singleton.h"
#include "viewplanes/ViewPlane.h"

typedef Singleton<Factory<ViewPlane> > ViewPlaneFactory;

#endif
