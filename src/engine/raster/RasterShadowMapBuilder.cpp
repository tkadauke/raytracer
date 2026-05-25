#include "engine/raster/detail/RasterShadowMapBuilder.h"

#include "engine/raster/detail/RasterPass.h"
#include "engine/raster/detail/RasterTriangleEmitter.h"
#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"
#include "render/TilePlan.h"
#include "render/lights/Light.h"
#include "render/primitives/Scene.h"
#include "render/viewplanes/ViewPlane.h"

#include "engine/TileRenderTask.h"

#include <algorithm>
#include <list>
#include <limits>
#include <utility>

namespace engine::raster::detail {

  RasterShadowMapBuilder::RasterShadowMapBuilder(const Rasterizer& rasterizer,
                                                 const std::shared_ptr<render::Scene>& scene,
                                                 const std::shared_ptr<render::Camera>& camera,
                                                 QThreadPool& threadPool,
                                                 const std::atomic<bool>& cancelled)
      : m_rasterizer(rasterizer),
        m_scene(scene),
        m_camera(camera),
        m_threadPool(threadPool),
        m_cancelled(cancelled) {
  }

  ShadowMaps RasterShadowMapBuilder::build() const {
    ShadowMaps shadowMaps;
    if (!canBuild()) {
      return shadowMaps;
    }

    const BoundingBoxd bounds = m_scene->boundingBox();
    if (!validBounds(bounds)) {
      return shadowMaps;
    }

    const int size = m_rasterizer.shadowMapSize();
    const auto corners = bounds.vertices();
    const auto cascadeDepths = cascadeDepthRangesFor(corners);

    for (const auto& light : m_scene->lights()) {
      if (m_cancelled.load()) {
        break;
      }

      const auto lightDirection = light ? light->directionalShadowMapDirection() : std::nullopt;
      if (!lightDirection) {
        continue;
      }

      std::vector<DirectionalShadowCascade> cascades;
      cascades.reserve(cascadeDepths.size());
      for (const auto& [cascadeMinDepth, cascadeMaxDepth] : cascadeDepths) {
        if (m_cancelled.load()) {
          break;
        }

        cascades.push_back(buildCascade(*lightDirection, corners, cascadeDepths, cascadeMinDepth,
                                        cascadeMaxDepth, size, size));
      }

      if (!cascades.empty()) {
        shadowMaps.add(
          DirectionalShadowMap(light.get(), m_camera.get(), std::move(cascades),
                               m_rasterizer.shadowBias(), m_rasterizer.shadowSlopeBias(),
                               m_rasterizer.shadowFilterRadius(), m_rasterizer.shadowFilterMode()));
      }
    }

    return shadowMaps;
  }

  bool RasterShadowMapBuilder::renderFirstDirectionalDepth(Buffer<double>& depthBuffer) const {
    depthBuffer.clear(std::numeric_limits<double>::infinity());

    if (!canBuild() || depthBuffer.width() <= 0 || depthBuffer.height() <= 0) {
      return false;
    }

    const BoundingBoxd bounds = m_scene->boundingBox();
    if (!validBounds(bounds)) {
      return false;
    }

    const auto corners = bounds.vertices();
    const auto cascadeDepths = cascadeDepthRangesFor(corners);
    if (cascadeDepths.empty()) {
      return false;
    }

    for (const auto& light : m_scene->lights()) {
      if (m_cancelled.load()) {
        return false;
      }

      const auto lightDirection = light ? light->directionalShadowMapDirection() : std::nullopt;
      if (!lightDirection) {
        continue;
      }

      const auto& [cascadeMinDepth, cascadeMaxDepth] = cascadeDepths.front();
      const auto points = cascadePoints(corners, cascadeDepths, cascadeMinDepth, cascadeMaxDepth);
      auto shadowCamera =
        cameraFor(*lightDirection, points, depthBuffer.width(), depthBuffer.height());
      renderDepth(shadowCamera, depthBuffer);
      return true;
    }

    return false;
  }

  bool RasterShadowMapBuilder::canBuild() const {
    return m_rasterizer.shadowMapsEnabled() && !m_rasterizer.fragmentShader() && m_scene &&
           m_camera && !m_cancelled.load();
  }

  bool RasterShadowMapBuilder::validBounds(const BoundingBoxd& bounds) {
    return bounds.isValid() && !bounds.isUndefined() && !bounds.isInfinite();
  }

  std::vector<std::pair<double, double>>
  RasterShadowMapBuilder::cascadeDepthRangesFor(const std::array<Vector3d, 8>& corners) const {
    const auto [minViewDepth, maxViewDepth] =
      viewDepthRange(*m_camera, corners, m_rasterizer.nearClipDepth(), m_rasterizer.farClipDepth());
    return cascadeDepthRanges(minViewDepth, maxViewDepth, m_rasterizer.shadowCascadeCount(),
                              m_rasterizer.shadowCascadeSplitLambda());
  }

  std::vector<Vector3d>
  RasterShadowMapBuilder::cascadePoints(const std::array<Vector3d, 8>& corners,
                                        const std::vector<std::pair<double, double>>& cascadeDepths,
                                        double cascadeMinDepth, double cascadeMaxDepth) const {
    if (cascadeDepths.size() == 1) {
      return {corners.begin(), corners.end()};
    }
    return cascadePointsForDepthRange(corners, *m_camera, cascadeMinDepth, cascadeMaxDepth);
  }

  DirectionalShadowCascade RasterShadowMapBuilder::buildCascade(
    const Vector3d& lightDirection, const std::array<Vector3d, 8>& corners,
    const std::vector<std::pair<double, double>>& cascadeDepths, double cascadeMinDepth,
    double cascadeMaxDepth, int width, int height) const {
    const auto points = cascadePoints(corners, cascadeDepths, cascadeMinDepth, cascadeMaxDepth);
    auto shadowCamera = cameraFor(lightDirection, points, width, height);
    auto depthBuffer = std::make_unique<Buffer<double>>(width, height);
    depthBuffer->clear(std::numeric_limits<double>::infinity());
    renderDepth(shadowCamera, *depthBuffer);
    return {std::move(shadowCamera), std::move(depthBuffer), cascadeMinDepth, cascadeMaxDepth};
  }

  std::shared_ptr<DirectionalShadowCamera>
  RasterShadowMapBuilder::cameraFor(const Vector3d& lightDirection,
                                    const std::vector<Vector3d>& points, int width,
                                    int height) const {
    const int fitSize = std::max(1, std::min(width, height));
    const auto shadowFit =
      directionalShadowFitForPoints(points, lightDirection, m_rasterizer.nearClipDepth(), fitSize);
    auto shadowCamera = std::make_shared<DirectionalShadowCamera>(shadowFit);
    shadowCamera->setViewPlane(std::make_shared<render::ViewPlane>());
    shadowCamera->viewPlane()->setup(Matrix4d(), Recti(width, height));
    return shadowCamera;
  }

  RasterTriangleSet
  RasterShadowMapBuilder::collectRasterTriangles(const RasterTriangleEmitter& triangleEmitter,
                                                 const render::TilePlan& tilePlan) const {
    RasterTriangleSet triangleSet(tilePlan);
    triangleEmitter.forEachTriangle(
      [&](const RasterTriangle& triangle) { triangleSet.add(triangle); });
    return triangleSet;
  }

  void
  RasterShadowMapBuilder::renderDepth(const std::shared_ptr<DirectionalShadowCamera>& shadowCamera,
                                      Buffer<double>& depthBuffer) const {
    std::shared_ptr<render::Camera> camera = shadowCamera;
    const render::TilePlan shadowTilePlan =
      render::TilePlan::forBuffer(depthBuffer.width(), depthBuffer.height(), 1);
    RasterTriangleEmitter shadowEmitter(m_scene.get(), camera, m_rasterizer.lod(), m_rasterizer,
                                        m_cancelled, Rasterizer::CullMode::Both, true, false);
    const RasterTriangleSet shadowTriangles = collectRasterTriangles(shadowEmitter, shadowTilePlan);
    if (shadowTriangles.empty()) {
      return;
    }

    std::list<std::shared_ptr<engine::TileRenderTask>> shadowTasks;
    rasterizeDepthOnlyTriangleSetWithPolicies(
      shadowTriangles, shadowTilePlan, m_threadPool, shadowTasks, m_cancelled, Vector2d(0.0, 0.0),
      NoStencilPolicy{},
      DepthWritePolicy<RasterFullBufferView<double>>{fullBufferView(depthBuffer),
                                                     DepthState{Rasterizer::DepthFunc::Less}});
  }

}
