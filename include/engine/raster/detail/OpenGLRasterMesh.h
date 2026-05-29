#pragma once

#include "core/Color.h"
#include "core/math/Rect.h"
#include "engine/raster/Rasterizer.h"
#include "engine/raster/detail/RasterMaterial.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace render {
  class Camera;
  class Light;
  class Scene;
}

namespace engine::raster::detail {
  struct RasterTriangle;
  struct RasterVertex;
  class ShadowMaps;

  /**
    * Screen-space triangle buffer prepared for the initial OpenGL raster path.
    *
    * The first OpenGL backend slice deliberately reuses the software
    * rasterizer's scene traversal, tessellation, clipping, culling, and
    * material classification. This object is the handoff from that shared
    * front end to the GPU upload path: packed vertices plus an index buffer.
    */
  class OpenGLRasterMesh {
  public:
    struct Vertex {
      float x{0.0f};
      float y{0.0f};
      float z{0.0f};
      float w{1.0f};
      float worldX{0.0f};
      float worldY{0.0f};
      float worldZ{0.0f};
      float normalX{0.0f};
      float normalY{0.0f};
      float normalZ{1.0f};
      float r{0.0f};
      float g{0.0f};
      float b{0.0f};
      float a{1.0f};
      float u{0.0f};
      float v{0.0f};
      float alphaScale{1.0f};
      float materialDiffuse{1.0f};
      float materialSpecularR{0.0f};
      float materialSpecularG{0.0f};
      float materialSpecularB{0.0f};
      float materialSpecularCoefficient{0.0f};
      float materialSpecularExponent{16.0f};
      float ambientR{1.0f};
      float ambientG{1.0f};
      float ambientB{1.0f};
      float directR{0.0f};
      float directG{0.0f};
      float directB{0.0f};
      float specularR{0.0f};
      float specularG{0.0f};
      float specularB{0.0f};
      float albedoMode{0.0f};
    };

    struct Batch {
      std::size_t indexOffset{0};
      std::size_t indexCount{0};
      RasterAlbedoShaderSource albedo;
    };

    struct DirectionalLight {
      float directionX{0.0f};
      float directionY{0.0f};
      float directionZ{1.0f};
      float radianceR{0.0f};
      float radianceG{0.0f};
      float radianceB{0.0f};
    };

    struct PointLight {
      float positionX{0.0f};
      float positionY{0.0f};
      float positionZ{0.0f};
      float radianceR{0.0f};
      float radianceG{0.0f};
      float radianceB{0.0f};
    };

    using Vertices = std::vector<Vertex>;
    using Indices = std::vector<std::uint32_t>;
    using Batches = std::vector<Batch>;
    using DirectionalLights = std::vector<DirectionalLight>;
    using PointLights = std::vector<PointLight>;

    bool empty() const;
    std::size_t triangleCount() const;
    const Vertices& vertices() const;
    const Indices& indices() const;
    const Batches& batches() const;
    const DirectionalLights& directionalLights() const;
    const PointLights& pointLights() const;

    void appendTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2,
                        const RasterAlbedoShaderSource& albedo);
    void addDirectionalLight(const DirectionalLight& light);
    void addPointLight(const PointLight& light);

  private:
    Vertices m_vertices;
    Indices m_indices;
    Batches m_batches;
    DirectionalLights m_directionalLights;
    PointLights m_pointLights;
  };

  class OpenGLRasterMeshBuilder {
  public:
    OpenGLRasterMeshBuilder(const render::Scene* scene, std::shared_ptr<render::Camera> camera,
                            int lod, const Recti& viewportRect, Rasterizer::CullMode cullMode,
                            bool hasCullModeOverride, const std::atomic<bool>& cancelled,
                            const ShadowMaps* shadowMaps = nullptr);

    OpenGLRasterMesh build() const;

  private:
    const render::Scene* m_scene;
    std::shared_ptr<render::Camera> m_camera;
    int m_lod;
    Recti m_viewportRect;
    Rasterizer::CullMode m_cullMode;
    bool m_hasCullModeOverride;
    const std::atomic<bool>& m_cancelled;
    const ShadowMaps* m_shadowMaps;

    void appendDirectionalLights(OpenGLRasterMesh& mesh) const;
    void appendPointLights(OpenGLRasterMesh& mesh) const;
    OpenGLRasterMesh::Vertex vertexFor(const RasterTriangle& triangle,
                                       const RasterVertex& vertex) const;
    bool usesFragmentShaderLighting() const;
    bool shadesInFragmentShader(const render::Light& light) const;
    Vector3d lightingNormalFor(const RasterTriangle& triangle, const RasterVertex& vertex) const;
    Colord ambientLightingFor(const RasterTriangle& triangle) const;
    Colord directLightingFor(const RasterTriangle& triangle, const RasterVertex& vertex,
                             const Vector3d& normal) const;
    Colord specularFor(const RasterTriangle& triangle, const RasterVertex& vertex,
                       const Vector3d& normal) const;
    double visibilityFor(const render::Light& light, const RasterVertex& vertex,
                         const Vector3d& normal, const Vector3d& lightDir) const;
  };

}
