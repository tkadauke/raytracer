#if defined(__linux__)

#include "engine/raster/gl/EglContext.h"

#include "engine/raster/gl/Bindings.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <cstring>
#include <sstream>

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
    std::string errorMessage;

    ~Private() {
      destroyResources();
    }

    void destroyResources() {
      if (context != EGL_NO_CONTEXT && display != EGL_NO_DISPLAY) {
        if (eglGetCurrentContext() == context) {
          eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
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
      return true;
    }

    bool create(int /*samples*/) {
      errorMessage.clear();
      if (context != EGL_NO_CONTEXT) {
        return true;
      }
      return initializeDisplay() && chooseConfig() && createContext();
    }
  };

  EglContext::EglContext()
      : p(std::make_unique<Private>()) {
  }

  EglContext::~EglContext() = default;

  Availability EglContext::probe() {
    Private probeContext;
    if (!probeContext.create(0)) {
      return Availability::unavailable(probeContext.errorMessage);
    }
    return Availability::available("OpenGL (EGL surfaceless) on Linux");
  }

  bool EglContext::create(int samples) {
    return p->create(samples);
  }

  bool EglContext::isValid() const {
    return p->context != EGL_NO_CONTEXT;
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
