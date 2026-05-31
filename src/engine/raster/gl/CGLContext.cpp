#if defined(__APPLE__)

// macOS deprecated all CGL and gl.h entry points in 10.14 with no
// public successor (the recommendation is "switch to Metal"). The
// CGL path is still supported, just noisy under -Werror. Silence the
// warnings — this file is the contained scope where the deprecation
// applies; switching to Metal is tracked separately.
#define GL_SILENCE_DEPRECATION 1

#include "engine/raster/gl/CGLContext.h"

#include <OpenGL/CGLCurrent.h>
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include <sstream>
#include <vector>

namespace engine::raster::gl {
  struct CGLContext::Private {
    CGLContextObj context{nullptr};
    CGLPixelFormatObj pixelFormat{nullptr};
    int samples{0};
    std::string errorMessage;

    ~Private() {
      destroyResources();
    }

    void destroyResources() {
      if (context) {
        if (CGLGetCurrentContext() == context) {
          CGLSetCurrentContext(nullptr);
        }
        CGLDestroyContext(context);
        context = nullptr;
      }
      if (pixelFormat) {
        CGLDestroyPixelFormat(pixelFormat);
        pixelFormat = nullptr;
      }
    }

    bool create(int requestedSamples) {
      errorMessage.clear();
      const int normalizedSamples = requestedSamples > 1 ? requestedSamples : 0;

      if (context && samples == normalizedSamples) {
        return true;
      }
      destroyResources();

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
    Private probeContext;
    if (!probeContext.create(0)) {
      return Availability::unavailable(probeContext.errorMessage);
    }
    return Availability::available("OpenGL (CGL) on macOS");
  }

  bool CGLContext::create(int samples) {
    return p->create(samples);
  }

  bool CGLContext::isValid() const {
    return p->context != nullptr;
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
