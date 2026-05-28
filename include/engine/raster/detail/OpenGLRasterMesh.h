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
  class Scene;
}

namespace engine::raster::detail {

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
      float r{0.0f};
      float g{0.0f};
      float b{0.0f};
      float a{1.0f};
      float u{0.0f};
      float v{0.0f};
      float alphaScale{1.0f};
      float albedoMode{0.0f};
    };

    struct Batch {
      std::size_t indexOffset{0};
      std::size_t indexCount{0};
      RasterAlbedoShaderSource albedo;
    };

    using Vertices = std::vector<Vertex>;
    using Indices = std::vector<std::uint32_t>;
    using Batches = std::vector<Batch>;

    bool empty() const;
    std::size_t triangleCount() const;
    const Vertices& vertices() const;
    const Indices& indices() const;
    const Batches& batches() const;

    void appendTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2,
                        const RasterAlbedoShaderSource& albedo);

  private:
    Vertices m_vertices;
    Indices m_indices;
    Batches m_batches;
  };

  class OpenGLRasterMeshBuilder {
  public:
    OpenGLRasterMeshBuilder(const render::Scene* scene, std::shared_ptr<render::Camera> camera,
                            int lod, const Recti& viewportRect, Rasterizer::CullMode cullMode,
                            bool hasCullModeOverride, const std::atomic<bool>& cancelled);

    OpenGLRasterMesh build() const;

  private:
    const render::Scene* m_scene;
    std::shared_ptr<render::Camera> m_camera;
    int m_lod;
    Recti m_viewportRect;
    Rasterizer::CullMode m_cullMode;
    bool m_hasCullModeOverride;
    const std::atomic<bool>& m_cancelled;
  };

}
