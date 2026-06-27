#pragma once

#include "render/GpuDiffusePathLoopBackend.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace render {
  struct GpuDiffusePathLoopSceneSupportReasons {
    std::string maxDepth;
    std::string geometry;
    std::string material;
    std::string texture;
    std::string light;
    std::string displayResolve;
  };

  class GpuDiffusePathLoopSceneSupport {
  public:
    [[nodiscard]] GpuDiffusePathLoopBackendSupport
    support(const GpuTracingSceneSections& scene, const GpuDiffusePathLoopSettings& settings,
            const GpuDiffusePathLoopSceneSupportReasons& reasons) const;

    [[nodiscard]] bool hasNoGeometry(const GpuTracingSceneSections& scene) const;
    [[nodiscard]] bool hasSupportedGeometry(const GpuTracingSceneSections& scene) const;
    [[nodiscard]] bool hasSupportedMaterials(const GpuTracingSceneSections& scene) const;
    [[nodiscard]] bool hasSupportedTextures(const GpuTracingSceneSections& scene) const;
    [[nodiscard]] bool hasSupportedLights(const GpuTracingSceneSections& scene) const;

  private:
    struct SupportedGeometryCounts {
      std::size_t triangles{0};
      std::size_t spheres{0};
      std::size_t planes{0};
      std::size_t rectangles{0};
      std::size_t disks{0};
      std::size_t openCylinders{0};
      std::size_t tori{0};
    };

    [[nodiscard]] bool
    primitiveUsesSupportedGeometry(const GpuIntersectionPrimitiveRecord& primitive,
                                   const GpuIntersectionSceneBuffers& geometry,
                                   SupportedGeometryCounts& counts) const;
    [[nodiscard]] bool
    materialUsesSupportedTextures(const GpuTracingSceneSections& scene,
                                  const GpuTracingMaterialRecord& material) const;
    [[nodiscard]] bool supportedTexture(const GpuTracingSceneSections& scene,
                                        std::size_t textureIndex, std::uint32_t depth = 0u) const;
    [[nodiscard]] bool supportedUntintedTexture(const GpuTracingSceneSections& scene,
                                                std::size_t textureIndex,
                                                std::uint32_t depth) const;
  };
}
