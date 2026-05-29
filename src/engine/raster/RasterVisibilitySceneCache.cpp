#include "engine/raster/RasterVisibilitySceneCache.h"

#include "core/geometry/Mesh.h"
#include "core/math/BoundingBox.h"
#include "engine/raster/detail/RasterMaterial.h"
#include "render/materials/Material.h"
#include "render/primitives/Primitive.h"

#include <sstream>
#include <tuple>
#include <utility>

namespace engine::raster {
  bool RasterVisibilitySceneCache::MeshStatsKey::operator<(const MeshStatsKey& other) const {
    return std::tie(primitive, lod, boundsFingerprint) <
           std::tie(other.primitive, other.lod, other.boundsFingerprint);
  }

  bool RasterVisibilitySceneCache::TransformedBoundsKey::operator<(
    const TransformedBoundsKey& other) const {
    return std::tie(primitive, boundsFingerprint, matrixFingerprint) <
           std::tie(other.primitive, other.boundsFingerprint, other.matrixFingerprint);
  }

  bool RasterVisibilitySceneCache::MaterialCullabilityKey::operator<(
    const MaterialCullabilityKey& other) const {
    return std::tie(material, sidedness) < std::tie(other.material, other.sidedness);
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

  RasterVisibilitySceneCache::TransformedBoundsLookup
  RasterVisibilitySceneCache::transformedBoundsFor(const render::Primitive& primitive,
                                                   const Matrix4d& pointMatrix) {
    const TransformedBoundsKey key{&primitive, boundsFingerprint(primitive),
                                   matrixFingerprint(pointMatrix)};
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      const auto it = m_transformedBounds.find(key);
      if (it != m_transformedBounds.end()) {
        return {it->second, true};
      }
    }

    BoundingBoxd bounds = buildTransformedBounds(primitive, pointMatrix);
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      const auto [it, inserted] = m_transformedBounds.emplace(key, std::move(bounds));
      return {it->second, !inserted};
    }
  }

  RasterVisibilitySceneCache::MaterialCullabilityLookup
  RasterVisibilitySceneCache::materialCullabilityFor(
    const std::shared_ptr<render::Material>& material) {
    const MaterialCullabilityKey key{material.get(), materialSidednessFingerprint(material)};
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      const auto it = m_materialCullability.find(key);
      if (it != m_materialCullability.end()) {
        return {it->second, true};
      }
    }

    MaterialCullability cullability = buildMaterialCullability(material);
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      const auto [it, inserted] = m_materialCullability.emplace(key, std::move(cullability));
      return {it->second, !inserted};
    }
  }

  void RasterVisibilitySceneCache::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_meshStats.clear();
    m_transformedBounds.clear();
    m_materialCullability.clear();
  }

  std::size_t RasterVisibilitySceneCache::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_meshStats.size();
  }

  std::size_t RasterVisibilitySceneCache::transformedBoundsSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_transformedBounds.size();
  }

  std::size_t RasterVisibilitySceneCache::materialCullabilitySize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_materialCullability.size();
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

  BoundingBoxd
  RasterVisibilitySceneCache::buildTransformedBounds(const render::Primitive& primitive,
                                                     const Matrix4d& pointMatrix) {
    const BoundingBoxd& bounds = primitive.boundingBox();
    if (!bounds.isValid() || bounds.isUndefined() || bounds.isInfinite()) {
      return bounds;
    }

    BoundingBoxd result;
    for (const Vector3d& vertex : bounds.vertices()) {
      result.include(pointMatrix.transformPoint(vertex));
    }
    return result;
  }

  RasterVisibilitySceneCache::MaterialCullability
  RasterVisibilitySceneCache::buildMaterialCullability(
    const std::shared_ptr<render::Material>& material) {
    return {detail::RasterMaterialSource::from(material).defaultCullMode()};
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

  std::string RasterVisibilitySceneCache::matrixFingerprint(const Matrix4d& matrix) {
    std::ostringstream out;
    for (int row = 0; row != 4; ++row) {
      for (int column = 0; column != 4; ++column) {
        out << matrix[row][column] << ';';
      }
    }
    return out.str();
  }

  int RasterVisibilitySceneCache::materialSidednessFingerprint(
    const std::shared_ptr<render::Material>& material) {
    if (!material) {
      return static_cast<int>(render::Material::Sidedness::TwoSided);
    }
    return static_cast<int>(material->sidedness());
  }
}
