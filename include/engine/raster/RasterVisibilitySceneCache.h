#pragma once

#include "core/math/BoundingBox.h"
#include "core/math/Matrix.h"

#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>

class Mesh;

namespace render {
  class Primitive;
}

namespace engine::raster {
  /**
    * Scene-side cache for raster visibility preprocessing.
    *
    * Visibility sets are camera dependent, but primitive tessellation stats are
    * stable for a primitive/lod/bounds tuple. Graph render clones share this
    * cache so camera movement can recompute visibility while reusing immutable
    * scene-side mesh facts.
    */
  class RasterVisibilitySceneCache {
  public:
    struct MeshStats {
      std::size_t triangleCount{0};
      std::size_t faceCount{0};
      std::shared_ptr<const Mesh> mesh;
    };

    struct MeshStatsLookup {
      MeshStats stats;
      bool hit{false};
    };

    struct TransformedBoundsLookup {
      BoundingBoxd bounds;
      bool hit{false};
    };

    MeshStatsLookup meshStatsFor(const render::Primitive& primitive, int lod);
    TransformedBoundsLookup transformedBoundsFor(const render::Primitive& primitive,
                                                 const Matrix4d& pointMatrix);
    void clear();
    std::size_t size() const;
    std::size_t transformedBoundsSize() const;

  private:
    struct MeshStatsKey {
      const render::Primitive* primitive{nullptr};
      int lod{0};
      std::string boundsFingerprint;

      bool operator<(const MeshStatsKey& other) const;
    };

    struct TransformedBoundsKey {
      const render::Primitive* primitive{nullptr};
      std::string boundsFingerprint;
      std::string matrixFingerprint;

      bool operator<(const TransformedBoundsKey& other) const;
    };

    static MeshStats buildMeshStats(const render::Primitive& primitive, int lod);
    static BoundingBoxd buildTransformedBounds(const render::Primitive& primitive,
                                               const Matrix4d& pointMatrix);
    static std::string boundsFingerprint(const render::Primitive& primitive);
    static std::string matrixFingerprint(const Matrix4d& matrix);

    mutable std::mutex m_mutex;
    std::map<MeshStatsKey, MeshStats> m_meshStats;
    std::map<TransformedBoundsKey, BoundingBoxd> m_transformedBounds;
  };
}
