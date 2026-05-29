#include "engine/raster/RasterVisibilitySceneCache.h"

#include "core/geometry/Mesh.h"
#include "core/math/BoundingBox.h"
#include "render/primitives/Primitive.h"

#include <sstream>
#include <tuple>
#include <utility>

namespace engine::raster {
  bool RasterVisibilitySceneCache::MeshStatsKey::operator<(const MeshStatsKey& other) const {
    return std::tie(primitive, lod, boundsFingerprint) <
           std::tie(other.primitive, other.lod, other.boundsFingerprint);
  }

  RasterVisibilitySceneCache::MeshStatsLookup
  RasterVisibilitySceneCache::meshStatsFor(const render::Primitive& primitive, int lod) {
    const MeshStatsKey key{&primitive, lod, boundsFingerprint(primitive)};
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      const auto it = m_meshStats.find(key);
      if (it != m_meshStats.end()) {
        return {it->second, true};
      }
    }

    MeshStats stats = buildMeshStats(primitive, lod);
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      const auto [it, inserted] = m_meshStats.emplace(key, std::move(stats));
      return {it->second, !inserted};
    }
  }

  void RasterVisibilitySceneCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_meshStats.clear();
  }

  std::size_t RasterVisibilitySceneCache::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_meshStats.size();
  }

  RasterVisibilitySceneCache::MeshStats
  RasterVisibilitySceneCache::buildMeshStats(const render::Primitive& primitive, int lod) {
    MeshStats stats;
    const std::shared_ptr<Mesh> mesh = primitive.tessellate(lod);
    if (mesh) {
      stats.mesh = mesh;
      stats.faceCount = mesh->faces().size();
      for (const auto& face : mesh->faces()) {
        if (face.size() >= 3) {
          stats.triangleCount += face.size() - 2;
        }
      }
    }
    return stats;
  }

  std::string RasterVisibilitySceneCache::boundsFingerprint(const render::Primitive& primitive) {
    const BoundingBoxd& bounds = primitive.boundingBox();
    std::ostringstream out;
    out << "valid=" << bounds.isValid() << ";undefined=" << bounds.isUndefined()
        << ";infinite=" << bounds.isInfinite() << ';';
    if (bounds.isValid() && !bounds.isUndefined() && !bounds.isInfinite()) {
      out << "min=" << bounds.min().x() << ',' << bounds.min().y() << ',' << bounds.min().z()
          << ";max=" << bounds.max().x() << ',' << bounds.max().y() << ',' << bounds.max().z()
          << ';';
    }
    return out.str();
  }
}
