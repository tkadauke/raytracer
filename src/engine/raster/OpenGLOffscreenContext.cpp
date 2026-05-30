#include "engine/raster/OpenGLOffscreenContext.h"

#include "core/Buffer.h"
#include "core/Color.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#include <QRect>
#include <QSurfaceFormat>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
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
    int framebufferWidth{0};
    int framebufferHeight{0};
    int framebufferSamples{0};
    std::string errorMessage;

    ~Private() {
      destroyResources();
    }

    bool ensureContext() {
      errorMessage.clear();

      if (context && surface && surface->isValid()) {
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

      return true;
    }

    bool ensureFramebuffer(int width, int height, int samples) {
      if (!ensureContext()) {
        return false;
      }

      const int normalizedSamples = samples > 1 ? samples : 0;
      const int targetWidth = std::max(1, width);
      const int targetHeight = std::max(1, height);
      if (framebuffer && framebuffer->isValid() && framebufferWidth == targetWidth &&
          framebufferHeight == targetHeight && framebufferSamples == normalizedSamples) {
        return true;
      }

      if (!context->makeCurrent(surface.get())) {
        errorMessage =
          "OpenGL raster backend is selected, but the offscreen context could not be made current";
        return false;
      }

      framebuffer.reset();
      QOpenGLFramebufferObjectFormat framebufferFormat;
      framebufferFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
      framebufferFormat.setSamples(normalizedSamples);
      framebuffer =
        std::make_unique<QOpenGLFramebufferObject>(targetWidth, targetHeight, framebufferFormat);
      if (!framebuffer->isValid()) {
        errorMessage =
          "OpenGL raster backend is selected, but Qt could not create an offscreen framebuffer";
        framebuffer.reset();
        context->doneCurrent();
        return false;
      }

      framebufferWidth = targetWidth;
      framebufferHeight = targetHeight;
      framebufferSamples = normalizedSamples;
      context->doneCurrent();
      return true;
    }

    bool create(int width, int height, int samples) {
      return ensureContext() && ensureFramebuffer(width, height, samples);
    }

    bool migrateToCurrentThread() {
      // Nothing to migrate — the context will be created on the current
      // thread by ensureContext().
      if (!context) {
        return true;
      }
      QThread* current = QThread::currentThread();
      if (context->thread() == current) {
        return true;
      }
      // Qt only allows `moveToThread` from the object's current thread,
      // unless the object is already detached (thread() == nullptr). The
      // shared-cache pattern leaves the context detached after each
      // render (see `detachFromCurrentThread` below) so any subsequent
      // render thread can claim it; a non-null thread() here means the
      // previous owner exited without detaching, in which case Qt will
      // not let us migrate and the cache has to be rebuilt.
      if (context->thread() != nullptr) {
        errorMessage = "OpenGL raster backend cannot migrate the offscreen context — owner thread "
                       "did not detach";
        return false;
      }
      context->moveToThread(current);
      if (surface) {
        surface->moveToThread(current);
      }
      // The FBO is not a QObject; it lives inside the context and follows
      // it across threads. No explicit move is needed.
      return true;
    }

    void detachFromCurrentThread() {
      // Called after `doneCurrent` so the next render's worker thread
      // can `migrateToCurrentThread` the context across, even though
      // its creation thread has by then exited. Must run on the
      // context's current thread, which is always the render thread
      // immediately after `doneCurrent`.
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
      if (!framebuffer || target.width() <= 0 || target.height() <= 0) {
        return;
      }

      if (framebuffer->format().samples() > 0) {
        copyResolvedColorTo(target);
        return;
      }

      readBoundColorTo(target, *framebuffer);
    }

    void copyResolvedColorTo(Buffer<Colord>& target) const {
      if (!QOpenGLFramebufferObject::hasOpenGLFramebufferBlit()) {
        throw std::runtime_error(
          "OpenGL raster backend cannot read multisample color without framebuffer blit support");
      }

      QOpenGLFramebufferObject resolved(framebuffer->width(), framebuffer->height());
      if (!resolved.isValid()) {
        throw std::runtime_error(
          "OpenGL raster backend could not create a color resolve framebuffer");
      }

      const QRect rect(0, 0, framebuffer->width(), framebuffer->height());
      QOpenGLFramebufferObject::blitFramebuffer(&resolved, rect, framebuffer.get(), rect,
                                                GL_COLOR_BUFFER_BIT, GL_NEAREST);
      resolved.bind();
      readBoundColorTo(target, resolved);
      framebuffer->bind();
    }

    void readBoundColorTo(Buffer<Colord>& target, const QOpenGLFramebufferObject& source) const {
      const int width = std::min(target.width(), source.width());
      const int height = std::min(target.height(), source.height());
      std::vector<GLfloat> pixels(static_cast<std::size_t>(width * height * 4), 0.0f);
      QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
      functions->glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, pixels.data());

      for (int y = 0; y != height; ++y) {
        const int sourceY = height - 1 - y;
        for (int x = 0; x != width; ++x) {
          const auto offset = static_cast<std::size_t>((sourceY * width + x) * 4);
          target[y][x] = Colord(std::clamp(static_cast<double>(pixels[offset]), 0.0, 1.0),
                                std::clamp(static_cast<double>(pixels[offset + 1]), 0.0, 1.0),
                                std::clamp(static_cast<double>(pixels[offset + 2]), 0.0, 1.0));
        }
      }
    }

    void copyDepthTo(Buffer<double>& target) const {
      if (!framebuffer || target.width() <= 0 || target.height() <= 0) {
        return;
      }

      if (framebuffer->format().samples() > 0) {
        copyResolvedDepthTo(target);
        return;
      }

      readBoundDepthTo(target, *framebuffer);
    }

    void copyStencilTo(Buffer<std::uint8_t>& target) const {
      if (!framebuffer || target.width() <= 0 || target.height() <= 0) {
        return;
      }

      if (framebuffer->format().samples() > 0) {
        copyResolvedStencilTo(target);
        return;
      }

      readBoundStencilTo(target, *framebuffer);
    }

    void copyResolvedDepthTo(Buffer<double>& target) const {
      if (!QOpenGLFramebufferObject::hasOpenGLFramebufferBlit()) {
        throw std::runtime_error(
          "OpenGL raster backend cannot read multisample depth without framebuffer blit support");
      }

      QOpenGLFramebufferObjectFormat resolveFormat;
      resolveFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
      QOpenGLFramebufferObject resolved(framebuffer->width(), framebuffer->height(), resolveFormat);
      if (!resolved.isValid()) {
        throw std::runtime_error(
          "OpenGL raster backend could not create a depth resolve framebuffer");
      }

      const QRect rect(0, 0, framebuffer->width(), framebuffer->height());
      QOpenGLFramebufferObject::blitFramebuffer(&resolved, rect, framebuffer.get(), rect,
                                                GL_DEPTH_BUFFER_BIT, GL_NEAREST);
      resolved.bind();
      readBoundDepthTo(target, resolved);
      framebuffer->bind();
    }

    void copyResolvedStencilTo(Buffer<std::uint8_t>& target) const {
      if (!QOpenGLFramebufferObject::hasOpenGLFramebufferBlit()) {
        throw std::runtime_error(
          "OpenGL raster backend cannot read multisample stencil without framebuffer blit support");
      }

      QOpenGLFramebufferObjectFormat resolveFormat;
      resolveFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
      QOpenGLFramebufferObject resolved(framebuffer->width(), framebuffer->height(), resolveFormat);
      if (!resolved.isValid()) {
        throw std::runtime_error(
          "OpenGL raster backend could not create a stencil resolve framebuffer");
      }

      const QRect rect(0, 0, framebuffer->width(), framebuffer->height());
      QOpenGLFramebufferObject::blitFramebuffer(&resolved, rect, framebuffer.get(), rect,
                                                GL_STENCIL_BUFFER_BIT, GL_NEAREST);
      resolved.bind();
      readBoundStencilTo(target, resolved);
      framebuffer->bind();
    }

    void readBoundDepthTo(Buffer<double>& target, const QOpenGLFramebufferObject& source) const {
      const int width = std::min(target.width(), source.width());
      const int height = std::min(target.height(), source.height());
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

    void readBoundStencilTo(Buffer<std::uint8_t>& target,
                            const QOpenGLFramebufferObject& source) const {
      const int width = std::min(target.width(), source.width());
      const int height = std::min(target.height(), source.height());
      std::vector<GLubyte> pixels(static_cast<std::size_t>(width * height), 0);
      QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
      functions->glReadPixels(0, 0, width, height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE,
                              pixels.data());

      for (int y = 0; y != height; ++y) {
        const int sourceY = height - 1 - y;
        for (int x = 0; x != width; ++x) {
          target[y][x] = pixels[static_cast<std::size_t>(sourceY * width + x)];
        }
      }
    }

    void destroyResources() {
      if (framebuffer) {
        const bool alreadyCurrent = QOpenGLContext::currentContext() == context.get();
        const bool canMakeCurrent =
          context && surface && surface->isValid() &&
          (alreadyCurrent || context->thread() == QThread::currentThread() ||
           (context->thread() == nullptr &&
            (context->moveToThread(QThread::currentThread()),
             surface->moveToThread(QThread::currentThread()), true)));
        const bool madeCurrent =
          alreadyCurrent || (canMakeCurrent && context->makeCurrent(surface.get()));
        if (madeCurrent) {
          framebuffer.reset();
          if (!alreadyCurrent) {
            context->doneCurrent();
          }
        } else {
          // Process-exit shutdown path: the context's owning thread has
          // exited without re-attaching here, so `makeCurrent` would
          // qFatal. Leak the GL framebuffer; the OS reclaims it.
          framebuffer.release();
        }
      }

      surface.reset();
      context.reset();
      framebufferWidth = 0;
      framebufferHeight = 0;
      framebufferSamples = 0;
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
    return create(width, height, 1);
  }

  bool OpenGLOffscreenContext::create(int width, int height, int samples) {
    return p->create(width, height, std::max(1, samples));
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

  void OpenGLOffscreenContext::copyStencilTo(Buffer<std::uint8_t>& target) const {
    p->copyStencilTo(target);
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
