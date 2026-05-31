#include "engine/raster/gl/ShaderProgram.h"

#include <cstdint>
#include <vector>

namespace engine::raster::gl {
  namespace {
    GLenum toGLStage(ShaderProgram::ShaderStage stage) {
      switch (stage) {
      case ShaderProgram::ShaderStage::Vertex:
        return GL_VERTEX_SHADER;
      case ShaderProgram::ShaderStage::Fragment:
        return GL_FRAGMENT_SHADER;
      }
      return GL_VERTEX_SHADER;
    }

    std::string shaderInfoLog(GLuint shader) {
      GLint length = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
      if (length <= 0) {
        return {};
      }
      std::vector<char> buffer(static_cast<std::size_t>(length));
      glGetShaderInfoLog(shader, length, nullptr, buffer.data());
      return std::string(buffer.data());
    }

    std::string programInfoLog(GLuint program) {
      GLint length = 0;
      glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
      if (length <= 0) {
        return {};
      }
      std::vector<char> buffer(static_cast<std::size_t>(length));
      glGetProgramInfoLog(program, length, nullptr, buffer.data());
      return std::string(buffer.data());
    }
  }

  ShaderProgram::ShaderProgram()
      : m_id(glCreateProgram()) {
  }

  ShaderProgram::~ShaderProgram() {
    if (m_id != 0) {
      glDeleteProgram(m_id);
    }
  }

  bool ShaderProgram::addShaderFromSourceCode(ShaderStage stage, const char* source) {
    if (m_id == 0) {
      m_errorLog = "ShaderProgram::addShaderFromSourceCode without a program object";
      return false;
    }
    const GLuint shader = glCreateShader(toGLStage(stage));
    if (shader == 0) {
      m_errorLog = "glCreateShader returned 0";
      return false;
    }
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
      m_errorLog = shaderInfoLog(shader);
      glDeleteShader(shader);
      return false;
    }

    glAttachShader(m_id, shader);
    // GL keeps the shader alive while it's attached to the program;
    // marking for deletion now means it's freed when the program is
    // deleted (or when detached). Saves having to track shader IDs
    // here for cleanup.
    glDeleteShader(shader);
    return true;
  }

  bool ShaderProgram::link() {
    if (m_id == 0) {
      m_errorLog = "ShaderProgram::link without a program object";
      return false;
    }
    glLinkProgram(m_id);
    GLint linked = GL_FALSE;
    glGetProgramiv(m_id, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
      m_errorLog = programInfoLog(m_id);
      m_linked = false;
      return false;
    }
    m_linked = true;
    m_errorLog.clear();
    return true;
  }

  bool ShaderProgram::bind() {
    if (m_id == 0 || !m_linked) {
      return false;
    }
    glUseProgram(m_id);
    return true;
  }

  void ShaderProgram::release() {
    glUseProgram(0);
  }

  int ShaderProgram::attributeLocation(const char* name) const {
    if (m_id == 0)
      return -1;
    return glGetAttribLocation(m_id, name);
  }

  int ShaderProgram::uniformLocation(const char* name) const {
    if (m_id == 0)
      return -1;
    return glGetUniformLocation(m_id, name);
  }

  void ShaderProgram::enableAttributeArray(int location) {
    if (location < 0)
      return;
    glEnableVertexAttribArray(static_cast<GLuint>(location));
  }

  void ShaderProgram::disableAttributeArray(int location) {
    if (location < 0)
      return;
    glDisableVertexAttribArray(static_cast<GLuint>(location));
  }

  void ShaderProgram::setAttributeBuffer(int location, GLenum type, int offset, int tupleSize,
                                         int stride) {
    if (location < 0)
      return;
    const void* pointer = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(offset));
    glVertexAttribPointer(static_cast<GLuint>(location), tupleSize, type, GL_FALSE, stride,
                          pointer);
  }

  void ShaderProgram::setUniformValue(const char* name, bool value) {
    const int loc = uniformLocation(name);
    if (loc < 0)
      return;
    glUniform1i(loc, value ? 1 : 0);
  }

  void ShaderProgram::setUniformValue(const char* name, int value) {
    const int loc = uniformLocation(name);
    if (loc < 0)
      return;
    glUniform1i(loc, value);
  }

  void ShaderProgram::setUniformValue(const char* name, GLfloat value) {
    const int loc = uniformLocation(name);
    if (loc < 0)
      return;
    glUniform1f(loc, value);
  }

  void ShaderProgram::setUniformValue(const char* name, GLfloat x, GLfloat y) {
    const int loc = uniformLocation(name);
    if (loc < 0)
      return;
    glUniform2f(loc, x, y);
  }

  void ShaderProgram::setUniformValue(const char* name, GLfloat x, GLfloat y, GLfloat z) {
    const int loc = uniformLocation(name);
    if (loc < 0)
      return;
    glUniform3f(loc, x, y, z);
  }
}
