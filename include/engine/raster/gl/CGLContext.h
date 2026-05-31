#pragma once

#if defined(__APPLE__)

#include "engine/raster/gl/Context.h"

#include <cstdint>
#include <string>

namespace engine::raster::gl {
  /**
    * Native Cocoa OpenGL backend for offscreen rendering on macOS.
    *
    * Uses `<OpenGL/CGL.h>` directly — no Qt, no `QGuiApplication`, no
    * QSurfaceFormat. The point: rendercli can render through OpenGL
    * without dragging in Qt's GUI subsystem. Modeler still uses
    * `OpenGLOffscreenContext` (the Qt backend) for shared-context
    * integration; `CGLContext` is the native alternative the engine
    * library hands rendercli (and ultimately CI lanes) at build/run
    * time.
    *
    * Thread model is simpler than Qt's: CGL contexts are not bound to
    * a thread; the current-binding is per-thread via
    * `CGLSetCurrentContext`. So `migrateToCurrentThread` is a no-op
    * that just succeeds, and `detachFromCurrentThread` calls
    * `CGLSetCurrentContext(NULL)` on the current thread. The
    * shared-cache pattern in `OpenGLRasterizer` still works without
    * any "moveToThread" dance.
    *
    * macOS Cocoa OpenGL is deprecated as of macOS 10.14 but still
    * functional through macOS 15. Long-term we hedge via ANGLE
    * (OpenGL ES over Metal); this backend is the simple short-term
    * answer.
    */
  class CGLContext final : public Context {
  public:
    CGLContext();
    ~CGLContext() override;

    CGLContext(const CGLContext&) = delete;
    CGLContext& operator=(const CGLContext&) = delete;

    static Availability probe();

    using Context::create;
    bool create(int width, int height, int samples) override;

    bool isValid() const override;
    bool migrateToCurrentThread() override;
    void detachFromCurrentThread() override;
    bool makeCurrent() override;
    void doneCurrent() override;
    bool bindFramebuffer() override;
    void releaseFramebuffer() override;
    void copyColorTo(Buffer<Colord>& target) const override;
    void copyDepthTo(Buffer<double>& target) const override;
    void copyStencilTo(Buffer<std::uint8_t>& target) const override;
    const std::string& errorMessage() const override;
    std::string detailText() const override;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}

#endif // __APPLE__
