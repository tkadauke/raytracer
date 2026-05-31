#pragma once

#include <string>

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
    * Abstract offscreen OpenGL context interface — context lifecycle
    * only, no framebuffer or attachment ownership.
    *
    * Concrete implementations:
    *   `OpenGLOffscreenContext` — Qt-backed
    *     (`QOpenGLContext`/`QOffscreenSurface`), used by the Modeler.
    *   `gl::CGLContext`         — macOS native via `<OpenGL/CGL.h>`.
    *   `gl::EglContext`         — Linux headless via Mesa surfaceless EGL.
    *
    * The FBO + color/depth/stencil renderbuffers live on
    * `gl::AttachmentSet` instances owned by
    * `OpenGLRasterResourceCache`, not on the context. Multiple
    * attachment sets can coexist (per-pass dimensions, per-AOV
    * sample counts) without forcing the context to reallocate.
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
      * Allocate the underlying GL context. `samples > 1` is a hint
      * for backends whose context pixel format must agree with the
      * MSAA renderbuffer (the legacy Qt path); the native backends
      * (CGL, EGL surfaceless) ignore it because per-FBO multisample
      * renderbuffers are independent of the context pixel format.
      *
      * @returns true on success. On failure, `errorMessage()` carries
      * an actionable diagnostic and `isValid()` returns false.
      */
    virtual bool create(int samples = 1) = 0;

    /// True iff the context was created.
    virtual bool isValid() const = 0;

    /// Move the context onto the calling thread. Returns false if the
    /// context still belongs to another thread that didn't detach —
    /// caller should drop this context and allocate a fresh one.
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

    /// Last operation's error message, set when `create`, `migrate*`,
    /// or `makeCurrent` returns false. Empty when the most recent
    /// operation succeeded.
    virtual const std::string& errorMessage() const = 0;

    /// Human-readable description of the active backend (GL version,
    /// profile, vendor). Empty before `create` or after teardown.
    virtual std::string detailText() const = 0;
  };
}
