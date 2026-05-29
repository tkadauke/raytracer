#pragma once

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

    MeshStatsLookup meshStatsFor(const render::Primitive& primitive, int lod);
    void clear();
    std::size_t size() const;

  private:
    struct MeshStatsKey {
      const render::Primitive* primitive{nullptr};
      int lod{0};
      std::string boundsFingerprint;

      bool operator<(const MeshStatsKey& other) const;
    };

    static MeshStats buildMeshStats(const render::Primitive& primitive, int lod);
    static std::string boundsFingerprint(const render::Primitive& primitive);

    mutable std::mutex m_mutex;
    std::map<MeshStatsKey, MeshStats> m_meshStats;
  };
}
