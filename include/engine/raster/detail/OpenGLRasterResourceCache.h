#pragma once

#include "core/math/Vector.h"
#include "engine/raster/OpenGLOffscreenContext.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raster/detail/OpenGLRasterMesh.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>

class QOpenGLBuffer;
class QOpenGLFunctions;
class QOpenGLShaderProgram;

namespace render {
  class ImageTexture;
  class Scene;
}

namespace engine::raster {
  class RasterVisibilitySet;
}

namespace engine::raster::detail {
  class ShadowMaps;
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
    * Cache key for the prepared `OpenGLRasterMesh`. The mesh content
    * depends on these inputs; camera-only changes (e.g. dragging in the
    * Modeler) do NOT change the key when the camera projection is moved
    * to the GPU shader (see `Camera::worldToClipMatrix`). On scene
    * mutations, `render::Scene` itself is replaced (the Modeler edit
    * pipeline calls `Scene::toRaytracerScene` which constructs a fresh
    * one), so weak-pointer identity comparison correctly invalidates the
    * cache.
    *
    * The scene is held as `weak_ptr` so a freed-then-reallocated scene
    * at the same memory address does not produce a false cache hit —
    * the previous weak_ptr expires when the original scene is
    * destroyed, and `sceneMatches` returns false even if the new
    * `shared_ptr` happens to wrap the same address.
    *
    * When the build path bakes CPU-side specular lighting that depends
    * on the camera (overflow lights with specular materials), the
    * `cameraPosition` field captures that dependence; same camera =
    * cache hit, different camera = cache miss.
    */
  struct OpenGLMeshCacheKey {
    std::weak_ptr<const render::Scene> scene;
    const RasterVisibilitySet* visibilitySet{nullptr};
    const ShadowMaps* shadowMaps{nullptr};
    int lod{0};
    int viewportWidth{0};
    int viewportHeight{0};
    Rasterizer::CullMode cullMode{Rasterizer::CullMode::Both};
    bool hasCullModeOverride{false};
    double depthBias{0.0};
    Vector3d cameraPosition; // only meaningful when cameraDependent is true
    Vector3d cameraTarget;   // ditto
    bool cameraDependent{false};

    bool sceneMatches(const std::shared_ptr<const render::Scene>& other) const;
    bool matches(const OpenGLMeshCacheKey& other) const;
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
    std::unique_ptr<QOpenGLBuffer> vertexBuffer;
    std::unique_ptr<QOpenGLBuffer> indexBuffer;
    std::optional<OpenGLMeshCacheKey> cachedMeshKey;
    OpenGLRasterMesh cachedMesh;

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

    /**
      * Lazily allocates persistent vertex and index `QOpenGLBuffer`s on
      * first call. Subsequent calls reuse the existing GL buffer objects;
      * the draw pass re-uploads the current frame's vertex/index payload
      * via `QOpenGLBuffer::allocate` (which calls `glBufferData`).
      *
      * The caller must have made the offscreen context current.
      *
      * @throws std::runtime_error if buffer creation fails.
      */
    void ensureMeshBuffers();
  };
}
