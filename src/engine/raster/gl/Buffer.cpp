#include "engine/raster/gl/Buffer.h"

namespace engine::raster::gl {
  namespace {
    GLenum toGLTarget(Buffer::Target target) {
      switch (target) {
      case Buffer::Target::Vertex:
        return GL_ARRAY_BUFFER;
      case Buffer::Target::Index:
        return GL_ELEMENT_ARRAY_BUFFER;
      }
      return GL_ARRAY_BUFFER;
    }
  }

  Buffer::Buffer(Target target)
      : m_glTarget(toGLTarget(target)) {
  }

  Buffer::~Buffer() {
    destroy();
  }

  bool Buffer::create() {
    if (m_id != 0) {
      return true;
    }
    glGenBuffers(1, &m_id);
    return m_id != 0;
  }

  void Buffer::destroy() {
    if (m_id != 0) {
      glDeleteBuffers(1, &m_id);
      m_id = 0;
    }
  }

  void Buffer::bind() {
    if (m_id == 0) {
      return;
    }
    glBindBuffer(m_glTarget, m_id);
  }

  void Buffer::release() {
    glBindBuffer(m_glTarget, 0);
  }

  void Buffer::allocate(const void* data, std::size_t size) {
    glBufferData(m_glTarget, static_cast<GLsizeiptr>(size), data, GL_STATIC_DRAW);
  }
}
