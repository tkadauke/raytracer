#ifndef CAMERA_PARAMETER_WIDGET_FACTORY_H
#define CAMERA_PARAMETER_WIDGET_FACTORY_H

#include "core/Factory.h"
#include "core/Singleton.h"
#include "widgets/CameraParameterWidget.h"

typedef Singleton<Factory<CameraParameterWidget> > CameraParameterWidgetFactory;

#endif
