#pragma once

#include "core/Color.h"

#include <cstdint>
#include <memory>
#include <string>

template<class T>
class Buffer;

namespace engine::raster::gl {
  /**
    * Result of probing the host's OpenGL offscreen rendering support.
    * Mirrors `engine::raster::OpenGLAvailability`; pulled into the
    * `gl` namespace as part of the Qt → native-GL decoupling so it
    * can return platform-native context details (CGL, EGL, …) without
    * importing Qt-specific types into its identifier.
    */
  class Availability {
  public:
    static Availability available(std::string detail) {
      return Availability(true, std::move(detail), {});
    }
    static Availability unavailable(std::string error) {
      return Availability(false, {}, std::move(error));
    }

    bool available() const {
      return m_available;
    }
    const std::string& detail() const {
      return m_detail;
    }
    const std::string& error() const {
      return m_error;
    }

  private:
    Availability(bool available, std::string detail, std::string error)
        : m_available(available),
          m_detail(std::move(detail)),
          m_error(std::move(error)) {
    }

    bool m_available{false};
    std::string m_detail;
    std::string m_error;
  };

  /**
    * Abstract offscreen OpenGL context + framebuffer-owner interface.
    *
    * Concrete implementations:
    *   `gl::QtContext`    — wraps Qt's QOpenGLContext/QOffscreenSurface/
    *                        QOpenGLFramebufferObject; the only backend
    *                        today.
    *   `gl::CGLContext`   — macOS native (Phase 2 follow-up).
    *   `gl::EGLContext`   — Linux/headless via Mesa EGL (Phase 2 follow-up).
    *
    * The interface mirrors the surface `OpenGLOffscreenContext` used to
    * publish directly. The Qt-decoupling rollout migrates callers to
    * the abstract type one site at a time; `OpenGLOffscreenContext`
    * itself becomes a thin facade over an owned `gl::Context` pointer.
    *
    * Threading semantics match Qt's per-thread context model:
    * `makeCurrent` ties the context to the calling thread until
    * `doneCurrent`; `detachFromCurrentThread` releases the affinity so
    * another thread can `migrateToCurrentThread` and adopt it. The
    * shared-cache pattern in `OpenGLRasterizer` relies on this so the
    * GL context can outlive any one render-worker thread.
    */
  class Context {
  public:
    virtual ~Context() = default;

    /**
      * Allocate the underlying GL context and an FBO sized for the
      * given pixels and MSAA sample count. Subsequent calls with the
      * same dimensions and sample count reuse the existing FBO;
      * differing dimensions trigger a fresh allocation. Calling with
      * `samples <= 1` selects single-sample.
      *
      * @returns true on success. On failure, `errorMessage()` carries
      * an actionable diagnostic and `isValid()` returns false.
      */
    virtual bool create(int width, int height, int samples) = 0;

    /// Convenience: `create(width, height, 1)`.
    bool create(int width, int height) {
      return create(width, height, 1);
    }

    /// True iff the context was created and the framebuffer is bound.
    virtual bool isValid() const = 0;

    /// Move the context (and its surface and FBO) onto the calling
    /// thread. Returns false if the context still belongs to another
    /// thread that didn't detach — caller should drop this context and
    /// allocate a fresh one.
    virtual bool migrateToCurrentThread() = 0;

    /// Release thread affinity so the next render's worker thread can
    /// pick the context up. Must be called from the context's current
    /// thread immediately after `doneCurrent`.
    virtual void detachFromCurrentThread() = 0;

    /// Make the context current on the calling thread; pair with
    /// `doneCurrent`. Returns false if binding fails (no surface, wrong
    /// thread, etc.).
    virtual bool makeCurrent() = 0;

    /// Release the current-binding without changing thread affinity.
    virtual void doneCurrent() = 0;

    /// Bind the FBO so subsequent GL calls render to it. Requires the
    /// context to be current. Returns false if no FBO was allocated.
    virtual bool bindFramebuffer() = 0;

    /// Unbind the FBO; pair with `bindFramebuffer`.
    virtual void releaseFramebuffer() = 0;

    // `::Buffer<T>` is the global pixel-buffer template (core/Buffer.h);
    // qualify it so the unqualified name doesn't resolve to
    // `engine::raster::gl::Buffer` (the VBO/IBO wrapper) when the
    // resource cache pulls both headers in.

    /// Read the FBO's color attachment into `target`, flipping rows
    /// so the buffer's row-0 is the visible top.
    virtual void copyColorTo(::Buffer<Colord>& target) const = 0;

    /// Read the FBO's depth attachment into `target` as normalized
    /// [0, 1] doubles, with the same row-flip as `copyColorTo`.
    virtual void copyDepthTo(::Buffer<double>& target) const = 0;

    /// Read the FBO's stencil attachment as raw bytes, row-flipped.
    virtual void copyStencilTo(::Buffer<std::uint8_t>& target) const = 0;

    /// Last operation's error message, set when `create`, `migrate*`,
    /// `makeCurrent`, or `bindFramebuffer` return false. Empty when
    /// the most recent operation succeeded.
    virtual const std::string& errorMessage() const = 0;

    /// Human-readable description of the active backend (GL version,
    /// profile, vendor). Empty before `create` or after teardown.
    virtual std::string detailText() const = 0;
  };
}
