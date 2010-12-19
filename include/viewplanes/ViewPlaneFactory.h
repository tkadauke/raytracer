#ifndef VIEW_PLANE_FACTORY
#define VIEW_PLANE_FACTORY

#include "core/Factory.h"
#include "core/Singleton.h"
#include "viewplanes/ViewPlane.h"

typedef Singleton<Factory<ViewPlane> > ViewPlaneFactory;

#endif
