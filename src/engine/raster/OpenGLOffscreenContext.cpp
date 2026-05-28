#include "engine/raster/OpenGLOffscreenContext.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QSurfaceFormat>

#include <algorithm>
#include <sstream>
#include <utility>

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
      if (QGuiApplication::platformName() == QStringLiteral("cocoa")) {
        errorMessage =
          "OpenGL raster backend is selected, but Qt Cocoa offscreen OpenGL context creation is "
          "disabled because this backend currently requires a safe offscreen platform probe";
        return false;
      }
#endif

      QSurfaceFormat requested;
      requested.setRenderableType(QSurfaceFormat::OpenGL);
      requested.setProfile(QSurfaceFormat::CoreProfile);
      requested.setVersion(3, 3);
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
