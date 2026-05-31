#include "engine/raster/OpenGLOffscreenContext.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QThread>

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
    std::unique_ptr<QOffscreenSurface> surface;
    std::unique_ptr<QOpenGLContext> context;
    int samples{0};
    std::string errorMessage;

    ~Private() {
      destroyResources();
    }

    bool create(int requestedSamples) {
      errorMessage.clear();
      const int normalizedSamples = requestedSamples > 1 ? requestedSamples : 0;

      if (context && surface && surface->isValid() && samples == normalizedSamples) {
        return true;
      }

      if (!qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
        errorMessage =
          "OpenGL raster backend is selected, but this process was started with "
          "QCoreApplication; Qt offscreen OpenGL context creation requires a QGuiApplication";
        return false;
      }

#if defined(Q_OS_MACOS)
      if (QGuiApplication::platformName() == QStringLiteral("cocoa") &&
          QCoreApplication::applicationName() != QStringLiteral("Modeler") &&
          !qEnvironmentVariableIsSet("RAYTRACER_ALLOW_RENDERCLI_COCOA_OPENGL")) {
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
      if (normalizedSamples > 0) {
        requested.setSamples(normalizedSamples);
      }

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

      samples = normalizedSamples;
      return true;
    }

    bool migrateToCurrentThread() {
      if (!context) {
        return true;
      }
      QThread* current = QThread::currentThread();
      if (context->thread() == current) {
        return true;
      }
      // Qt only allows `moveToThread` from the object's current thread,
      // unless the object is detached (thread() == nullptr). The shared
      // cache leaves the context detached after each render so any
      // subsequent render thread can claim it.
      if (context->thread() != nullptr) {
        errorMessage = "OpenGL raster backend cannot migrate the offscreen context — owner thread "
                       "did not detach";
        return false;
      }
      context->moveToThread(current);
      if (surface) {
        surface->moveToThread(current);
      }
      return true;
    }

    void detachFromCurrentThread() {
      if (context) {
        context->moveToThread(nullptr);
      }
      if (surface) {
        surface->moveToThread(nullptr);
      }
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

    void destroyResources() {
      surface.reset();
      context.reset();
      samples = 0;
    }

    bool isValid() const {
      return context && surface && surface->isValid();
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
    if (!context.create()) {
      return OpenGLAvailability::unavailable(context.errorMessage());
    }
    return OpenGLAvailability::available(context.detailText());
  }

  bool OpenGLOffscreenContext::create(int samples) {
    return p->create(std::max(1, samples));
  }

  bool OpenGLOffscreenContext::migrateToCurrentThread() {
    return p->migrateToCurrentThread();
  }

  void OpenGLOffscreenContext::detachFromCurrentThread() {
    p->detachFromCurrentThread();
  }

  bool OpenGLOffscreenContext::makeCurrent() {
    return p->makeCurrent();
  }

  void OpenGLOffscreenContext::doneCurrent() {
    p->doneCurrent();
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
