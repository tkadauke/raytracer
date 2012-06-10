#ifndef SCENE_FACTORY_H
#define SCENE_FACTORY_H

#include "core/Factory.h"
#include "core/Singleton.h"
#include "surfaces/Scene.h"

typedef Singleton<Factory<Scene> > SceneFactory;

#endif
