#pragma once

#include "engine/raster/gl/Context.h"

#include <memory>
#include <string>

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
    *
    * Implements `engine::raster::gl::Context` so downstream callers can
    * gradually migrate to the abstract base while the Qt-backed
    * implementation lives here. The native backends (CGL, EGL) land
    * later in Phase 2.
    */
  class OpenGLOffscreenContext final : public gl::Context {
  public:
    OpenGLOffscreenContext();
    ~OpenGLOffscreenContext() override;

    OpenGLOffscreenContext(const OpenGLOffscreenContext&) = delete;
    OpenGLOffscreenContext& operator=(const OpenGLOffscreenContext&) = delete;

    static OpenGLAvailability probe();

    bool create(int samples = 1) override;

    bool migrateToCurrentThread() override;
    void detachFromCurrentThread() override;

    bool makeCurrent() override;
    void doneCurrent() override;
    bool isValid() const override;
    const std::string& errorMessage() const override;
    std::string detailText() const override;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
