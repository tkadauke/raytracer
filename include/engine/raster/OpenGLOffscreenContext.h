#pragma once

#include "core/Color.h"

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
    bool makeCurrent();
    void doneCurrent();
    bool bindFramebuffer();
    void releaseFramebuffer();
    void copyColorTo(Buffer<Colord>& target) const;
    void copyDepthTo(Buffer<double>& target) const;
    bool isValid() const;
    const std::string& errorMessage() const;
    std::string detailText() const;

  private:
    struct Private;
    std::unique_ptr<Private> p;
  };
}
