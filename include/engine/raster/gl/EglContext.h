#pragma once

#if defined(__linux__)

#include "engine/raster/gl/Context.h"

#include <cstdint>
#include <memory>
#include <string>

namespace engine::raster::gl {
  /**
    * Native EGL backend for offscreen rendering on Linux.
    *
    * Uses Mesa's surfaceless EGL platform (`EGL_PLATFORM_SURFACELESS_MESA`)
    * — no X11 / Wayland surface required, just an EGL display and a
    * pbuffer-equivalent OpenGL context. The point: rendercli can render
    * through OpenGL on a headless Linux host (CI, containers, server
    * deploys) without dragging in `QGuiApplication` or any window
    * system. Modeler still uses `OpenGLOffscreenContext` (the Qt
    * backend) for shared-context integration; `EglContext` is the
    * native alternative the engine library hands rendercli (and CI)
    * at build/run time.
    *
    * Thread model: EGL contexts are not bound to a thread; the
    * current-binding is per-thread via `eglMakeCurrent`. So
    * `migrateToCurrentThread` is a no-op that succeeds, and
    * `detachFromCurrentThread` calls `eglMakeCurrent(display,
    * EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT)`.
    *
    * Requires Mesa with `EGL_MESA_platform_surfaceless` (Mesa 19.0+,
    * shipped by every supported Ubuntu LTS). Falls back to an
    * Availability::unavailable() with a clear message when the
    * extension is missing.
    *
    * Spelled `EglContext` (lower-case) rather than `EGLContext` to
    * avoid colliding with the EGL header typedef
    * (`typedef void* EGLContext` in <EGL/egl.h>) — the same name in
    * the same namespace makes the typedef shadow the class inside
    * the implementation.
    */
  class EglContext final : public Context {
  public:
    EglContext();
    ~EglContext() override;

    EglContext(const EglContext&) = delete;
    EglContext& operator=(const EglContext&) = delete;

    static Availability probe();

    bool create(int samples = 1) override;

    bool isValid() const override;
    bool migrateToCurrentThread() override;
    void detachFromCurrentThread() override;
    bool makeCurrent() override;
    void doneCurrent() override;
    const std::string& errorMessage() const override;
    std::string detailText() const override;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}

#endif // __linux__
