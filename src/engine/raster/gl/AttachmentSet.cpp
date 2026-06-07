#include "engine/raster/gl/AttachmentSet.h"

#include "core/Buffer.h"
#include "core/Color.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/raster/detail/OpenGLRasterResource.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>

namespace engine::raster::gl {
  namespace {
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

    std::shared_ptr<detail::OpenGLRasterResource>
    copyAttachmentToRenderbuffer(GLuint sourceFbo, int width, int height, int samples,
                                 GLenum internalFormat, GLenum attachment,
                                 GLbitfield blitMask,
                                 engine::graph::RenderResourceType resourceType,
                                 std::shared_ptr<Context> sourceContext) {
      if (!sourceFbo || !sourceContext) {
        return nullptr;
      }

      GLuint destinationFbo = 0;
      GLuint destinationRenderbuffer = 0;
      glGenFramebuffers(1, &destinationFbo);
      glGenRenderbuffers(1, &destinationRenderbuffer);
      glBindRenderbuffer(GL_RENDERBUFFER, destinationRenderbuffer);
      if (samples > 0) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internalFormat, width, height);
      } else {
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
      }
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destinationFbo);
      glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, attachment, GL_RENDERBUFFER,
                                destinationRenderbuffer);
      if (internalFormat == GL_DEPTH24_STENCIL8) {
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                  destinationRenderbuffer);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  destinationRenderbuffer);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
      }

      if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, sourceFbo);
        glDeleteFramebuffers(1, &destinationFbo);
        glDeleteRenderbuffers(1, &destinationRenderbuffer);
        return nullptr;
      }

      glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFbo);
      glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, blitMask, GL_NEAREST);
      glBindFramebuffer(GL_FRAMEBUFFER, sourceFbo);
      glDeleteFramebuffers(1, &destinationFbo);

      return std::make_shared<detail::OpenGLRasterResource>(
        resourceType, detail::OpenGLRasterResource::HandleKind::Renderbuffer,
        destinationRenderbuffer, width, height, samples > 1 ? samples : 1, std::move(sourceContext));
    }

    GLenum attachmentPoint(engine::graph::RenderResourceType type) {
      switch (type) {
      case engine::graph::RenderResourceType::Color:
      case engine::graph::RenderResourceType::Normal:
      case engine::graph::RenderResourceType::WorldPosition:
      case engine::graph::RenderResourceType::ShadowMask:
      case engine::graph::RenderResourceType::CustomTexture:
        return GL_COLOR_ATTACHMENT0;
      case engine::graph::RenderResourceType::Depth:
      case engine::graph::RenderResourceType::ShadowMap:
        return GL_DEPTH_ATTACHMENT;
      case engine::graph::RenderResourceType::Stencil:
        return GL_STENCIL_ATTACHMENT;
      default:
        return GL_COLOR_ATTACHMENT0;
      }
    }

    int normalizedSamples(int samples) {
      return samples > 1 ? samples : 0;
    }
  }

  AttachmentSet::~AttachmentSet() {
    destroy();
  }

  void AttachmentSet::destroy() {
    if (m_fbo) {
      glDeleteFramebuffers(1, &m_fbo);
      m_fbo = 0;
    }
    if (m_colorRenderbuffer) {
      glDeleteRenderbuffers(1, &m_colorRenderbuffer);
      m_colorRenderbuffer = 0;
    }
    if (m_depthStencilRenderbuffer) {
      glDeleteRenderbuffers(1, &m_depthStencilRenderbuffer);
      m_depthStencilRenderbuffer = 0;
    }
    m_width = 0;
    m_height = 0;
    m_samples = 0;
  }

  bool AttachmentSet::create(int width, int height, int samples) {
    m_errorMessage.clear();
    const int targetWidth = std::max(1, width);
    const int targetHeight = std::max(1, height);
    const int normalizedSamples = samples > 1 ? samples : 0;

    if (m_fbo != 0 && m_width == targetWidth && m_height == targetHeight &&
        m_samples == normalizedSamples) {
      return true;
    }

    destroy();

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);

    glGenRenderbuffers(1, &m_colorRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_colorRenderbuffer);
    if (normalizedSamples > 0) {
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, normalizedSamples, GL_RGBA8, targetWidth,
                                       targetHeight);
    } else {
      glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, targetWidth, targetHeight);
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              m_colorRenderbuffer);

    glGenRenderbuffers(1, &m_depthStencilRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depthStencilRenderbuffer);
    if (normalizedSamples > 0) {
      glRenderbufferStorageMultisample(GL_RENDERBUFFER, normalizedSamples, GL_DEPTH24_STENCIL8,
                                       targetWidth, targetHeight);
    } else {
      glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, targetWidth, targetHeight);
    }
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              m_depthStencilRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              m_depthStencilRenderbuffer);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
      std::ostringstream out;
      out << "AttachmentSet FBO incomplete: status=0x" << std::hex << status;
      m_errorMessage = out.str();
      destroy();
      return false;
    }

    m_width = targetWidth;
    m_height = targetHeight;
    m_samples = normalizedSamples;
    return true;
  }

  void AttachmentSet::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  }

  void AttachmentSet::release() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void AttachmentSet::loadColorFrom(
    const ::engine::raster::detail::OpenGLRasterResource& source) {
    loadFrom(source, GL_COLOR_BUFFER_BIT);
  }

  void AttachmentSet::loadDepthFrom(
    const ::engine::raster::detail::OpenGLRasterResource& source) {
    loadFrom(source, GL_DEPTH_BUFFER_BIT);
  }

  void AttachmentSet::loadStencilFrom(
    const ::engine::raster::detail::OpenGLRasterResource& source) {
    loadFrom(source, GL_STENCIL_BUFFER_BIT);
  }

  void AttachmentSet::loadFrom(const ::engine::raster::detail::OpenGLRasterResource& source,
                               GLbitfield mask) {
    m_errorMessage.clear();
    if (!m_fbo || !source.valid()) {
      return;
    }
    if (source.width() != m_width || source.height() != m_height ||
        normalizedSamples(source.sampleCount()) != m_samples) {
      m_errorMessage = "AttachmentSet resident load source shape does not match destination";
      return;
    }

    GLuint sourceFbo = 0;
    glGenFramebuffers(1, &sourceFbo);
    GLint drawFbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &drawFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFbo);

    const GLenum sourceAttachment = attachmentPoint(source.resourceType());
    if (source.handleKind() ==
        ::engine::raster::detail::OpenGLRasterResource::HandleKind::Texture) {
      glFramebufferTexture2D(GL_READ_FRAMEBUFFER, sourceAttachment, GL_TEXTURE_2D,
                             source.handle(), 0);
    } else {
      glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, sourceAttachment, GL_RENDERBUFFER,
                                source.handle());
    }

    const GLenum status = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      std::ostringstream out;
      out << "AttachmentSet resident load source FBO incomplete: status=0x" << std::hex << status;
      m_errorMessage = out.str();
      glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
      glDeleteFramebuffers(1, &sourceFbo);
      return;
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_fbo);
    glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, mask, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(drawFbo));
    glDeleteFramebuffers(1, &sourceFbo);
  }

  void AttachmentSet::copyColorTo(::Buffer<Colord>& target) {
    if (!m_fbo || target.width() <= 0 || target.height() <= 0) {
      return;
    }
    const int width = std::min(target.width(), m_width);
    const int height = std::min(target.height(), m_height);

    GLuint resolvedFbo = 0;
    GLuint resolvedRb = 0;
    if (m_samples > 0) {
      glGenFramebuffers(1, &resolvedFbo);
      glGenRenderbuffers(1, &resolvedRb);
      glBindRenderbuffer(GL_RENDERBUFFER, resolvedRb);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, m_width, m_height);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
      glBindFramebuffer(GL_FRAMEBUFFER, resolvedFbo);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, resolvedRb);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glDeleteFramebuffers(1, &resolvedFbo);
        glDeleteRenderbuffers(1, &resolvedRb);
        m_errorMessage = "AttachmentSet color resolve FBO incomplete";
        return;
      }
      glBindFramebuffer(GL_READ_FRAMEBUFFER, m_fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFbo);
      glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT,
                        GL_NEAREST);
      glBindFramebuffer(GL_FRAMEBUFFER, resolvedFbo);
    } else {
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    }

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

    if (resolvedFbo) {
      glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
      glDeleteFramebuffers(1, &resolvedFbo);
      glDeleteRenderbuffers(1, &resolvedRb);
    }
  }

  void AttachmentSet::copyDepthTo(::Buffer<double>& target) {
    if (!m_fbo || target.width() <= 0 || target.height() <= 0) {
      return;
    }
    if (m_samples > 0) {
      m_errorMessage = "AttachmentSet::copyDepthTo not yet supported for multisample FBOs";
      return;
    }
    const int width = std::min(target.width(), m_width);
    const int height = std::min(target.height(), m_height);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    std::vector<GLfloat> pixels(static_cast<std::size_t>(width * height), 1.0f);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());

    for (int y = 0; y != height; ++y) {
      const int sourceY = height - 1 - y;
      for (int x = 0; x != width; ++x) {
        target[y][x] =
          linearDepthFromGlDepth(pixels[static_cast<std::size_t>(sourceY * width + x)]);
      }
    }
  }

  void AttachmentSet::copyStencilTo(::Buffer<std::uint8_t>& target) {
    if (!m_fbo || target.width() <= 0 || target.height() <= 0) {
      return;
    }
    if (m_samples > 0) {
      m_errorMessage = "AttachmentSet::copyStencilTo not yet supported for multisample FBOs";
      return;
    }
    const int width = std::min(target.width(), m_width);
    const int height = std::min(target.height(), m_height);

    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    std::vector<GLubyte> pixels(static_cast<std::size_t>(width * height), 0);
    glReadPixels(0, 0, width, height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, pixels.data());

    for (int y = 0; y != height; ++y) {
      const int sourceY = height - 1 - y;
      for (int x = 0; x != width; ++x) {
        target[y][x] = pixels[static_cast<std::size_t>(sourceY * width + x)];
      }
    }
  }

  std::shared_ptr<detail::OpenGLRasterResource>
  AttachmentSet::copyColorToOpenGLResource(std::shared_ptr<Context> sourceContext) {
    m_errorMessage.clear();
    auto result = copyAttachmentToRenderbuffer(
      m_fbo, m_width, m_height, m_samples, GL_RGBA8, GL_COLOR_ATTACHMENT0, GL_COLOR_BUFFER_BIT,
      engine::graph::RenderResourceType::Color, std::move(sourceContext));
    if (!result) {
      m_errorMessage = "AttachmentSet color resident copy FBO incomplete";
    }
    return result;
  }

  std::shared_ptr<detail::OpenGLRasterResource>
  AttachmentSet::copyDepthToOpenGLResource(std::shared_ptr<Context> sourceContext) {
    m_errorMessage.clear();
    auto result = copyAttachmentToRenderbuffer(
      m_fbo, m_width, m_height, m_samples, GL_DEPTH24_STENCIL8, GL_DEPTH_ATTACHMENT,
      GL_DEPTH_BUFFER_BIT, engine::graph::RenderResourceType::Depth, std::move(sourceContext));
    if (!result) {
      m_errorMessage = "AttachmentSet depth resident copy FBO incomplete";
    }
    return result;
  }

  std::shared_ptr<detail::OpenGLRasterResource>
  AttachmentSet::copyStencilToOpenGLResource(std::shared_ptr<Context> sourceContext) {
    m_errorMessage.clear();
    auto result = copyAttachmentToRenderbuffer(
      m_fbo, m_width, m_height, m_samples, GL_DEPTH24_STENCIL8, GL_STENCIL_ATTACHMENT,
      GL_STENCIL_BUFFER_BIT, engine::graph::RenderResourceType::Stencil, std::move(sourceContext));
    if (!result) {
      m_errorMessage = "AttachmentSet stencil resident copy FBO incomplete";
    }
    return result;
  }
}
