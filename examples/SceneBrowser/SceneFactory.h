#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "raytracer/primitives/Scene.h"

typedef Singleton<Factory<raytracer::Scene>> SceneFactory;
