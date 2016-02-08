#pragma once

#include "core/Factory.h"
#include "core/Singleton.h"
#include "widgets/CameraParameterWidget.h"

typedef Singleton<Factory<CameraParameterWidget>> CameraParameterWidgetFactory;
