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
    // QOpenGL* classes require a QGuiApplication for their internal
    // platform integration. When the host process only spun up a
    // QCoreApplication (rendercli) — or no Qt app at all — fall back
    // to the native CGL backend, which talks to Apple's OpenGL
    // framework directly with no Qt dependency.
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()) == nullptr) {
      return std::make_unique<CGLContext>();
    }
#endif
    return std::make_unique<OpenGLOffscreenContext>();
  }
}
