#pragma once

#include "core/Color.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/raster/gl/Bindings.h"

#include <cstdint>
#include <memory>
#include <string>

template<class T>
class Buffer;

namespace engine::raster::detail {
  class OpenGLRasterResource;
}

namespace engine::raster::gl {
  class Context;
}

namespace engine::raster::gl {
  /**
    * Owns one OpenGL FBO plus its color and combined-depth/stencil
    * renderbuffers. The Phase 3 carve-out of attachment ownership out
    * of `gl::Context`: each backend (Qt, CGL, EGL) used to allocate
    * its own FBO when `Context::create` was called, which meant
    * there was exactly one attachment configuration per process.
    * Multi-pass graphs that need different sizes or sample counts had
    * to either reallocate the context FBO every pass (cache thrash)
    * or upcast to the largest configuration and waste pixels.
    *
    * `AttachmentSet` lives in the resource cache instead, keyed by
    * `(width, height, samples)`. Multiple sets coexist; the draw pass
    * picks the right one per render and binds it before issuing
    * draws. This is also the substrate the residency work in
    * `opengl-gpu-residency.md` needs: register an attachment set with
    * the graph storage and Phase 0 residency lifetime falls out
    * naturally.
    *
    * Allocation and readback require a GL context current on the
    * caller's thread; the class does not interact with `gl::Context`
    * directly and is portable across all three context backends.
    *
    * Color readback handles multisample resolve internally via a
    * single-sample resolve FBO blit. Depth and stencil readback
    * matches the prior `OpenGLOffscreenContext` behavior: multisample
    * readback is unsupported because the resolve blit can't preserve
    * per-sample depth/stencil portably; the caller gets a clear
    * error message via `errorMessage()` when it tries.
    */
  class AttachmentSet {
  public:
    AttachmentSet() = default;
    ~AttachmentSet();

    AttachmentSet(const AttachmentSet&) = delete;
    AttachmentSet& operator=(const AttachmentSet&) = delete;

    /**
      * Allocate the FBO + color renderbuffer + combined depth/stencil
      * renderbuffer sized for `(width, height)`. `samples > 1` enables
      * MSAA via `glRenderbufferStorageMultisample`. Subsequent calls
      * with the same parameters return true without reallocating.
      *
      * The owning GL context must be current on the calling thread.
      *
      * @returns true on success. On failure, `errorMessage()` carries
      * an actionable diagnostic and `isValid()` returns false.
      */
    bool create(int width, int height, int samples);

    /**
      * Release the GL objects. The caller's context must be current.
      * The destructor calls this automatically.
      */
    void destroy();

    /**
      * Drop ownership of the GL handles without calling `glDelete`.
      * Used by the resource cache's leak path at process shutdown
      * when no GL context is current to safely free them; the OS
      * reclaims the handles at process exit.
      */
    void abandon() {
      m_fbo = 0;
      m_colorRenderbuffer = 0;
      m_depthStencilRenderbuffer = 0;
      m_width = 0;
      m_height = 0;
      m_samples = 0;
    }

    bool isValid() const {
      return m_fbo != 0;
    }

    int width() const {
      return m_width;
    }
    int height() const {
      return m_height;
    }
    int samples() const {
      return m_samples;
    }

    /// Bind the FBO so subsequent GL calls render into it. Context
    /// must be current.
    void bind() const;

    /// Unbind the FBO (binds the default framebuffer object 0).
    void release() const;

    /// Blit a resident OpenGL color resource into this set's color attachment.
    void loadColorFrom(const ::engine::raster::detail::OpenGLRasterResource& source);

    /// Blit a resident OpenGL depth resource into this set's depth attachment.
    void loadDepthFrom(const ::engine::raster::detail::OpenGLRasterResource& source);

    /// Blit a resident OpenGL stencil resource into this set's stencil attachment.
    void loadStencilFrom(const ::engine::raster::detail::OpenGLRasterResource& source);

    /// Read the color attachment into `target` with row-0 at the
    /// visible top. Multisample sources are resolved via
    /// `glBlitFramebuffer` into a temporary single-sample read FBO.
    void copyColorTo(::Buffer<Colord>& target);

    /// Read the depth attachment as normalized [0, 1] doubles with
    /// the same row-flip as `copyColorTo`. Sets `errorMessage()` and
    /// no-ops when `samples() > 0`.
    void copyDepthTo(::Buffer<double>& target);

    /// Read the stencil attachment as raw bytes with the same
    /// row-flip. Sets `errorMessage()` and no-ops when `samples() > 0`.
    void copyStencilTo(::Buffer<std::uint8_t>& target);

    /// Copy one attachment into a graph-owned resident OpenGL renderbuffer.
    std::shared_ptr<::engine::raster::detail::OpenGLRasterResource>
    residentCopy(engine::graph::RenderResourceType type,
                 std::shared_ptr<::engine::raster::gl::Context> sourceContext);

    /// Last operation's error message; empty when the most recent
    /// operation succeeded.
    const std::string& errorMessage() const {
      return m_errorMessage;
    }

  private:
    void loadFrom(const ::engine::raster::detail::OpenGLRasterResource& source, GLbitfield mask);

    GLuint m_fbo{0};
    GLuint m_colorRenderbuffer{0};
    GLuint m_depthStencilRenderbuffer{0};
    int m_width{0};
    int m_height{0};
    int m_samples{0};
    mutable std::string m_errorMessage;
  };
}
