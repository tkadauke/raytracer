#pragma once

#include "engine/raster/gl/Bindings.h"

#include <cstddef>

namespace engine::raster::gl {
  /**
    * RAII wrapper around a single GL buffer object (VBO or IBO).
    *
    * Replaces `QOpenGLBuffer` so the raster cache doesn't pin
    * itself to a Qt OpenGL context — `Buffer` uses raw `gl*`
    * symbols and works against any current GL context, including the
    * native `gl::CGLContext`. The wrapper deliberately keeps the
    * same surface the Qt class exposed (create, bind, release,
    * allocate, destroy, isValid) so migrating call sites is a
    * mechanical type swap.
    *
    * The caller is responsible for making a GL context current
    * before any method that touches GL state (`create`, `bind`,
    * `allocate`, `destroy`, the destructor). The destructor leaks
    * the GL handle to the OS if no context is current, matching the
    * shutdown-safety pattern in
    * `OpenGLRasterResourceCache::~OpenGLRasterResourceCache`.
    */
  class Buffer {
  public:
    enum class Target {
      Vertex,
      Index,
    };

    explicit Buffer(Target target);
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    /// Allocate a GL buffer object name (`glGenBuffers`). Requires a
    /// current context. Returns true on success.
    bool create();

    /// Release the GL handle (`glDeleteBuffers`). Subsequent `bind`
    /// calls fail until the buffer is recreated.
    void destroy();

    bool isValid() const {
      return m_id != 0;
    }

    /// `glBindBuffer(target, id)`. No-op when `isValid()` is false.
    void bind();

    /// `glBindBuffer(target, 0)`.
    void release();

    /// `glBufferData(target, size, data, GL_STATIC_DRAW)`. Re-allocates
    /// the buffer's storage to `size` bytes and uploads the payload.
    /// Requires the buffer to be currently bound.
    void allocate(const void* data, std::size_t size);

    /// Raw GL handle for callers that still need to feed it directly
    /// to `glVertexAttribPointer` etc.
    GLuint id() const {
      return m_id;
    }

    /// Underlying GL target enum (`GL_ARRAY_BUFFER` or
    /// `GL_ELEMENT_ARRAY_BUFFER`).
    GLenum glTarget() const {
      return m_glTarget;
    }

  private:
    GLenum m_glTarget;
    GLuint m_id{0};
  };
}
