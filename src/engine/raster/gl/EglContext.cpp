#if defined(__linux__)

#include "engine/raster/gl/EglContext.h"

#include "core/Buffer.h"
#include "core/Color.h"
#include "engine/raster/gl/Bindings.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

namespace engine::raster::gl {
  namespace {
    const char* eglErrorString(EGLint code) {
      switch (code) {
      case EGL_SUCCESS:
        return "EGL_SUCCESS";
      case EGL_NOT_INITIALIZED:
        return "EGL_NOT_INITIALIZED";
      case EGL_BAD_ACCESS:
        return "EGL_BAD_ACCESS";
      case EGL_BAD_ALLOC:
        return "EGL_BAD_ALLOC";
      case EGL_BAD_ATTRIBUTE:
        return "EGL_BAD_ATTRIBUTE";
      case EGL_BAD_CONFIG:
        return "EGL_BAD_CONFIG";
      case EGL_BAD_CONTEXT:
        return "EGL_BAD_CONTEXT";
      case EGL_BAD_CURRENT_SURFACE:
        return "EGL_BAD_CURRENT_SURFACE";
      case EGL_BAD_DISPLAY:
        return "EGL_BAD_DISPLAY";
      case EGL_BAD_MATCH:
        return "EGL_BAD_MATCH";
      case EGL_BAD_NATIVE_PIXMAP:
        return "EGL_BAD_NATIVE_PIXMAP";
      case EGL_BAD_NATIVE_WINDOW:
        return "EGL_BAD_NATIVE_WINDOW";
      case EGL_BAD_PARAMETER:
        return "EGL_BAD_PARAMETER";
      case EGL_BAD_SURFACE:
        return "EGL_BAD_SURFACE";
      case EGL_CONTEXT_LOST:
        return "EGL_CONTEXT_LOST";
      default:
        return "EGL_<unknown>";
      }
    }

    std::string formatEglError(const char* prefix) {
      const EGLint code = eglGetError();
      std::ostringstream out;
      out << prefix << ": " << eglErrorString(code);
      return out.str();
    }

    bool clientExtensionSupported(const char* extension) {
      const char* exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
      if (!exts) {
        return false;
      }
      return std::strstr(exts, extension) != nullptr;
    }
  }

  struct EglContext::Private {
    EGLDisplay display{EGL_NO_DISPLAY};
    EGLContext context{EGL_NO_CONTEXT};
    EGLConfig config{nullptr};
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
      const EGLContext previouslyCurrent =
        display != EGL_NO_DISPLAY ? eglGetCurrentContext() : EGL_NO_CONTEXT;
      const bool wasCurrent = (previouslyCurrent != EGL_NO_CONTEXT && previouslyCurrent == context);
      const bool madeCurrent = wasCurrent || (context != EGL_NO_CONTEXT &&
                                              eglMakeCurrent(display, EGL_NO_SURFACE,
                                                             EGL_NO_SURFACE, context) == EGL_TRUE);

      if (madeCurrent) {
        if (fbo)
          glDeleteFramebuffers(1, &fbo);
        if (colorRenderbuffer)
          glDeleteRenderbuffers(1, &colorRenderbuffer);
        if (depthStencilRenderbuffer)
          glDeleteRenderbuffers(1, &depthStencilRenderbuffer);
        if (!wasCurrent && display != EGL_NO_DISPLAY) {
          eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
      }
      fbo = 0;
      colorRenderbuffer = 0;
      depthStencilRenderbuffer = 0;

      if (context != EGL_NO_CONTEXT && display != EGL_NO_DISPLAY) {
        eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
      }
      // Don't eglTerminate(display): the display is a process-wide
      // singleton; tearing it down breaks any sibling EGL user. The
      // process exit reclaims it.
      display = EGL_NO_DISPLAY;
      config = nullptr;
    }

    bool initializeDisplay() {
      if (!clientExtensionSupported("EGL_MESA_platform_surfaceless")) {
        errorMessage = "EGL_MESA_platform_surfaceless not supported by EGL runtime";
        return false;
      }

      auto getPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));
      if (getPlatformDisplay) {
        display = getPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
      } else {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
      }
      if (display == EGL_NO_DISPLAY) {
        errorMessage = formatEglError("eglGetPlatformDisplay (surfaceless) failed");
        return false;
      }

      EGLint major = 0;
      EGLint minor = 0;
      if (eglInitialize(display, &major, &minor) != EGL_TRUE) {
        errorMessage = formatEglError("eglInitialize failed");
        display = EGL_NO_DISPLAY;
        return false;
      }
      return true;
    }

    bool chooseConfig() {
      const EGLint configAttrs[] = {
        EGL_SURFACE_TYPE,
        EGL_PBUFFER_BIT,
        EGL_BLUE_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_RED_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8,
        EGL_DEPTH_SIZE,
        24,
        EGL_STENCIL_SIZE,
        8,
        EGL_RENDERABLE_TYPE,
        EGL_OPENGL_BIT,
        EGL_NONE,
      };
      EGLint configCount = 0;
      if (eglChooseConfig(display, configAttrs, &config, 1, &configCount) != EGL_TRUE ||
          configCount == 0) {
        errorMessage = formatEglError("eglChooseConfig failed");
        return false;
      }
      return true;
    }

    bool createContext() {
      if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) {
        errorMessage = formatEglError("eglBindAPI(OpenGL) failed");
        return false;
      }
      const EGLint contextAttrs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 2, EGL_CONTEXT_MINOR_VERSION, 1, EGL_NONE,
      };
      context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttrs);
      if (context == EGL_NO_CONTEXT) {
        errorMessage = formatEglError("eglCreateContext failed");
        return false;
      }
      // The surfaceless platform accepts EGL_NO_SURFACE for both draw
      // and read targets; the rasterizer never targets a window.
      return true;
    }

    bool create(int requestedWidth, int requestedHeight, int requestedSamples) {
      errorMessage.clear();
      const int targetWidth = std::max(1, requestedWidth);
      const int targetHeight = std::max(1, requestedHeight);
      const int normalizedSamples = requestedSamples > 1 ? requestedSamples : 0;

      if (context != EGL_NO_CONTEXT && width == targetWidth && height == targetHeight &&
          samples == normalizedSamples) {
        return true;
      }

      if (display == EGL_NO_DISPLAY) {
        if (!initializeDisplay() || !chooseConfig() || !createContext()) {
          return false;
        }
      }

      if (eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context) != EGL_TRUE) {
        errorMessage = formatEglError("eglMakeCurrent during FBO allocation failed");
        return false;
      }

      if (fbo)
        glDeleteFramebuffers(1, &fbo);
      if (colorRenderbuffer)
        glDeleteRenderbuffers(1, &colorRenderbuffer);
      if (depthStencilRenderbuffer)
        glDeleteRenderbuffers(1, &depthStencilRenderbuffer);
      fbo = 0;
      colorRenderbuffer = 0;
      depthStencilRenderbuffer = 0;

      glGenFramebuffers(1, &fbo);
      glBindFramebuffer(GL_FRAMEBUFFER, fbo);

      glGenRenderbuffers(1, &colorRenderbuffer);
      glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
      if (normalizedSamples > 0) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, normalizedSamples, GL_RGBA8, targetWidth,
                                         targetHeight);
      } else {
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, targetWidth, targetHeight);
      }
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                colorRenderbuffer);

      glGenRenderbuffers(1, &depthStencilRenderbuffer);
      glBindRenderbuffer(GL_RENDERBUFFER, depthStencilRenderbuffer);
      if (normalizedSamples > 0) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, normalizedSamples, GL_DEPTH24_STENCIL8,
                                         targetWidth, targetHeight);
      } else {
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, targetWidth, targetHeight);
      }
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                depthStencilRenderbuffer);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                depthStencilRenderbuffer);

      const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

      if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::ostringstream out;
        out << "EGL FBO incomplete: status=0x" << std::hex << status;
        errorMessage = out.str();
        return false;
      }

      width = targetWidth;
      height = targetHeight;
      samples = normalizedSamples;
      return true;
    }
  };

  EglContext::EglContext()
      : p(std::make_unique<Private>()) {
  }

  EglContext::~EglContext() = default;

  Availability EglContext::probe() {
    Private probeContext;
    if (!probeContext.create(1, 1, 0)) {
      return Availability::unavailable(probeContext.errorMessage);
    }
    return Availability::available("OpenGL (EGL surfaceless) on Linux");
  }

  bool EglContext::create(int width, int height, int samples) {
    return p->create(width, height, samples);
  }

  bool EglContext::isValid() const {
    return p->context != EGL_NO_CONTEXT && p->fbo != 0;
  }

  bool EglContext::migrateToCurrentThread() {
    return true;
  }

  void EglContext::detachFromCurrentThread() {
    if (p->display != EGL_NO_DISPLAY && eglGetCurrentContext() == p->context) {
      eglMakeCurrent(p->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
  }

  bool EglContext::makeCurrent() {
    if (p->context == EGL_NO_CONTEXT) {
      p->errorMessage = "EglContext::makeCurrent without an allocated context";
      return false;
    }
    if (eglMakeCurrent(p->display, EGL_NO_SURFACE, EGL_NO_SURFACE, p->context) != EGL_TRUE) {
      p->errorMessage = formatEglError("eglMakeCurrent failed");
      return false;
    }
    return true;
  }

  void EglContext::doneCurrent() {
    if (p->display != EGL_NO_DISPLAY && eglGetCurrentContext() == p->context) {
      eglMakeCurrent(p->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
  }

  bool EglContext::bindFramebuffer() {
    if (!p->fbo) {
      p->errorMessage = "EglContext::bindFramebuffer with no allocated FBO";
      return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, p->fbo);
    return true;
  }

  void EglContext::releaseFramebuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void EglContext::copyColorTo(Buffer<Colord>& target) const {
    if (!p->fbo || target.width() <= 0 || target.height() <= 0) {
      return;
    }
    const int width = std::min(target.width(), p->width);
    const int height = std::min(target.height(), p->height);

    GLuint resolvedFbo = 0;
    GLuint resolvedRb = 0;
    if (p->samples > 0) {
      glGenFramebuffers(1, &resolvedFbo);
      glGenRenderbuffers(1, &resolvedRb);
      glBindRenderbuffer(GL_RENDERBUFFER, resolvedRb);
      glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, p->width, p->height);
      glBindRenderbuffer(GL_RENDERBUFFER, 0);
      glBindFramebuffer(GL_FRAMEBUFFER, resolvedFbo);
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, resolvedRb);
      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, p->fbo);
        glDeleteFramebuffers(1, &resolvedFbo);
        glDeleteRenderbuffers(1, &resolvedRb);
        return;
      }
      glBindFramebuffer(GL_READ_FRAMEBUFFER, p->fbo);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolvedFbo);
      glBlitFramebuffer(0, 0, p->width, p->height, 0, 0, p->width, p->height, GL_COLOR_BUFFER_BIT,
                        GL_NEAREST);
      glBindFramebuffer(GL_FRAMEBUFFER, resolvedFbo);
    } else {
      glBindFramebuffer(GL_FRAMEBUFFER, p->fbo);
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
      glBindFramebuffer(GL_FRAMEBUFFER, p->fbo);
      glDeleteFramebuffers(1, &resolvedFbo);
      glDeleteRenderbuffers(1, &resolvedRb);
    }
  }

  void EglContext::copyDepthTo(Buffer<double>& target) const {
    if (!p->fbo || target.width() <= 0 || target.height() <= 0) {
      return;
    }
    const int width = std::min(target.width(), p->width);
    const int height = std::min(target.height(), p->height);

    // Multisample depth/stencil readback isn't supported; the
    // resolve-via-blit path covers color but not depth/stencil
    // portably across drivers. Matches CGLContext's behavior.
    if (p->samples > 0) {
      p->errorMessage = "EglContext::copyDepthTo not yet supported for multisample FBOs";
      return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, p->fbo);
    std::vector<GLfloat> pixels(static_cast<std::size_t>(width * height), 0.0f);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());

    for (int y = 0; y != height; ++y) {
      const int sourceY = height - 1 - y;
      for (int x = 0; x != width; ++x) {
        target[y][x] = static_cast<double>(pixels[sourceY * width + x]);
      }
    }
  }

  void EglContext::copyStencilTo(Buffer<std::uint8_t>& target) const {
    if (!p->fbo || target.width() <= 0 || target.height() <= 0) {
      return;
    }
    const int width = std::min(target.width(), p->width);
    const int height = std::min(target.height(), p->height);

    if (p->samples > 0) {
      p->errorMessage = "EglContext::copyStencilTo not yet supported for multisample FBOs";
      return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, p->fbo);
    std::vector<GLubyte> pixels(static_cast<std::size_t>(width * height), 0);
    glReadPixels(0, 0, width, height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, pixels.data());

    for (int y = 0; y != height; ++y) {
      const int sourceY = height - 1 - y;
      for (int x = 0; x != width; ++x) {
        target[y][x] = pixels[sourceY * width + x];
      }
    }
  }

  const std::string& EglContext::errorMessage() const {
    return p->errorMessage;
  }

  std::string EglContext::detailText() const {
    if (p->context == EGL_NO_CONTEXT) {
      return {};
    }
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    std::ostringstream out;
    out << "EGL OpenGL";
    if (version)
      out << " " << reinterpret_cast<const char*>(version);
    if (renderer)
      out << " (" << reinterpret_cast<const char*>(renderer) << ")";
    return out.str();
  }
}

#endif // __linux__
