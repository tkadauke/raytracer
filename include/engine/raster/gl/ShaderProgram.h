#pragma once

#include "engine/raster/gl/Bindings.h"

#include <cstddef>
#include <string>

namespace engine::raster::gl {
  /**
    * Compiled-and-linked GLSL program wrapper over raw GL —
    * `glCreateShader/glCompileShader/glCreateProgram/glLinkProgram` +
    * `glGetAttribLocation/glGetUniformLocation` + `glUniform*` /
    * `glVertexAttribPointer`.
    *
    * Replaces `QOpenGLShaderProgram` in the raster cache so the
    * cache + draw pass can run against any current GL context,
    * including the native `gl::CGLContext` (the Qt class requires
    * a `QOpenGLContext::currentContext()`).
    *
    * Surface mirrors the subset of `QOpenGLShaderProgram` the
    * rasterizer used (addShaderFromSourceCode for vertex/fragment,
    * link/bind/release/isLinked, attribute/uniform lookups,
    * setAttributeBuffer, the small handful of `setUniformValue`
    * overloads we exercise). Other overloads are intentionally
    * absent — add them as new use cases land.
    *
    * Threading: the caller is responsible for making a GL context
    * current before any GL-touching method. The destructor releases
    * the program with the context current; on the no-context
    * shutdown path the cache must `release()` the unique_ptr (leak
    * the heap object) the same way the Qt path did.
    */
  class ShaderProgram {
  public:
    enum class ShaderStage {
      Vertex,
      Fragment,
    };

    ShaderProgram();
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;

    /// Compile a stage's source and attach it to the program.
    /// `errorLog()` is populated on failure. The program is not yet
    /// usable until `link()`.
    bool addShaderFromSourceCode(ShaderStage stage, const char* source);

    /// `glLinkProgram` after all stages are attached. `errorLog()`
    /// is populated on failure.
    bool link();

    bool isLinked() const {
      return m_linked;
    }

    /// Compile / link diagnostics.
    const std::string& errorLog() const {
      return m_errorLog;
    }

    /// `glUseProgram(id)`.
    bool bind();
    /// `glUseProgram(0)`.
    void release();

    int attributeLocation(const char* name) const;
    int uniformLocation(const char* name) const;

    void enableAttributeArray(int location);
    void disableAttributeArray(int location);

    /// `glVertexAttribPointer(location, tupleSize, GL_FLOAT, GL_FALSE,
    /// stride, offset)`. Caller must have the source VBO currently
    /// bound to `GL_ARRAY_BUFFER`.
    void setAttributeBuffer(int location, GLenum type, int offset, int tupleSize, int stride);

    void setUniformValue(const char* name, bool value);
    void setUniformValue(const char* name, int value);
    void setUniformValue(const char* name, GLfloat value);
    void setUniformValue(const char* name, GLfloat x, GLfloat y);
    void setUniformValue(const char* name, GLfloat x, GLfloat y, GLfloat z);

    /// Raw GL handle — for callers (the rasterizer's
    /// viewProjection upload) that need `glUniformMatrix4fv` etc.
    GLuint id() const {
      return m_id;
    }

  private:
    GLuint m_id{0};
    bool m_linked{false};
    std::string m_errorLog;
  };
}
