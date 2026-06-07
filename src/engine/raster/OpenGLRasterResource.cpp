#include "engine/raster/detail/OpenGLRasterResource.h"

#include "engine/raster/gl/Context.h"

#include <sstream>
#include <utility>

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
