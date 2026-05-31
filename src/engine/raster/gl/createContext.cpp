#include "engine/raster/gl/createContext.h"

#include "engine/raster/OpenGLOffscreenContext.h"

#if defined(__APPLE__)
#include "engine/raster/gl/CGLContext.h"

#include <QCoreApplication>
#include <QGuiApplication>
#endif

namespace engine::raster::gl {
  std::unique_ptr<Context> createOffscreenContext() {
#if defined(__APPLE__)
    // Qt backend when QGuiApplication is up (Modeler), CGL when not
    // (rendercli). The QGuiApplication check exists today inside the
    // Qt backend's ensureContext as a refusal; making the decision
    // up front here is cleaner and lets rendercli drop QGuiApplication
    // entirely.
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
      return std::make_unique<OpenGLOffscreenContext>();
    }
    return std::make_unique<CGLContext>();
#else
    // Linux/headless: Qt backend until EGL lands. Tracked as Phase 2
    // task #41.
    return std::make_unique<OpenGLOffscreenContext>();
#endif
  }
}
