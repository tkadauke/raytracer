#pragma once

#include "engine/raster/detail/OpenGLShadowTextureData.h"
#include "engine/raster/gl/Bindings.h"

#include <cstdint>
#include <stdexcept>

namespace engine::raster::detail {
  /**
    * 1×1 white RGBA texture bound to whichever sampler the shader
    * expects when no real texture is active. Lets the OpenGL raster
    * fragment shader keep a `sampler2D` binding pinned to a valid
    * texture in every code path, including the constant-color and
    * checker albedo modes that never actually sample.
    *
    * Constructed against a current GL context; destructor releases the
    * texture against the same context. RAII.
    */
  class OpenGLFallbackTexture {
  public:
    OpenGLFallbackTexture() {
      static constexpr GLfloat pixels[] = {1.0f, 1.0f, 1.0f, 1.0f};

      glGenTextures(1, &m_texture);
      if (m_texture == 0) {
        throw std::runtime_error("OpenGL raster backend could not allocate a fallback texture");
      }

      glBindTexture(GL_TEXTURE_2D, m_texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_FLOAT, pixels);
      glBindTexture(GL_TEXTURE_2D, 0);
    }

    ~OpenGLFallbackTexture() {
      if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
      }
    }

    OpenGLFallbackTexture(const OpenGLFallbackTexture&) = delete;
    OpenGLFallbackTexture& operator=(const OpenGLFallbackTexture&) = delete;

    void bind(int textureUnit) const {
      glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + textureUnit));
      glBindTexture(GL_TEXTURE_2D, m_texture);
    }

    void release(int textureUnit) const {
      glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + textureUnit));
      glBindTexture(GL_TEXTURE_2D, 0);
    }

  private:
    GLuint m_texture{0};
  };

  /**
    * RGBA32F texture holding a single directional shadow cascade for the
    * OpenGL fragment shader's shadow sampling path. No-op (`enabled()`
    * returns false) when the source `OpenGLShadowTextureData` is empty
    * — that's the common case where shader-side shadowing isn't
    * eligible and the renderer relies on CPU-baked shadow visibility.
    */
  class OpenGLShadowTexture {
  public:
    explicit OpenGLShadowTexture(const OpenGLShadowTextureData& data) {
      if (!data.enabled()) {
        return;
      }

      glGenTextures(1, &m_texture);
      if (m_texture == 0) {
        throw std::runtime_error("OpenGL raster backend could not allocate a shadow texture");
      }

      glBindTexture(GL_TEXTURE_2D, m_texture);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
      glTexImage2D(GL_TEXTURE_2D, 0, internalFormat(), data.width(), data.height(), 0, GL_RGBA,
                   GL_FLOAT, data.rgbaPixels().data());
      glBindTexture(GL_TEXTURE_2D, 0);
    }

    ~OpenGLShadowTexture() {
      if (m_texture != 0) {
        glDeleteTextures(1, &m_texture);
      }
    }

    OpenGLShadowTexture(const OpenGLShadowTexture&) = delete;
    OpenGLShadowTexture& operator=(const OpenGLShadowTexture&) = delete;

    bool enabled() const {
      return m_texture != 0;
    }

    void bind(int textureUnit) const {
      glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + textureUnit));
      glBindTexture(GL_TEXTURE_2D, m_texture);
    }

    void release(int textureUnit) const {
      glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + textureUnit));
      glBindTexture(GL_TEXTURE_2D, 0);
    }

  private:
    static GLint internalFormat() {
#if defined(GL_RGBA32F)
      return GL_RGBA32F;
#else
      return GL_RGBA;
#endif
    }

    GLuint m_texture{0};
  };
}
