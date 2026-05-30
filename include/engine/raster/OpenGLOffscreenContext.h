#pragma once

#include "core/Color.h"

#include <cstdint>
#include <memory>
#include <string>

template<class T>
class Buffer;

namespace engine::raster {
  /**
    * Result of probing Qt/OpenGL offscreen rendering support.
    */
  class OpenGLAvailability {
  public:
    static OpenGLAvailability available(std::string detail);
    static OpenGLAvailability unavailable(std::string error);

    bool available() const;
    const std::string& detail() const;
    const std::string& error() const;

  private:
    OpenGLAvailability(bool available, std::string detail, std::string error);

    bool m_available{false};
    std::string m_detail;
    std::string m_error;
  };

  /**
    * Owns a Qt offscreen OpenGL context, surface, and framebuffer object.
    */
  class OpenGLOffscreenContext {
  public:
    OpenGLOffscreenContext();
    ~OpenGLOffscreenContext();

    OpenGLOffscreenContext(const OpenGLOffscreenContext&) = delete;
    OpenGLOffscreenContext& operator=(const OpenGLOffscreenContext&) = delete;

    static OpenGLAvailability probe();

    bool create(int width, int height);
    bool create(int width, int height, int samples);

    /**
      * Migrates the underlying `QOpenGLContext` / `QOffscreenSurface` /
      * `QOpenGLFramebufferObject` to the current thread before
      * `makeCurrent()` is called. This is the migration path used by the
      * shared `OpenGLRasterizer` resource cache, where the cache outlives
      * any one render thread while the GL context is tied to a specific
      * thread at any moment.
      *
      * Returns true if the context now belongs to the current thread (or
      * already did), false if migration was not possible (the original
      * thread has exited and Qt refuses the move). On false, callers
      * should fall through to recreating the context.
      */
    bool migrateToCurrentThread();

    bool makeCurrent();
    void doneCurrent();
    bool bindFramebuffer();
    void releaseFramebuffer();
    void copyColorTo(Buffer<Colord>& target) const;
    void copyDepthTo(Buffer<double>& target) const;
    void copyStencilTo(Buffer<std::uint8_t>& target) const;
    bool isValid() const;
    const std::string& errorMessage() const;
    std::string detailText() const;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
