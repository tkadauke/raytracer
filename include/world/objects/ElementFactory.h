#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "world/objects/Element.h"

typedef Singleton<Factory<Element>> ElementFactory;
