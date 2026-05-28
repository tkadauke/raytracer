#include "engine/raster/OpenGLOffscreenContext.h"

#include "core/Buffer.h"
#include "core/Color.h"

#include <QColor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

namespace engine::raster {
  OpenGLAvailability::OpenGLAvailability(bool available, std::string detail, std::string error)
      : m_available(available),
        m_detail(std::move(detail)),
        m_error(std::move(error)) {
  }

  OpenGLAvailability OpenGLAvailability::available(std::string detail) {
    return OpenGLAvailability(true, std::move(detail), {});
  }

  OpenGLAvailability OpenGLAvailability::unavailable(std::string error) {
    return OpenGLAvailability(false, {}, std::move(error));
  }

  bool OpenGLAvailability::available() const {
    return m_available;
  }

  const std::string& OpenGLAvailability::detail() const {
    return m_detail;
  }

  const std::string& OpenGLAvailability::error() const {
    return m_error;
  }

  struct OpenGLOffscreenContext::Private {
    std::unique_ptr<QOpenGLFramebufferObject> framebuffer;
    std::unique_ptr<QOffscreenSurface> surface;
    std::unique_ptr<QOpenGLContext> context;
    std::string errorMessage;

    ~Private() {
      destroyResources();
    }

    bool create(int width, int height) {
      destroyResources();
      errorMessage.clear();

      if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        errorMessage =
          "OpenGL raster backend is selected, but this process was started with "
          "QCoreApplication; Qt offscreen OpenGL context creation requires a QGuiApplication";
        return false;
      }

#if defined(Q_OS_MACOS)
      if (QGuiApplication::platformName() == QStringLiteral("cocoa") &&
          QCoreApplication::applicationName() != QStringLiteral("Modeler")) {
        errorMessage =
          "OpenGL raster backend is selected, but Qt Cocoa offscreen OpenGL context creation is "
          "only enabled inside Modeler because headless Cocoa context probes can crash in Qt";
        return false;
      }
#endif

      QSurfaceFormat requested;
      requested.setRenderableType(QSurfaceFormat::OpenGL);
      requested.setProfile(QSurfaceFormat::CompatibilityProfile);
      requested.setVersion(2, 1);
      requested.setDepthBufferSize(24);
      requested.setStencilBufferSize(8);

      context = std::make_unique<QOpenGLContext>();
      context->setFormat(requested);
      if (!context->create()) {
        errorMessage =
          "OpenGL raster backend is selected, but Qt could not create an OpenGL context";
        context.reset();
        return false;
      }

      surface = std::make_unique<QOffscreenSurface>();
      surface->setFormat(context->format());
      surface->create();
      if (!surface->isValid()) {
        errorMessage =
          "OpenGL raster backend is selected, but Qt could not create an offscreen surface";
        surface.reset();
        context.reset();
        return false;
      }

      if (!context->makeCurrent(surface.get())) {
        errorMessage =
          "OpenGL raster backend is selected, but the offscreen context could not be made current";
        surface.reset();
        context.reset();
        return false;
      }

      QOpenGLFramebufferObjectFormat framebufferFormat;
      framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
      framebuffer = std::make_unique<QOpenGLFramebufferObject>(
        std::max(1, width), std::max(1, height), framebufferFormat);
      if (!framebuffer->isValid()) {
        errorMessage =
          "OpenGL raster backend is selected, but Qt could not create an offscreen framebuffer";
        framebuffer.reset();
        context->doneCurrent();
        surface.reset();
        context.reset();
        return false;
      }

      context->doneCurrent();
      return true;
    }

    bool makeCurrent() {
      if (!context || !surface || !surface->isValid()) {
        errorMessage =
          "OpenGL raster backend is selected, but no valid offscreen context is available";
        return false;
      }
      if (!context->makeCurrent(surface.get())) {
        errorMessage =
          "OpenGL raster backend is selected, but the offscreen context could not be made current";
        return false;
      }
      return true;
    }

    void doneCurrent() {
      if (context) {
        context->doneCurrent();
      }
    }

    bool bindFramebuffer() {
      if (!framebuffer || !framebuffer->isValid()) {
        errorMessage =
          "OpenGL raster backend is selected, but no valid offscreen framebuffer is available";
        return false;
      }
      if (!framebuffer->bind()) {
        errorMessage =
          "OpenGL raster backend is selected, but the offscreen framebuffer could not be bound";
        return false;
      }
      return true;
    }

    void releaseFramebuffer() {
      if (framebuffer) {
        framebuffer->release();
      }
    }

    void copyColorTo(Buffer<Colord>& target) const {
      if (!framebuffer) {
        return;
      }

      const QImage image = framebuffer->toImage();
      const int width = std::min(target.width(), image.width());
      const int height = std::min(target.height(), image.height());
      for (int y = 0; y != height; ++y) {
        for (int x = 0; x != width; ++x) {
          const QColor color = image.pixelColor(x, y);
          target[y][x] = Colord(color.redF(), color.greenF(), color.blueF());
        }
      }
    }

    void copyDepthTo(Buffer<double>& target) const {
      if (!framebuffer || target.width() <= 0 || target.height() <= 0) {
        return;
      }

      const int width = std::min(target.width(), framebuffer->width());
      const int height = std::min(target.height(), framebuffer->height());
      std::vector<GLfloat> pixels(static_cast<std::size_t>(width * height), 1.0f);
      QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
      functions->glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());

      for (int y = 0; y != height; ++y) {
        const int sourceY = height - 1 - y;
        for (int x = 0; x != width; ++x) {
          target[y][x] =
            linearDepthFromOpenGLDepth(pixels[static_cast<std::size_t>(sourceY * width + x)]);
        }
      }
    }

    double linearDepthFromOpenGLDepth(float depth) const {
      if (!std::isfinite(depth) || depth >= 1.0f) {
        return std::numeric_limits<double>::infinity();
      }
      const double clampedDepth = std::clamp(static_cast<double>(depth), 0.0, 1.0);
      if (clampedDepth >= 1.0) {
        return std::numeric_limits<double>::infinity();
      }
      return clampedDepth / (1.0 - clampedDepth);
    }

    void destroyResources() {
      if (framebuffer) {
        const bool alreadyCurrent = QOpenGLContext::currentContext() == context.get();
        const bool madeCurrent = alreadyCurrent || (context && surface && surface->isValid() &&
                                                    context->makeCurrent(surface.get()));
        framebuffer.reset();
        if (madeCurrent && !alreadyCurrent) {
          context->doneCurrent();
        }
      }

      surface.reset();
      context.reset();
    }

    bool isValid() const {
      return context && surface && surface->isValid() && framebuffer && framebuffer->isValid();
    }

    std::string detailText() const {
      if (!context) {
        return {};
      }

      const QSurfaceFormat format = context->format();
      std::ostringstream out;
      out << "OpenGL " << format.majorVersion() << "." << format.minorVersion();
      if (format.profile() == QSurfaceFormat::CoreProfile) {
        out << " core";
      } else if (format.profile() == QSurfaceFormat::CompatibilityProfile) {
        out << " compatibility";
      }
      return out.str();
    }
  };

  OpenGLOffscreenContext::OpenGLOffscreenContext()
      : p(std::make_unique<Private>()) {
  }

  OpenGLOffscreenContext::~OpenGLOffscreenContext() = default;

  OpenGLAvailability OpenGLOffscreenContext::probe() {
    OpenGLOffscreenContext context;
    if (!context.create(1, 1)) {
      return OpenGLAvailability::unavailable(context.errorMessage());
    }
    return OpenGLAvailability::available(context.detailText());
  }

  bool OpenGLOffscreenContext::create(int width, int height) {
    return p->create(width, height);
  }

  bool OpenGLOffscreenContext::makeCurrent() {
    return p->makeCurrent();
  }

  void OpenGLOffscreenContext::doneCurrent() {
    p->doneCurrent();
  }

  bool OpenGLOffscreenContext::bindFramebuffer() {
    return p->bindFramebuffer();
  }

  void OpenGLOffscreenContext::releaseFramebuffer() {
    p->releaseFramebuffer();
  }

  void OpenGLOffscreenContext::copyColorTo(Buffer<Colord>& target) const {
    p->copyColorTo(target);
  }

  void OpenGLOffscreenContext::copyDepthTo(Buffer<double>& target) const {
    p->copyDepthTo(target);
  }

  bool OpenGLOffscreenContext::isValid() const {
    return p->isValid();
  }

  const std::string& OpenGLOffscreenContext::errorMessage() const {
    return p->errorMessage;
  }

  std::string OpenGLOffscreenContext::detailText() const {
    return p->detailText();
  }
}
