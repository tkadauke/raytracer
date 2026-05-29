#pragma once

#include "engine/raster/OpenGLOffscreenContext.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

class QOpenGLFunctions;
class QOpenGLShaderProgram;

namespace render {
  class ImageTexture;
}

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
    * Caches GL texture handles for `ImageTexture` albedo sources so each
    * image uploads only once per OpenGLRasterizer lifetime. Stored handles
    * are valid for the cache's owning context; release must happen with
    * that context current.
    */
  struct OpenGLRasterImageTextureCache {
    std::unordered_map<const render::ImageTexture*, std::uint32_t> textures;

    std::uint32_t textureFor(const render::ImageTexture& image, QOpenGLFunctions* functions);
    void releaseAll(QOpenGLFunctions* functions);
  };

  /**
    * Owns the persistent GPU-side state of a single `OpenGLRasterizer`
    * instance — the offscreen Qt context/surface/FBO, the linked raster
    * shader program with its attribute slot lookups, and the per-image GL
    * texture handles for `ImageTexture` albedo sources. Each
    * `OpenGLRasterizer` (including each `cloneForRender` clone) owns one
    * cache.
    *
    * `ensureProgram()` compiles and links the program on first use; later
    * calls return immediately. The destructor makes the offscreen context
    * current before releasing the program and image textures so the GL
    * resources are freed against the right context.
    */
  struct OpenGLRasterResourceCache {
    OpenGLOffscreenContext context;
    std::unique_ptr<QOpenGLShaderProgram> program;
    OpenGLRasterAttributeLocations locations;
    std::unique_ptr<OpenGLRasterImageTextureCache> imageTextures;

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
