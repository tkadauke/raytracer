#pragma once

#include "engine/raster/OpenGLOffscreenContext.h"

#include <memory>

class QOpenGLShaderProgram;

namespace engine::raster::detail {
  /**
    * Attribute slot indices for the OpenGL raster shader program. Populated
    * once at link time and reused across `OpenGLRasterizer::render()` calls
    * by `OpenGLRasterResourceCache`.
    */
  struct OpenGLRasterAttributeLocations {
    int position{-1};
    int worldPosition{-1};
    int normal{-1};
    int color{-1};
    int uv{-1};
    int alphaScale{-1};
    int materialDiffuse{-1};
    int materialSpecularColor{-1};
    int materialSpecularCoefficient{-1};
    int materialSpecularExponent{-1};
    int ambientLighting{-1};
    int directLighting{-1};
    int specular{-1};
    int albedoMode{-1};

    bool resolved() const;
  };

  /**
    * Owns the persistent GPU-side state of a single `OpenGLRasterizer`
    * instance — the offscreen Qt context/surface/FBO and the linked raster
    * shader program with its attribute slot lookups. Each `OpenGLRasterizer`
    * (including each `cloneForRender` clone) owns one cache.
    *
    * `ensureProgram()` compiles and links the program on first use; later
    * calls return immediately. The destructor makes the offscreen context
    * current before releasing the program so the GL resources are freed
    * against the right context.
    */
  struct OpenGLRasterResourceCache {
    OpenGLOffscreenContext context;
    std::unique_ptr<QOpenGLShaderProgram> program;
    OpenGLRasterAttributeLocations locations;

    OpenGLRasterResourceCache();
    ~OpenGLRasterResourceCache();

    OpenGLRasterResourceCache(const OpenGLRasterResourceCache&) = delete;
    OpenGLRasterResourceCache& operator=(const OpenGLRasterResourceCache&) = delete;

    /**
      * Compiles/links the raster shader program against the currently-bound
      * GL context on first call. Subsequent calls return immediately. The
      * caller must have made the offscreen context current before invoking
      * this method.
      *
      * @throws std::runtime_error if shader compilation, linking, or
      * attribute resolution fails.
      */
    void ensureProgram();
  };
}
