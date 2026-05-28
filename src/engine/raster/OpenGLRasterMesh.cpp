#include "engine/raster/detail/OpenGLRasterMesh.h"

#include "engine/raster/Rasterizer.h"
#include "engine/raster/detail/RasterPipelineTypes.h"
#include "engine/raster/detail/RasterTriangleEmitter.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"
#include "render/viewplanes/ViewPlane.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace engine::raster::detail {
  namespace {
    float normalizedDeviceX(double screenX, const Recti& rect) {
      if (rect.width() <= 0) {
        return 0.0f;
      }
      return static_cast<float>(
        ((screenX - rect.left()) / static_cast<double>(rect.width())) * 2.0 - 1.0);
    }

    float normalizedDeviceY(double screenY, const Recti& rect) {
      if (rect.height() <= 0) {
        return 0.0f;
      }
      return static_cast<float>(
        1.0 - ((screenY - rect.top()) / static_cast<double>(rect.height())) * 2.0);
    }

    float normalizedDeviceDepth(const RasterVertex& vertex) {
      if (vertex.invW == 0.0) {
        return 0.0f;
      }
      const double depth = vertex.depthOverW / vertex.invW;
      if (!std::isfinite(depth)) {
        return 0.0f;
      }
      const double normalized = depth / (depth + 1.0);
      return static_cast<float>(std::clamp(normalized * 2.0 - 1.0, -1.0, 1.0));
    }

    OpenGLRasterMesh::Vertex vertexFor(const RasterTriangle& triangle, const RasterVertex& vertex,
                                       const Recti& rect) {
      const Colord albedo = triangle.rasterMaterial.albedo(
        triangle.primitive, vertex.point, vertex.normal, vertex.uv, triangle.uvDx, triangle.uvDy);
      const double alpha = triangle.rasterMaterial.alpha(
        triangle.primitive, vertex.point, vertex.normal, vertex.uv, triangle.uvDx, triangle.uvDy);
      return {normalizedDeviceX(vertex.x, rect),
              normalizedDeviceY(vertex.y, rect),
              normalizedDeviceDepth(vertex),
              static_cast<float>(std::clamp(albedo.r(), 0.0, 1.0)),
              static_cast<float>(std::clamp(albedo.g(), 0.0, 1.0)),
              static_cast<float>(std::clamp(albedo.b(), 0.0, 1.0)),
              static_cast<float>(std::clamp(alpha, 0.0, 1.0))};
    }
  }

  bool OpenGLRasterMesh::empty() const {
    return m_indices.empty();
  }

  std::size_t OpenGLRasterMesh::triangleCount() const {
    return m_indices.size() / 3;
  }

  const OpenGLRasterMesh::Vertices& OpenGLRasterMesh::vertices() const {
    return m_vertices;
  }

  const OpenGLRasterMesh::Indices& OpenGLRasterMesh::indices() const {
    return m_indices;
  }

  void OpenGLRasterMesh::appendTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
    const auto base = static_cast<std::uint32_t>(m_vertices.size());
    m_vertices.push_back(v0);
    m_vertices.push_back(v1);
    m_vertices.push_back(v2);
    m_indices.push_back(base);
    m_indices.push_back(base + 1);
    m_indices.push_back(base + 2);
  }

  OpenGLRasterMeshBuilder::OpenGLRasterMeshBuilder(const render::Scene* scene,
                                                   std::shared_ptr<render::Camera> camera, int lod,
                                                   const Recti& viewportRect,
                                                   Rasterizer::CullMode cullMode,
                                                   bool hasCullModeOverride,
                                                   const std::atomic<bool>& cancelled)
      : m_scene(scene),
        m_camera(std::move(camera)),
        m_lod(lod),
        m_viewportRect(viewportRect),
        m_cullMode(cullMode),
        m_hasCullModeOverride(hasCullModeOverride),
        m_cancelled(cancelled) {
  }

  OpenGLRasterMesh OpenGLRasterMeshBuilder::build() const {
    OpenGLRasterMesh mesh;
    if (!m_scene || !m_camera || !m_camera->viewPlane()) {
      return mesh;
    }

    m_camera->viewPlane()->setup(m_camera->matrix(), m_viewportRect);
    Rasterizer rasterizer(m_camera, std::shared_ptr<render::Scene>());
    rasterizer.setLod(m_lod);
    RasterTriangleEmitter emitter(m_scene, m_camera, m_lod, rasterizer, m_cancelled, m_cullMode,
                                  m_hasCullModeOverride, false);
    emitter.forEachTriangle([&](const RasterTriangle& triangle) {
      mesh.appendTriangle(vertexFor(triangle, triangle.vertices[0], m_viewportRect),
                          vertexFor(triangle, triangle.vertices[1], m_viewportRect),
                          vertexFor(triangle, triangle.vertices[2], m_viewportRect));
    });

    return mesh;
  }
}
