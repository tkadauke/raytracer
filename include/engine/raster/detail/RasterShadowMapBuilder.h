#pragma once

#include "engine/raster/detail/RasterShadowMaps.h"

#include <array>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

template<class T>
class Buffer;

class QThreadPool;

namespace render {
  class Camera;
  class DirectionalLight;
  class Scene;
  class TilePlan;
}

namespace engine::raster {
  class Rasterizer;
}

namespace engine::raster::detail {
  class RasterTriangleEmitter;
  class RasterTriangleSet;

  class RasterShadowMapBuilder {
  public:
    RasterShadowMapBuilder(const Rasterizer& rasterizer,
                           const std::shared_ptr<render::Scene>& scene,
                           const std::shared_ptr<render::Camera>& camera, QThreadPool& threadPool,
                           const std::atomic<bool>& cancelled);

    ShadowMaps build() const;
    bool renderFirstDirectionalDepth(Buffer<double>& depthBuffer) const;

  private:
    bool canBuild() const;
    static bool validBounds(const BoundingBoxd& bounds);

    std::vector<std::pair<double, double>>
    cascadeDepthRangesFor(const std::array<Vector3d, 8>& corners) const;

    std::vector<Vector3d> cascadePoints(const std::array<Vector3d, 8>& corners,
                                        const std::vector<std::pair<double, double>>& cascadeDepths,
                                        double cascadeMinDepth, double cascadeMaxDepth) const;

    DirectionalShadowCascade
    buildCascade(const render::DirectionalLight& light, const std::array<Vector3d, 8>& corners,
                 const std::vector<std::pair<double, double>>& cascadeDepths,
                 double cascadeMinDepth, double cascadeMaxDepth, int width, int height) const;

    std::shared_ptr<DirectionalShadowCamera> cameraFor(const render::DirectionalLight& light,
                                                       const std::vector<Vector3d>& points,
                                                       int width, int height) const;

    RasterTriangleSet collectRasterTriangles(const RasterTriangleEmitter& triangleEmitter,
                                             const render::TilePlan& tilePlan) const;

    void renderDepth(const std::shared_ptr<DirectionalShadowCamera>& shadowCamera,
                     Buffer<double>& depthBuffer) const;

    const Rasterizer& m_rasterizer;
    std::shared_ptr<render::Scene> m_scene;
    std::shared_ptr<render::Camera> m_camera;
    QThreadPool& m_threadPool;
    const std::atomic<bool>& m_cancelled;
  };

}
