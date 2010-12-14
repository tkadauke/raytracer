#ifndef VIEW_PLANE_FACTORY
#define VIEW_PLANE_FACTORY

#include "Factory.h"
#include "Singleton.h"
#include "ViewPlane.h"

typedef Singleton<Factory<ViewPlane> > ViewPlaneFactory;

#endif
