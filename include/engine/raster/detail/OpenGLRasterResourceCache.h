#pragma once

#include "core/math/Vector.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raster/detail/OpenGLRasterMesh.h"
#include "engine/raster/gl/Context.h"

#include <array>
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
    // `visibilitySet` and `shadowMaps` are held as weak_ptrs for the
    // same reason as `scene`: a pool-allocated visibility set / shadow
    // map that gets freed and reallocated at the same address would
    // otherwise produce a false cache hit. `matches()` compares via
    // `lock().get()` so both-null counts as equal.
    std::weak_ptr<const RasterVisibilitySet> visibilitySet;
    std::weak_ptr<const ShadowMaps> shadowMaps;
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
  /**
    * One entry in the LRU mesh cache. `lastUsed` rises monotonically on
    * each touch (build or hit); the slot with the smallest `lastUsed`
    * is evicted on the next miss. A `lastUsed == 0` slot is empty.
    */
  struct OpenGLCachedMeshEntry {
    std::optional<OpenGLMeshCacheKey> key;
    OpenGLRasterMesh mesh;
    std::uint64_t lastUsed{0};
  };

  /**
    * LRU cache capacity. A multi-pass render graph typically renders
    * color + depth AOV + stencil AOV per frame, sometimes plus object
    * ID; four slots covers that without thrashing.
    */
  inline constexpr std::size_t kOpenGLMeshCacheSize = 4;

  struct OpenGLRasterResourceCache {
    // Held as `unique_ptr<gl::Context>` so the cache stays agnostic
    // of which backend created it. `OpenGLRasterResourceCache`'s
    // default constructor picks via `gl::createOffscreenContext()`:
    // Qt-backed when a QGuiApplication is up (Modeler), CGL otherwise
    // (rendercli).
    std::unique_ptr<gl::Context> context;
    std::unique_ptr<QOpenGLShaderProgram> program;
    OpenGLRasterAttributeLocations locations;
    std::unique_ptr<OpenGLRasterImageTextureCache> imageTextures;
    std::unique_ptr<QOpenGLBuffer> vertexBuffer;
    std::unique_ptr<QOpenGLBuffer> indexBuffer;
    std::array<OpenGLCachedMeshEntry, kOpenGLMeshCacheSize> meshCache;
    // Monotonic tick used as the LRU sort key. Bumped on every cache
    // build/hit; new builds get the highest value, the slot with the
    // smallest value (or 0, empty) is evicted next.
    std::uint64_t meshUseTick{0};
    // Index into `meshCache` of the entry whose vertex/index payload is
    // currently in `vertexBuffer` / `indexBuffer`. -1 means the GPU
    // buffers are stale (or were never populated). The draw pass
    // checks this against the active entry's index; mismatch means
    // re-upload. Keeps the per-frame upload off the LRU-hit path even
    // when multiple passes rotate through several cached meshes.
    std::ptrdiff_t uploadedMeshSlot{-1};

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

    /**
      * Result of `acquireMeshSlot`. `entry` is the cache slot to use;
      * the caller fills `entry->mesh` and `entry->key` on a miss. `slot`
      * is the slot's array index (for the upload-skip tracking).
      */
    struct MeshSlotResult {
      OpenGLCachedMeshEntry* entry{nullptr};
      std::ptrdiff_t slot{-1};
      bool hit{false};
    };

    /**
      * Find a cached mesh whose key matches, or evict the LRU slot for
      * the caller to populate. On a hit the slot's `lastUsed` is
      * bumped to the highest tick. On a miss the chosen slot's
      * previous `key`/`mesh` are reset; the caller must overwrite
      * them and the caller is responsible for invalidating
      * `uploadedMeshSlot` if the chosen slot equals it (its GL bytes
      * are stale).
      */
    MeshSlotResult acquireMeshSlot(const OpenGLMeshCacheKey& key);
  };
}
