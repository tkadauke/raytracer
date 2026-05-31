#include "engine/raster/gl/createContext.h"

#include "engine/raster/OpenGLOffscreenContext.h"

#include <QCoreApplication>
#include <QGuiApplication>

#if defined(__APPLE__)
#include "engine/raster/gl/CGLContext.h"
#elif defined(__linux__)
#include "engine/raster/gl/EglContext.h"
#endif

namespace engine::raster::gl {
  std::unique_ptr<Context> createOffscreenContext() {
    // QOpenGL* classes require a QGuiApplication for their internal
    // platform integration. When the host process only spun up a
    // QCoreApplication (rendercli) — or no Qt app at all — fall back
    // to the native backend, which talks to OpenGL directly with no
    // Qt dependency: CGL on macOS, EGL (surfaceless) on Linux.
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr) {
#if defined(__APPLE__)
      return std::make_unique<CGLContext>();
#elif defined(__linux__)
      return std::make_unique<EglContext>();
#endif
    }
    return std::make_unique<OpenGLOffscreenContext>();
  }
}
