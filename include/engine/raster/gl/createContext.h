#pragma once

#include "engine/raster/gl/Context.h"

#include <memory>

namespace engine::raster::gl {
  /**
    * Construct the offscreen GL context appropriate for the current
    * host process.
    *
    * - **macOS** (`__APPLE__`): returns a `CGLContext` (native, no
    *   Qt) when no `QGuiApplication` is running; rendercli without
    *   QGuiApplication gets the native path. Returns the Qt-backed
    *   `OpenGLOffscreenContext` when `QGuiApplication` is present so
    *   the Modeler keeps sharing its GL infrastructure with Qt
    *   widgets.
    * - **Linux** and other platforms: returns the Qt-backed
    *   `OpenGLOffscreenContext` for now. An EGL native backend is
    *   tracked under Phase 2 of opengl-gpu-hardening.md and slots
    *   into this factory.
    *
    * Callers that need a specific backend (tests, advanced users)
    * can still construct the concrete classes directly.
    */
  std::unique_ptr<Context> createOffscreenContext();
}
