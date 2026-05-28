#pragma once

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
    */
  class OpenGLOffscreenContext {
  public:
    OpenGLOffscreenContext();
    ~OpenGLOffscreenContext();

    OpenGLOffscreenContext(const OpenGLOffscreenContext&) = delete;
    OpenGLOffscreenContext& operator=(const OpenGLOffscreenContext&) = delete;

    static OpenGLAvailability probe();

    bool create(int width, int height);
    bool isValid() const;
    const std::string& errorMessage() const;
    std::string detailText() const;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
