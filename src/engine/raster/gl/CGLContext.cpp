#if defined(__APPLE__)

// macOS deprecated all CGL and gl.h entry points in 10.14 with no
// public successor (the recommendation is "switch to Metal"). The
// CGL path is still supported, just noisy under -Werror. Silence the
// warnings — this file is the contained scope where the deprecation
// applies; switching to Metal is tracked separately.
#define GL_SILENCE_DEPRECATION 1

#include "engine/raster/gl/CGLContext.h"

#include "core/Buffer.h"
#include "core/Color.h"

#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include <algorithm>
#include <sstream>
#include <vector>

namespace engine::raster::gl {
  struct CGLContext::Private {
    CGLContextObj context{nullptr};
    CGLPixelFormatObj pixelFormat{nullptr};
    GLuint fbo{0};
    GLuint colorRenderbuffer{0};
    GLuint depthStencilRenderbuffer{0};
    int width{0};
    int height{0};
    int samples{0};
    std::string errorMessage;

    ~Private() {
      destroyResources();
    }

    void destroyResources() {
      // Releasing GL resources requires the context current. If we no
      // longer have one (process-exit shutdown path) leak the GL
      // handles — the OS reclaims them.
      const bool wasCurrent = (CGLGetCurrentContext() == context);
      const bool madeCurrent =
        wasCurrent || (context && CGLSetCurrentContext(context) == kCGLNoError);
      if (madeCurrent) {
        if (fbo)
          glDeleteFramebuffersEXT(1, &fbo);
        if (colorRenderbuffer)
          glDeleteRenderbuffersEXT(1, &colorRenderbuffer);
        if (depthStencilRenderbuffer)
          glDeleteRenderbuffersEXT(1, &depthStencilRenderbuffer);
        if (!wasCurrent)
          CGLSetCurrentContext(nullptr);
      }
      fbo = 0;
      colorRenderbuffer = 0;
      depthStencilRenderbuffer = 0;

      if (context) {
        CGLDestroyContext(context);
        context = nullptr;
      }
      if (pixelFormat) {
        CGLDestroyPixelFormat(pixelFormat);
        pixelFormat = nullptr;
      }
    }

    bool create(int requestedWidth, int requestedHeight, int requestedSamples) {
      errorMessage.clear();
      const int targetWidth = std::max(1, requestedWidth);
      const int targetHeight = std::max(1, requestedHeight);
      const int normalizedSamples = requestedSamples > 1 ? requestedSamples : 0;

      if (context && width == targetWidth && height == targetHeight &&
          samples == normalizedSamples) {
        return true;
      }

      if (!context) {
        std::vector<CGLPixelFormatAttribute> attrs;
        attrs.push_back(kCGLPFAAccelerated);
        attrs.push_back(kCGLPFAColorSize);
        attrs.push_back(static_cast<CGLPixelFormatAttribute>(24));
        attrs.push_back(kCGLPFAAlphaSize);
        attrs.push_back(static_cast<CGLPixelFormatAttribute>(8));
        attrs.push_back(kCGLPFADepthSize);
        attrs.push_back(static_cast<CGLPixelFormatAttribute>(24));
        attrs.push_back(kCGLPFAStencilSize);
        attrs.push_back(static_cast<CGLPixelFormatAttribute>(8));
        if (normalizedSamples > 0) {
          attrs.push_back(kCGLPFASampleBuffers);
          attrs.push_back(static_cast<CGLPixelFormatAttribute>(1));
          attrs.push_back(kCGLPFASamples);
          attrs.push_back(static_cast<CGLPixelFormatAttribute>(normalizedSamples));
        }
        attrs.push_back(static_cast<CGLPixelFormatAttribute>(0)); // terminator

        GLint pixelFormatCount = 0;
        const CGLError pfErr = CGLChoosePixelFormat(attrs.data(), &pixelFormat, &pixelFormatCount);
        if (pfErr != kCGLNoError || pixelFormat == nullptr) {
          errorMessage = formatCGLError("CGLChoosePixelFormat failed", pfErr);
          return false;
        }

        const CGLError ctxErr = CGLCreateContext(pixelFormat, nullptr, &context);
        if (ctxErr != kCGLNoError || context == nullptr) {
          errorMessage = formatCGLError("CGLCreateContext failed", ctxErr);
          CGLDestroyPixelFormat(pixelFormat);
          pixelFormat = nullptr;
          return false;
        }
      }

      // Allocate the FBO under the context. CGLSetCurrentContext on
      // this thread; tear down anything stale; create renderbuffers
      // sized for (width, height, samples); attach to a fresh FBO.
      const CGLContextObj previouslyCurrent = CGLGetCurrentContext();
      if (CGLSetCurrentContext(context) != kCGLNoError) {
        errorMessage = "CGLSetCurrentContext failed during FBO allocation";
        return false;
      }

      if (fbo)
        glDeleteFramebuffersEXT(1, &fbo);
      if (colorRenderbuffer)
        glDeleteRenderbuffersEXT(1, &colorRenderbuffer);
      if (depthStencilRenderbuffer)
        glDeleteRenderbuffersEXT(1, &depthStencilRenderbuffer);
      fbo = 0;
      colorRenderbuffer = 0;
      depthStencilRenderbuffer = 0;

      glGenFramebuffersEXT(1, &fbo);
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo);

      glGenRenderbuffersEXT(1, &colorRenderbuffer);
      glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, colorRenderbuffer);
      if (normalizedSamples > 0) {
        glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, normalizedSamples, GL_RGBA8,
                                            targetWidth, targetHeight);
      } else {
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, targetWidth, targetHeight);
      }
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                   GL_RENDERBUFFER_EXT, colorRenderbuffer);

      glGenRenderbuffersEXT(1, &depthStencilRenderbuffer);
      glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, depthStencilRenderbuffer);
      if (normalizedSamples > 0) {
        glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, normalizedSamples,
                                            GL_DEPTH24_STENCIL8, targetWidth, targetHeight);
      } else {
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8, targetWidth,
                                 targetHeight);
      }
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT,
                                   depthStencilRenderbuffer);
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT,
                                   GL_RENDERBUFFER_EXT, depthStencilRenderbuffer);

      const GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
      glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
      CGLSetCurrentContext(previouslyCurrent);

      if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
        std::ostringstream out;
        out << "CGL FBO incomplete: status=0x" << std::hex << status;
        errorMessage = out.str();
        return false;
      }

      width = targetWidth;
      height = targetHeight;
      samples = normalizedSamples;
      return true;
    }

    static std::string formatCGLError(const char* prefix, CGLError err) {
      std::ostringstream out;
      out << prefix << ": " << CGLErrorString(err);
      return out.str();
    }
  };

  CGLContext::CGLContext()
      : p(std::make_unique<Private>()) {
  }

  CGLContext::~CGLContext() = default;

  Availability CGLContext::probe() {
    // No process-wide state to check — CGL is available on every
    // macOS host as long as the OpenGL framework is linked. We
    // attempt a tiny throwaway allocation to confirm; failure is
    // caught and reported.
    Private probeContext;
    if (!probeContext.create(1, 1, 0)) {
      return Availability::unavailable(probeContext.errorMessage);
    }
    return Availability::available("OpenGL (CGL) on macOS");
  }

  bool CGLContext::create(int width, int height, int samples) {
    return p->create(width, height, samples);
  }

  bool CGLContext::isValid() const {
    return p->context != nullptr && p->fbo != 0;
  }

  bool CGLContext::migrateToCurrentThread() {
    // CGL contexts have no thread affinity; the per-thread current-
    // binding handles all of this. Nothing to migrate.
    return true;
  }

  void CGLContext::detachFromCurrentThread() {
    if (CGLGetCurrentContext() == p->context) {
      CGLSetCurrentContext(nullptr);
    }
  }

  bool CGLContext::makeCurrent() {
    if (!p->context) {
      p->errorMessage = "CGLContext::makeCurrent without an allocated context";
      return false;
    }
    const CGLError err = CGLSetCurrentContext(p->context);
    if (err != kCGLNoError) {
      p->errorMessage = Private::formatCGLError("CGLSetCurrentContext failed", err);
      return false;
    }
    return true;
  }

  void CGLContext::doneCurrent() {
    if (CGLGetCurrentContext() == p->context) {
      CGLSetCurrentContext(nullptr);
    }
  }

  bool CGLContext::bindFramebuffer() {
    if (!p->fbo) {
      p->errorMessage = "CGLContext::bindFramebuffer with no allocated FBO";
      return false;
    }
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, p->fbo);
    return true;
  }

  void CGLContext::releaseFramebuffer() {
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
  }

  namespace {
    // Resolve a multisample FBO into a single-sample read-target FBO
    // so glReadPixels can read it. Returns the resolved FBO id and the
    // renderbuffer id; the caller deletes both when done.
    bool resolveColorIntoReadFbo(int width, int height, GLuint sourceFbo, GLuint& outFbo,
                                 GLuint& outRenderbuffer) {
      glGenFramebuffersEXT(1, &outFbo);
      glGenRenderbuffersEXT(1, &outRenderbuffer);
      glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, outRenderbuffer);
      glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_RGBA8, width, height);
      glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, outFbo);
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT,
                                   GL_RENDERBUFFER_EXT, outRenderbuffer);
      if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, sourceFbo);
        glDeleteFramebuffersEXT(1, &outFbo);
        glDeleteRenderbuffersEXT(1, &outRenderbuffer);
        return false;
      }
      glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, sourceFbo);
      glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, outFbo);
      glBlitFramebufferEXT(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, outFbo);
      return true;
    }
  }

  void CGLContext::copyColorTo(Buffer<Colord>& target) const {
    if (!p->fbo || target.width() <= 0 || target.height() <= 0) {
      return;
    }
    const int width = std::min(target.width(), p->width);
    const int height = std::min(target.height(), p->height);

    GLuint resolvedFbo = 0;
    GLuint resolvedRb = 0;
    if (p->samples > 0) {
      if (!resolveColorIntoReadFbo(p->width, p->height, p->fbo, resolvedFbo, resolvedRb)) {
        return;
      }
    } else {
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, p->fbo);
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
      glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, p->fbo);
      glDeleteFramebuffersEXT(1, &resolvedFbo);
      glDeleteRenderbuffersEXT(1, &resolvedRb);
    }
  }

  void CGLContext::copyDepthTo(Buffer<double>& target) const {
    if (!p->fbo || target.width() <= 0 || target.height() <= 0) {
      return;
    }
    const int width = std::min(target.width(), p->width);
    const int height = std::min(target.height(), p->height);

    // Multisample depth/stencil readback isn't implemented yet — the
    // EXT_framebuffer_multisample blit path can resolve color but
    // not depth/stencil portably. The Qt backend handles this with
    // QOpenGLFramebufferObject::blitFramebuffer; CGLContext leaves
    // depth/stencil readback unsupported when samples > 0 and emits
    // a clear error message on first use.
    if (p->samples > 0) {
      p->errorMessage = "CGLContext::copyDepthTo not yet supported for multisample FBOs";
      return;
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, p->fbo);
    std::vector<GLfloat> pixels(static_cast<std::size_t>(width * height), 0.0f);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());

    for (int y = 0; y != height; ++y) {
      const int sourceY = height - 1 - y;
      for (int x = 0; x != width; ++x) {
        target[y][x] = static_cast<double>(pixels[sourceY * width + x]);
      }
    }
  }

  void CGLContext::copyStencilTo(Buffer<std::uint8_t>& target) const {
    if (!p->fbo || target.width() <= 0 || target.height() <= 0) {
      return;
    }
    const int width = std::min(target.width(), p->width);
    const int height = std::min(target.height(), p->height);

    if (p->samples > 0) {
      p->errorMessage = "CGLContext::copyStencilTo not yet supported for multisample FBOs";
      return;
    }

    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, p->fbo);
    std::vector<GLubyte> pixels(static_cast<std::size_t>(width * height), 0);
    glReadPixels(0, 0, width, height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, pixels.data());

    for (int y = 0; y != height; ++y) {
      const int sourceY = height - 1 - y;
      for (int x = 0; x != width; ++x) {
        target[y][x] = pixels[sourceY * width + x];
      }
    }
  }

  const std::string& CGLContext::errorMessage() const {
    return p->errorMessage;
  }

  std::string CGLContext::detailText() const {
    if (!p->context) {
      return {};
    }
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    std::ostringstream out;
    out << "CGL OpenGL";
    if (version)
      out << " " << reinterpret_cast<const char*>(version);
    if (renderer)
      out << " (" << reinterpret_cast<const char*>(renderer) << ")";
    return out.str();
  }
}

#endif // __APPLE__
