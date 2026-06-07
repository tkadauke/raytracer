#include "engine/raster/detail/OpenGLRasterResource.h"

#include "core/Buffer.h"
#include "core/Color.h"
#include "engine/raster/gl/Context.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace engine::raster::detail {
  namespace {
    const char* toString(OpenGLRasterResource::HandleKind kind) {
      switch (kind) {
      case OpenGLRasterResource::HandleKind::Texture:
        return "texture";
      case OpenGLRasterResource::HandleKind::Renderbuffer:
        return "renderbuffer";
      }
      return "unknown";
    }

    double linearDepthFromGlDepth(float depth) {
      if (!std::isfinite(depth) || depth >= 1.0f) {
        return std::numeric_limits<double>::infinity();
      }
      const double clamped = std::clamp(static_cast<double>(depth), 0.0, 1.0);
      if (clamped >= 1.0) {
        return std::numeric_limits<double>::infinity();
      }
      return clamped / (1.0 - clamped);
    }

    void requireType(engine::graph::RenderResourceType actual,
                     engine::graph::RenderResourceType expected) {
      if (actual != expected) {
        throw std::out_of_range(std::string("OpenGL resident resource is ") +
                                engine::graph::toString(actual) + ", expected " +
                                engine::graph::toString(expected));
      }
    }
  }

  OpenGLRasterResource::OpenGLRasterResource(engine::graph::RenderResourceType resourceType,
                                             HandleKind handleKind, GLuint handle, int width,
                                             int height, int sampleCount,
                                             std::shared_ptr<gl::Context> sourceContext)
      : m_resourceType(resourceType),
        m_handleKind(handleKind),
        m_handle(handle),
        m_width(width),
        m_height(height),
        m_sampleCount(sampleCount),
        m_sourceContext(std::move(sourceContext)) {
  }

  OpenGLRasterResource::~OpenGLRasterResource() {
    release();
  }

  OpenGLRasterResource::OpenGLRasterResource(OpenGLRasterResource&& other) noexcept {
    moveFrom(std::move(other));
  }

  OpenGLRasterResource& OpenGLRasterResource::operator=(OpenGLRasterResource&& other) noexcept {
    if (this != &other) {
      release();
      moveFrom(std::move(other));
    }
    return *this;
  }

  std::string OpenGLRasterResource::description() const {
    std::ostringstream out;
    out << "OpenGL " << toString(m_handleKind) << " " << m_handle << " for "
        << engine::graph::toString(m_resourceType) << " " << m_width << "x" << m_height
        << " samples=" << m_sampleCount << " context=" << sourceContextIdentity();
    return out.str();
  }

  void OpenGLRasterResource::copyTo(::Buffer<Colord>& target) const {
    requireType(m_resourceType, engine::graph::RenderResourceType::Color);
    if (!m_sourceContext || !m_sourceContext->migrateToCurrentThread() ||
        !m_sourceContext->makeCurrent()) {
      throw std::runtime_error("OpenGL color resident readback failed: missing source context");
    }

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_handle);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers(1, &fbo);
      m_sourceContext->doneCurrent();
      throw std::runtime_error("OpenGL color resident readback FBO incomplete");
    }

    GLuint readFbo = fbo;
    GLuint resolvedFbo = 0;
    GLuint resolvedRenderbuffer = 0;
    if (m_sampleCount > 1) {
      glGenFramebuffers(1, &resolvedFbo);
      glGenRenderbuffers(1, &resolvedRenderbuffer);
      glBindRenderbuffer(GL_RENDERBUFFER, resolvedRenderbuffer);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, m_width, m_height);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFbo);
      glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                resolvedRenderbuffer);
      if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &resolvedFbo);
        glDeleteRenderbuffers(1, &resolvedRenderbuffer);
        glDeleteFramebuffers(1, &fbo);
        m_sourceContext->doneCurrent();
        throw std::runtime_error("OpenGL color resident resolve FBO incomplete");
      }
      glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFbo);
      glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT,
                        GL_NEAREST);
      readFbo = resolvedFbo;
    }

    const int width = std::min(target.width(), m_width);
    const int height = std::min(target.height(), m_height);
    glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    std::vector<GLfloat> pixels(static_cast<std::size_t>(width * height * 4), 0.0f);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels.data());
    for (int y = 0; y != height; ++y) {
      const int sourceY = height - 1 - y;
      for (int x = 0; x != width; ++x) {
        const auto offset = static_cast<std::size_t>((sourceY * width + x) * 4);
        target[y][x] = Colord(std::clamp(static_cast<double>(pixels[offset]), 0.0, 1.0),
                              std::clamp(static_cast<double>(pixels[offset + 1]), 0.0, 1.0),
                              std::clamp(static_cast<double>(pixels[offset + 2]), 0.0, 1.0));
      }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (resolvedFbo) {
      glDeleteFramebuffers(1, &resolvedFbo);
      glDeleteRenderbuffers(1, &resolvedRenderbuffer);
    }
    glDeleteFramebuffers(1, &fbo);
    m_sourceContext->doneCurrent();
  }

  void OpenGLRasterResource::copyTo(::Buffer<double>& target) const {
    requireType(m_resourceType, engine::graph::RenderResourceType::Depth);
    if (m_sampleCount > 1) {
      throw std::runtime_error("OpenGL depth resident readback does not support multisample "
                               "renderbuffers yet");
    }
    if (!m_sourceContext || !m_sourceContext->migrateToCurrentThread() ||
        !m_sourceContext->makeCurrent()) {
      throw std::runtime_error("OpenGL depth resident readback failed: missing source context");
    }

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_handle);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_handle);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers(1, &fbo);
      m_sourceContext->doneCurrent();
      throw std::runtime_error("OpenGL depth resident readback FBO incomplete");
    }

    const int width = std::min(target.width(), m_width);
    const int height = std::min(target.height(), m_height);
    std::vector<GLfloat> pixels(static_cast<std::size_t>(width * height), 1.0f);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());
    for (int y = 0; y != height; ++y) {
      const int sourceY = height - 1 - y;
      for (int x = 0; x != width; ++x) {
        target[y][x] =
          linearDepthFromGlDepth(pixels[static_cast<std::size_t>(sourceY * width + x)]);
      }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    m_sourceContext->doneCurrent();
  }

  void OpenGLRasterResource::copyTo(::Buffer<std::uint8_t>& target) const {
    requireType(m_resourceType, engine::graph::RenderResourceType::Stencil);
    if (m_sampleCount > 1) {
      throw std::runtime_error("OpenGL stencil resident readback does not support multisample "
                               "renderbuffers yet");
    }
    if (!m_sourceContext || !m_sourceContext->migrateToCurrentThread() ||
        !m_sourceContext->makeCurrent()) {
      throw std::runtime_error("OpenGL stencil resident readback failed: missing source context");
    }

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_handle);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_handle);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDeleteFramebuffers(1, &fbo);
      m_sourceContext->doneCurrent();
      throw std::runtime_error("OpenGL stencil resident readback FBO incomplete");
    }

    const int width = std::min(target.width(), m_width);
    const int height = std::min(target.height(), m_height);
    std::vector<GLubyte> pixels(static_cast<std::size_t>(width * height), 0);
    glReadPixels(0, 0, width, height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, pixels.data());
    for (int y = 0; y != height; ++y) {
      const int sourceY = height - 1 - y;
      for (int x = 0; x != width; ++x) {
        target[y][x] = pixels[static_cast<std::size_t>(sourceY * width + x)];
      }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    m_sourceContext->doneCurrent();
  }

  bool OpenGLRasterResource::release() {
    if (m_handle == 0) {
      return true;
    }

    if (!m_sourceContext) {
      abandonWithDiagnostic("OpenGL resident resource release failed: missing source context");
      return false;
    }

    if (!m_sourceContext->migrateToCurrentThread()) {
      abandonWithDiagnostic("OpenGL resident resource release failed: " +
                            m_sourceContext->errorMessage());
      return false;
    }

    if (!m_sourceContext->makeCurrent()) {
      abandonWithDiagnostic("OpenGL resident resource release failed: " +
                            m_sourceContext->errorMessage());
      return false;
    }

    GLuint handle = m_handle;
    switch (m_handleKind) {
    case HandleKind::Texture:
      glDeleteTextures(1, &handle);
      break;
    case HandleKind::Renderbuffer:
      glDeleteRenderbuffers(1, &handle);
      break;
    }
    m_handle = 0;
    m_releaseDiagnostic.clear();
    m_sourceContext->doneCurrent();
    return true;
  }

  void OpenGLRasterResource::abandonWithDiagnostic(std::string diagnostic) {
    m_releaseDiagnostic = std::move(diagnostic);
    m_handle = 0;
  }

  void OpenGLRasterResource::moveFrom(OpenGLRasterResource&& other) noexcept {
    m_resourceType = other.m_resourceType;
    m_handleKind = other.m_handleKind;
    m_handle = other.m_handle;
    m_width = other.m_width;
    m_height = other.m_height;
    m_sampleCount = other.m_sampleCount;
    m_sourceContext = std::move(other.m_sourceContext);
    m_releaseDiagnostic = std::move(other.m_releaseDiagnostic);
    other.m_handle = 0;
    other.m_width = 0;
    other.m_height = 0;
    other.m_sampleCount = 1;
  }
}
