#pragma once

#include "core/Color.h"
#include "core/math/Vector.h"
#include "render/GpuIntersectionScene.h"

#include <cstdint>

namespace render {
  struct GpuTracingSceneSections;
  struct GpuTracingTextureRecord;

  class GpuTracingTextureEvaluator {
  public:
    explicit GpuTracingTextureEvaluator(const GpuTracingSceneSections& scene);

    [[nodiscard]] Colord evaluate(std::uint32_t textureId,
                                  const GpuIntersectionHitRecord& hit) const;

  private:
    static constexpr std::uint32_t maxEvaluationDepth = 8;

    [[nodiscard]] Colord evaluate(std::uint32_t textureId, const GpuIntersectionHitRecord& hit,
                                  std::uint32_t depth) const;
    [[nodiscard]] Colord imageTexelColor(const GpuTracingTextureRecord& texture, int x, int y,
                                         int width, const GpuIntersectionHitRecord& hit,
                                         std::uint32_t depth) const;
    [[nodiscard]] static Vector2d textureCoordinates(const GpuTracingTextureRecord& texture,
                                                     const GpuIntersectionHitRecord& hit);
    [[nodiscard]] static double normalizedTextureCoordinate(const GpuTracingTextureRecord& texture,
                                                            double coordinate);
    [[nodiscard]] static int imageTextureWrappedCoordinate(const GpuTracingTextureRecord& texture,
                                                           int coordinate, int size);
    [[nodiscard]] static int imageTextureCoordinate(const GpuTracingTextureRecord& texture,
                                                    double coordinate, int size);

    const GpuTracingSceneSections& m_scene;
  };
}
