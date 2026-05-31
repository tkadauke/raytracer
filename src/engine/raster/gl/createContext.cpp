#include "engine/raster/gl/createContext.h"

#include "engine/raster/OpenGLOffscreenContext.h"

#if defined(__APPLE__)
#include "engine/raster/gl/CGLContext.h"

#include <QCoreApplication>
#include <QGuiApplication>
#endif

namespace engine::raster::gl {
  std::unique_ptr<Context> createOffscreenContext() {
    // Today the OpenGL raster path still uses `QOpenGLBuffer`,
    // `QOpenGLShaderProgram`, and `QOpenGLContext::currentContext()`
    // downstream of the context type, so any caller needs a
    // QGuiApplication regardless of which backend lives here. The
    // factory therefore always returns the Qt-backed
    // OpenGLOffscreenContext for now. Once the buffer/shader/
    // framebuffer wrappers in opengl-gpu-hardening.md Phase 2 land,
    // this factory selects CGL/EGL when no QGuiApplication is
    // running — the CGLContext class exists and is tested
    // standalone; only the rest of the rasterizer needs to catch
    // up.
    return std::make_unique<OpenGLOffscreenContext>();
  }
}
