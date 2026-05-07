#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "render/primitives/Scene.h"

typedef Singleton<Factory<render::Scene>> SceneFactory;
