#include "engine/raster/Rasterizer.h"

#include "RasterMSAA.h"
#include "RasterPass.h"
#include "RasterPipelineTypes.h"
#include "RasterShadowMaps.h"
#include "RasterTriangleEmitter.h"

#include "core/Buffer.h"
#include "core/math/Vector.h"
#include "render/TilePlan.h"
#include "render/cameras/Camera.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/Light.h"
#include "render/postprocess/Fxaa.h"
#include "render/primitives/Scene.h"
#include "render/viewplanes/ViewPlane.h"

#include "../TileRenderTask.h"

#include <QThread>
#include <QThreadPool>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <list>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

using namespace engine::raster;

namespace {
  constexpr double kMinimumRasterClipDepth = 1e-6;

  double minimumFarClipDepth(double nearDepth) {
    return std::nextafter(nearDepth, std::numeric_limits<double>::infinity());
  }
}

// Rasterizer.cpp is organized as a software graphics pipeline:
//
//   Rasterizer::render()
//     sets up the public engine state, clears the output, and hands
//     the frame to Rasterizer::Private.
//
//   Rasterizer::Private
//     owns frame-level orchestration: tile dispatch, pass sequencing,
//     MSAA path selection, shadow-map depth passes, and the active task list
//     used by the UI progress overlay.
//
//   detail headers
//     define the internal vocabulary of the pipeline: projected
//     vertices, clip vertices, raster vertices, triangle batches,
//     material evaluation, shadow maps, and the policy objects that
//     specialize the hot loop.
//
//   RasterTriangleEmitter
//     is the front end. It walks leaf primitives, tessellates them,
//     projects each mesh vertex once, clips polygons in homogeneous
//     space, applies culling/vertex-shader hooks, and streams prepared
//     RasterTriangle objects.
//
//   RasterTriangleSet / RasterTileGrid
//     is the batch. It stores the emitted triangles and bins each one
//     into the tiles whose pixel rectangles it may touch.
//
//   rasterizeTriangleSetWithPolicies()
//     is the draw step. It chooses direct single-tile rendering or
//     QRunnable tile dispatch, then calls the core edge-function
//     triangle rasterizer for each prepared triangle.
//
//   Depth/Stencil/Fragment policy objects
//     are the C++ implementation trick: runtime state such as
//     "stencil enabled?" or "custom fragment shader?" is resolved once
//     per pass into concrete policy objects, so the per-pixel loop can
//     inline pass/fail/write/shade behavior.
//
// Example path for a normal 1x single-tile render:
//   render -> Private::renderFrame -> RasterTriangleEmitter
//   -> Private::renderTriangleStreamPass -> withPreparedTrianglePolicies
//   -> rasterizePreparedTriangleWithPolicies.
//
// Example path for queued 4x MSAA:
//   renderFrame builds the same triangle set once, then renderMSAAFrame
//   renders each tile's four subpixel offsets into tile-local buffers
//   and resolves the tile directly into the output framebuffer.
//
// If you are reading for performance, start at
// rasterizePreparedTriangleWithPolicies(): that is the fragment loop.
// If you are reading for correctness of projection/clipping, start at
// RasterTriangleEmitter::forEachTriangle(). If you are reading for
// threading, start at Rasterizer::Private::renderTriangleSetPass().

namespace {
  using engine::raster::detail::accumulateMSAASample;
  using engine::raster::detail::cascadePointsForDepthRange;
  using engine::raster::detail::cascadeDepthRanges;
  using engine::raster::detail::colorOutputPolicy;
  using engine::raster::detail::DepthState;
  using engine::raster::detail::DepthWritePolicy;
  using engine::raster::detail::DirectionalShadowCamera;
  using engine::raster::detail::DirectionalShadowCascade;
  using engine::raster::detail::directionalShadowFitForPoints;
  using engine::raster::detail::DirectionalShadowMap;
  using engine::raster::detail::fullBufferView;
  using engine::raster::detail::MSAASamplePattern;
  using engine::raster::detail::MSAATileScratch;
  using engine::raster::detail::NoStencilPolicy;
  using engine::raster::detail::PassBuffers;
  using engine::raster::detail::RasterDiagnosticBufferViews;
  using engine::raster::detail::RasterFullBufferView;
  using engine::raster::detail::rasterizeDepthOnlyTriangleSetWithPolicies;
  using engine::raster::detail::rasterizePreparedTriangleWithPolicies;
  using engine::raster::detail::rasterizeTileWithPolicies;
  using engine::raster::detail::rasterizeTriangleSetWithPolicies;
  using engine::raster::detail::intersectRasterRects;
  using engine::raster::detail::rasterRectEmpty;
  using engine::raster::detail::RasterTileBufferView;
  using engine::raster::detail::RasterTriangle;
  using engine::raster::detail::RasterTriangleEmitter;
  using engine::raster::detail::RasterTriangleSet;
  using engine::raster::detail::resolveMSAA;
  using engine::raster::detail::ShadowMaps;
  using engine::raster::detail::stabilizeDirectionalShadowCenter;
  using engine::raster::detail::tileBufferView;
  using engine::raster::detail::viewDepthRange;
  using engine::raster::detail::withPreparedTrianglePolicies;

  template<class T>
  bool bufferMatches(const Buffer<T>* buffer, int width, int height) {
    return buffer && buffer->width() == width && buffer->height() == height;
  }

  template<class T>
  RasterFullBufferView<T> diagnosticView(Buffer<T>* buffer, int width, int height) {
    if (!bufferMatches(buffer, width, height))
      return RasterFullBufferView<T>();
    return fullBufferView(*buffer);
  }

  RasterDiagnosticBufferViews diagnosticViews(const Rasterizer& rasterizer, int width, int height) {
    const auto& outputs = rasterizer.diagnosticOutputBuffers();
    return {diagnosticView(outputs.depth, width, height),
            diagnosticView(outputs.normal, width, height),
            diagnosticView(outputs.primitive, width, height),
            diagnosticView(outputs.material, width, height),
            diagnosticView(outputs.face, width, height),
            diagnosticView(outputs.stencil, width, height)};
  }

  template<class T>
  void clearDiagnosticBuffer(Buffer<T>* buffer, int width, int height, const T& value) {
    if (bufferMatches(buffer, width, height)) {
      buffer->clear(value);
    }
  }

  void clearDiagnosticOutputsForRender(const Rasterizer& rasterizer, int width, int height) {
    const auto& outputs = rasterizer.diagnosticOutputBuffers();
    clearDiagnosticBuffer(outputs.depth, width, height, rasterizer.depthClearValue());
    clearDiagnosticBuffer(outputs.normal, width, height, Vector3d::undefined);
    clearDiagnosticBuffer(outputs.primitive, width, height,
                          static_cast<const render::Primitive*>(nullptr));
    clearDiagnosticBuffer(outputs.material, width, height,
                          static_cast<const render::Material*>(nullptr));
    clearDiagnosticBuffer(outputs.face, width, height, std::numeric_limits<std::uint64_t>::max());
    clearDiagnosticBuffer(outputs.stencil, width, height, rasterizer.stencilClearValue());
  }

  Recti sanitizeRasterRect(const Recti& rect) {
    return Recti(rect.left(), rect.top(), std::max(0, rect.width()), std::max(0, rect.height()));
  }

  Recti configuredViewportRect(const Rasterizer& rasterizer, const Recti& framebufferRect) {
    if (!rasterizer.viewportEnabled()) {
      return framebufferRect;
    }
    return rasterizer.viewportRect();
  }

  Recti effectiveRasterClipRect(const Rasterizer& rasterizer, const Recti& framebufferRect) {
    Recti result = intersectRasterRects(framebufferRect,
                                        configuredViewportRect(rasterizer, framebufferRect));
    if (rasterizer.scissorTestEnabled()) {
      result = intersectRasterRects(result, rasterizer.scissorRect());
    }
    return result;
  }
}

// Pimpl: hides Qt threading and render-pass orchestration from the
// public header, and gives the .cpp room for local pipeline types.
struct Rasterizer::Private {
  Private()
      : threadPool(std::make_unique<QThreadPool>()),
        queueSize(1) {
    threadPool->setMaxThreadCount(std::max(1, QThread::idealThreadCount()));
  }

  std::unique_ptr<QThreadPool> threadPool;
  std::list<std::shared_ptr<engine::TileRenderTask>> tasks;
  int queueSize;

  void renderFrame(const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
                   const std::shared_ptr<render::Camera>& camera,
                   const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

  void renderSingleSampleFrame(const Rasterizer& rasterizer,
                               const std::shared_ptr<render::Scene>& scene,
                               const render::TilePlan& tilePlan,
                               const RasterTriangleEmitter& triangleEmitter,
                               const ShadowMaps& shadowMaps, const Recti& renderClip,
                               const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

  void renderMSAAFrame(const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
                       const render::TilePlan& tilePlan, const MSAASamplePattern& pattern,
                       const RasterTriangleEmitter& triangleEmitter, const ShadowMaps& shadowMaps,
                       const Recti& renderClip, const std::atomic<bool>& cancelled,
                       Buffer<Colord>& buffer);

  void renderMSAAFullFrame(const Rasterizer& rasterizer,
                           const std::shared_ptr<render::Scene>& scene,
                           const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
                           const ShadowMaps& shadowMaps, const Recti& renderClip,
                           const MSAASamplePattern& pattern, const std::atomic<bool>& cancelled,
                           Buffer<Colord>& buffer);

  void renderMSAATile(const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
                      const RasterTriangleSet& triangleSet, const ShadowMaps& shadowMaps,
                      const Recti& renderClip, const MSAASamplePattern& pattern, const Recti& rect,
                      std::size_t tileIndex, const std::atomic<bool>& cancelled,
                      Buffer<Colord>& buffer);

  void renderTriangleSetPass(const Rasterizer& rasterizer,
                             const std::shared_ptr<render::Scene>& scene,
                             const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
                             const ShadowMaps& shadowMaps, const Recti& renderClip,
                             const std::atomic<bool>& cancelled, Buffer<Colord>& buffer,
                             const Vector2d& sampleOffset);
  void renderTriangleStreamPass(const Rasterizer& rasterizer,
                                const std::shared_ptr<render::Scene>& scene,
                                const RasterTriangleEmitter& triangleEmitter,
                                const render::TilePlan& tilePlan, const ShadowMaps& shadowMaps,
                                const Recti& renderClip, const std::atomic<bool>& cancelled,
                                Buffer<Colord>& buffer, const Vector2d& sampleOffset);

  ShadowMaps buildShadowMaps(const Rasterizer& rasterizer,
                             const std::shared_ptr<render::Scene>& scene,
                             const std::shared_ptr<render::Camera>& camera,
                             const std::atomic<bool>& cancelled);

  static RasterTriangleSet collectRasterTriangles(const RasterTriangleEmitter& triangleEmitter,
                                                  const render::TilePlan& tilePlan);
};

Rasterizer::Rasterizer(std::shared_ptr<render::Scene> scene)
    : RenderEngine(std::move(scene)),
      p(std::make_unique<Private>()) {
}

Rasterizer::Rasterizer(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene)
    : RenderEngine(std::move(camera), std::move(scene)),
      p(std::make_unique<Private>()) {
}

Rasterizer::~Rasterizer() = default;

std::shared_ptr<render::RenderEngine> Rasterizer::cloneForRender() const {
  auto result = std::make_shared<Rasterizer>(m_camera ? m_camera->clone() : nullptr, m_scene);
  result->setTonemap(tonemap());
  result->setLod(m_lod);
  result->setMaximumThreads(p->threadPool->maxThreadCount());
  result->setQueueSize(p->queueSize);
  result->setMSAASamples(m_msaaSamples);
  result->setNearClipDepth(m_nearClipDepth);
  result->setFarClipDepth(m_farClipDepth);
  result->setPostProcessAA(m_postProcessAA);
  result->setShadowMapsEnabled(m_shadowMapsEnabled);
  result->setShadowMapSize(m_shadowMapSize);
  result->setShadowCascadeCount(m_shadowCascadeCount);
  result->setShadowCascadeSplitLambda(m_shadowCascadeSplitLambda);
  result->setShadowBias(m_shadowBias);
  result->setShadowSlopeBias(m_shadowSlopeBias);
  result->setShadowFilterRadius(m_shadowFilterRadius);
  result->setShadowFilterMode(m_shadowFilterMode);
  result->setCullMode(m_cullMode);
  result->m_viewportEnabled = m_viewportEnabled;
  result->m_viewportRect = m_viewportRect;
  result->m_scissorTestEnabled = m_scissorTestEnabled;
  result->m_scissorRect = m_scissorRect;
  result->setDepthFunc(m_depthFunc);
  result->setDepthClearValue(m_depthClearValue);
  result->setDepthWriteEnabled(m_depthWriteEnabled);
  result->setStencilTestEnabled(m_stencilTestEnabled);
  result->setStencilFunc(m_stencilFunc, m_stencilReference, m_stencilMask);
  result->setStencilClearValue(m_stencilClearValue);
  result->setStencilWriteMask(m_stencilWriteMask);
  result->setStencilOps(m_stencilFailOp, m_stencilDepthFailOp, m_stencilPassOp);
  result->setColorWriteMask(m_colorWriteMask);
  result->setBlendingEnabled(m_blendingEnabled);
  result->setBlendFactors(m_sourceBlendFactor, m_destinationBlendFactor);
  result->setBlendOp(m_blendOp);
  result->setBlendConstant(m_blendConstantColor, m_blendConstantAlpha);
  result->setVertexShader(m_vertexShader);
  result->setFragmentShader(m_fragmentShader);
  if (hasBackgroundColorOverride()) {
    result->setBackgroundColor(backgroundColor());
  }
  return result;
}

void Rasterizer::cancel() {
  m_cancelled.store(true);
}

void Rasterizer::uncancel() {
  m_cancelled.store(false);
}

std::list<Recti> Rasterizer::activeTiles() const {
  std::list<Recti> result;
  for (const auto& task : p->tasks) {
    if (task->active.load(std::memory_order_acquire)) {
      result.push_back(task->rect);
    }
  }
  return result;
}

void Rasterizer::setMaximumThreads(int threads) {
  p->threadPool->setMaxThreadCount(std::max(1, threads));
}

void Rasterizer::setQueueSize(int queue) {
  p->queueSize = std::max(1, queue);
}

void Rasterizer::setViewportRect(const Recti& rect) {
  m_viewportRect = sanitizeRasterRect(rect);
  m_viewportEnabled = true;
}

void Rasterizer::clearViewportRect() {
  m_viewportRect = Recti();
  m_viewportEnabled = false;
}

void Rasterizer::setScissorRect(const Recti& rect) {
  m_scissorRect = sanitizeRasterRect(rect);
  m_scissorTestEnabled = true;
}

void Rasterizer::clearScissorRect() {
  m_scissorRect = Recti();
  m_scissorTestEnabled = false;
}

void Rasterizer::setMSAASamples(int samples) {
  if (samples <= 1) {
    m_msaaSamples = 1;
  } else if (samples <= 2) {
    m_msaaSamples = 2;
  } else if (samples <= 4) {
    m_msaaSamples = 4;
  } else {
    m_msaaSamples = 8;
  }
}

void Rasterizer::setNearClipDepth(double depth) {
  m_nearClipDepth =
    std::isfinite(depth) ? std::max(kMinimumRasterClipDepth, depth) : kMinimumRasterClipDepth;
  if (std::isfinite(m_farClipDepth) && m_farClipDepth <= m_nearClipDepth) {
    m_farClipDepth = minimumFarClipDepth(m_nearClipDepth);
  }
}

void Rasterizer::setFarClipDepth(double depth) {
  if (!std::isfinite(depth)) {
    m_farClipDepth = std::numeric_limits<double>::infinity();
    return;
  }
  m_farClipDepth = std::max(depth, minimumFarClipDepth(m_nearClipDepth));
}

void Rasterizer::setShadowMapSize(int size) {
  m_shadowMapSize = std::max(1, size);
}

void Rasterizer::render(Buffer<Colord>& buffer) {
  // Caller is expected to call uncancel() between renders. Matches
  // the Wireframe / Raytracer convention.

  // Clear to the configured background before depth-tested fragments overwrite it.
  buffer.clear(backgroundColor());
  clearDiagnosticOutputsForRender(*this, buffer.width(), buffer.height());

  if (!m_scene || !m_camera)
    return;

  // Same view-plane setup the other engines perform — the camera
  // projection math depends on the cached basis vectors.
  const Recti viewport = configuredViewportRect(*this, buffer.rect());
  if (rasterRectEmpty(viewport))
    return;
  m_camera->viewPlane()->setup(m_camera->matrix(), viewport);

  // From here down the render is expressed in pipeline terms. The
  // Rasterizer object contributes configuration; Private drives the
  // concrete passes and keeps task state available for activeTiles().
  p->tasks.clear();
  p->renderFrame(*this, m_scene, m_camera, m_cancelled, buffer);
}

RasterTriangleSet
Rasterizer::Private::collectRasterTriangles(const RasterTriangleEmitter& triangleEmitter,
                                            const render::TilePlan& tilePlan) {
  // The emitter streams triangles, the set owns them and their tile
  // bins. Keeping those roles separate makes the later tile raster
  // pass independent of scene traversal and tessellation.
  RasterTriangleSet triangleSet(tilePlan);
  triangleEmitter.forEachTriangle(
    [&](const RasterTriangle& triangle) { triangleSet.add(triangle); });
  return triangleSet;
}

ShadowMaps Rasterizer::Private::buildShadowMaps(const Rasterizer& rasterizer,
                                                const std::shared_ptr<render::Scene>& scene,
                                                const std::shared_ptr<render::Camera>& camera,
                                                const std::atomic<bool>& cancelled) {
  ShadowMaps shadowMaps;
  if (!rasterizer.shadowMapsEnabled() || rasterizer.fragmentShader() || !camera || cancelled.load())
    return shadowMaps;

  const BoundingBoxd bounds = scene->boundingBox();
  if (!bounds.isValid() || bounds.isUndefined() || bounds.isInfinite())
    return shadowMaps;

  const int size = rasterizer.shadowMapSize();
  const auto corners = bounds.vertices();
  const auto [minViewDepth, maxViewDepth] =
    viewDepthRange(*camera, corners, rasterizer.nearClipDepth(), rasterizer.farClipDepth());
  const auto cascadeDepths = cascadeDepthRanges(
    minViewDepth, maxViewDepth, rasterizer.shadowCascadeCount(),
    rasterizer.shadowCascadeSplitLambda());

  for (const auto& light : scene->lights()) {
    if (cancelled.load())
      break;

    auto directional = std::dynamic_pointer_cast<render::DirectionalLight>(light);
    if (!directional)
      continue;

    std::vector<DirectionalShadowCascade> cascades;
    cascades.reserve(cascadeDepths.size());
    for (const auto& [cascadeMinDepth, cascadeMaxDepth] : cascadeDepths) {
      if (cancelled.load())
        break;

      std::vector<Vector3d> cascadePoints;
      if (cascadeDepths.size() == 1) {
        cascadePoints.assign(corners.begin(), corners.end());
      } else {
        cascadePoints =
          cascadePointsForDepthRange(corners, *camera, cascadeMinDepth, cascadeMaxDepth);
      }
      const auto shadowFit = directionalShadowFitForPoints(
        cascadePoints, directional->direction(), rasterizer.nearClipDepth(), size);
      auto shadowCamera = std::make_shared<DirectionalShadowCamera>(shadowFit);
      shadowCamera->setViewPlane(std::make_shared<render::ViewPlane>());
      shadowCamera->viewPlane()->setup(Matrix4d(), Recti(size, size));

      auto depthBuffer = std::make_unique<Buffer<double>>(size, size);
      depthBuffer->clear(std::numeric_limits<double>::infinity());

      const render::TilePlan shadowTilePlan = render::TilePlan::forBuffer(size, size, 1);
      RasterTriangleEmitter shadowEmitter(scene.get(), shadowCamera, rasterizer.lod(), rasterizer,
                                          cancelled, Rasterizer::CullMode::Both, false);
      const RasterTriangleSet shadowTriangles =
        collectRasterTriangles(shadowEmitter, shadowTilePlan);
      if (!shadowTriangles.empty()) {
        std::list<std::shared_ptr<engine::TileRenderTask>> shadowTasks;
        rasterizeDepthOnlyTriangleSetWithPolicies(
          shadowTriangles, shadowTilePlan, *threadPool, shadowTasks, cancelled, Vector2d(0.0, 0.0),
          NoStencilPolicy{},
          DepthWritePolicy<RasterFullBufferView<double>>{fullBufferView(*depthBuffer),
                                                         DepthState{Rasterizer::DepthFunc::Less}});
      }

      cascades.push_back(
        {std::move(shadowCamera), std::move(depthBuffer), cascadeMinDepth, cascadeMaxDepth});
    }

    if (!cascades.empty()) {
      shadowMaps.add(DirectionalShadowMap(light.get(), camera.get(), std::move(cascades),
                                          rasterizer.shadowBias(), rasterizer.shadowSlopeBias(),
                                          rasterizer.shadowFilterRadius(),
                                          rasterizer.shadowFilterMode()));
    }
  }

  return shadowMaps;
}

void Rasterizer::Private::renderTriangleSetPass(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
  const ShadowMaps& shadowMaps, const Recti& renderClip, const std::atomic<bool>& cancelled,
  Buffer<Colord>& buffer, const Vector2d& sampleOffset) {
  // A pass freezes current depth/stencil/shader state into policies,
  // then hands the triangle set to either the direct or tiled path.
  PassBuffers passBuffers(rasterizer, tilePlan, buffer);
  RasterFullBufferView<std::uint8_t> stencilView;
  if (passBuffers.stencil()) {
    stencilView = fullBufferView(*passBuffers.stencil());
  }
  const RasterDiagnosticBufferViews diagnostics =
    diagnosticViews(rasterizer, tilePlan.width(), tilePlan.height());
  withPreparedTrianglePolicies(
    scene.get(), rasterizer, shadowMaps, fullBufferView(passBuffers.depth()), stencilView,
    [&](auto stencil, auto depth, auto fragmentPolicy) {
      rasterizeTriangleSetWithPolicies(triangleSet, tilePlan, *threadPool, tasks, cancelled,
                                       colorOutputPolicy(rasterizer,
                                                         fullBufferView(passBuffers.color())),
                                       renderClip, sampleOffset, stencil, depth, fragmentPolicy,
                                       diagnostics);
    });
}

void Rasterizer::Private::renderTriangleStreamPass(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const RasterTriangleEmitter& triangleEmitter, const render::TilePlan& tilePlan,
  const ShadowMaps& shadowMaps, const Recti& renderClip, const std::atomic<bool>& cancelled,
  Buffer<Colord>& buffer, const Vector2d& sampleOffset) {
  // The ordinary 1x single-tile path does not need a retained triangle
  // batch or tile bins. Freeze render state once, then draw each emitted
  // triangle immediately into the full-frame pass buffers.
  PassBuffers passBuffers(rasterizer, tilePlan, buffer);
  auto colorView = colorOutputPolicy(rasterizer, fullBufferView(passBuffers.color()));
  auto depthView = fullBufferView(passBuffers.depth());
  RasterFullBufferView<std::uint8_t> stencilView;
  if (passBuffers.stencil()) {
    stencilView = fullBufferView(*passBuffers.stencil());
  }
  const RasterDiagnosticBufferViews diagnostics =
    diagnosticViews(rasterizer, tilePlan.width(), tilePlan.height());

  const Recti clipRect = intersectRasterRects(tilePlan.fullRect(), renderClip);
  if (rasterRectEmpty(clipRect))
    return;

  withPreparedTrianglePolicies(
    scene.get(), rasterizer, shadowMaps, depthView, stencilView,
    [&](auto stencil, auto depth, auto fragmentPolicy) {
      triangleEmitter.forEachTriangle([&](const RasterTriangle& triangle) {
        if (cancelled.load())
          return;
        rasterizePreparedTriangleWithPolicies(triangle, clipRect, colorView, sampleOffset, stencil,
                                              depth, fragmentPolicy, diagnostics);
      });
    });
}

void Rasterizer::Private::renderSingleSampleFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& tilePlan, const RasterTriangleEmitter& triangleEmitter,
  const ShadowMaps& shadowMaps, const Recti& renderClip, const std::atomic<bool>& cancelled,
  Buffer<Colord>& buffer) {
  if (tilePlan.isSingleTile()) {
    renderTriangleStreamPass(rasterizer, scene, triangleEmitter, tilePlan, shadowMaps, renderClip,
                             cancelled, buffer, Vector2d(0.0, 0.0));
    return;
  }

  const RasterTriangleSet triangleSet = collectRasterTriangles(triangleEmitter, tilePlan);
  if (cancelled.load() || triangleSet.empty())
    return;

  renderTriangleSetPass(rasterizer, scene, triangleSet, tilePlan, shadowMaps, renderClip, cancelled,
                        buffer, Vector2d(0.0, 0.0));
}

void Rasterizer::Private::renderMSAAFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& tilePlan, const MSAASamplePattern& pattern,
  const RasterTriangleEmitter& triangleEmitter, const ShadowMaps& shadowMaps, const Recti& renderClip,
  const std::atomic<bool>& cancelled, Buffer<Colord>& buffer) {
  const RasterTriangleSet triangleSet = collectRasterTriangles(triangleEmitter, tilePlan);
  if (cancelled.load() || triangleSet.empty())
    return;

  if (tilePlan.isSingleTile()) {
    renderMSAAFullFrame(rasterizer, scene, triangleSet, tilePlan, shadowMaps, renderClip, pattern,
                        cancelled, buffer);
    return;
  }

  engine::dispatchTileTasks(tilePlan, *threadPool, tasks,
                            [&](const Recti& rect, std::size_t tileIndex) {
                              renderMSAATile(rasterizer, scene, triangleSet, shadowMaps, renderClip,
                                             pattern, rect, tileIndex, cancelled, buffer);
                            });
}

void Rasterizer::Private::renderMSAAFullFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
  const ShadowMaps& shadowMaps, const Recti& renderClip, const MSAASamplePattern& pattern,
  const std::atomic<bool>& cancelled, Buffer<Colord>& buffer) {
  buffer.clear(Colord::black());
  for (int sampleIndex = 0; sampleIndex != pattern.count; ++sampleIndex) {
    if (cancelled.load())
      return;

    Buffer<Colord> sampleBuffer(tilePlan.width(), tilePlan.height());
    sampleBuffer.clear(rasterizer.backgroundColor());

    renderTriangleSetPass(rasterizer, scene, triangleSet, tilePlan, shadowMaps, renderClip, cancelled,
                          sampleBuffer, pattern.offsets[sampleIndex]);

    if (cancelled.load())
      return;
    accumulateMSAASample(buffer, sampleBuffer);
  }

  resolveMSAA(buffer, pattern.count);
}

void Rasterizer::Private::renderMSAATile(const Rasterizer& rasterizer,
                                         const std::shared_ptr<render::Scene>& scene,
                                         const RasterTriangleSet& triangleSet,
                                         const ShadowMaps& shadowMaps, const Recti& renderClip,
                                         const MSAASamplePattern& pattern, const Recti& rect,
                                         std::size_t tileIndex, const std::atomic<bool>& cancelled,
                                         Buffer<Colord>& buffer) {
  if (rect.width() <= 0 || rect.height() <= 0)
    return;

  MSAATileScratch scratch(rasterizer, rect);

  for (int sampleIndex = 0; sampleIndex != pattern.count; ++sampleIndex) {
    if (cancelled.load())
      return;

    // This is simple supersampling scoped to one tile: rerun
    // coverage/depth at a fixed subpixel offset, accumulate local
    // colors, and resolve the tile into the output framebuffer.
    scratch.clearSample(rasterizer);

    RasterTileBufferView<std::uint8_t> stencilView;
    if (scratch.stencil()) {
      stencilView = tileBufferView(*scratch.stencil(), rect);
    }
    const RasterDiagnosticBufferViews diagnostics =
      diagnosticViews(rasterizer, buffer.width(), buffer.height());

    withPreparedTrianglePolicies(
      scene.get(), rasterizer, shadowMaps, tileBufferView(scratch.depth(), rect), stencilView,
      [&](auto stencil, auto depth, auto fragmentPolicy) {
        rasterizeTileWithPolicies(
          triangleSet, rect, tileIndex,
          colorOutputPolicy(rasterizer, tileBufferView(scratch.sampleColor(), rect)), renderClip,
          pattern.offsets[sampleIndex], cancelled, stencil, depth, fragmentPolicy, diagnostics);
      });

    if (cancelled.load())
      return;
    scratch.accumulateSample();
  }

  scratch.resolveTo(buffer, pattern.count);
}

void Rasterizer::Private::renderFrame(const Rasterizer& rasterizer,
                                      const std::shared_ptr<render::Scene>& scene,
                                      const std::shared_ptr<render::Camera>& camera,
                                      const std::atomic<bool>& cancelled, Buffer<Colord>& buffer) {
  const int width = buffer.width();
  const int height = buffer.height();

  if (width <= 0 || height <= 0 || cancelled.load())
    return;

  const Recti renderClip = effectiveRasterClipRect(rasterizer, buffer.rect());
  if (rasterRectEmpty(renderClip))
    return;

  const render::TilePlan tilePlan = render::TilePlan::forBuffer(width, height, queueSize);
  const MSAASamplePattern pattern(rasterizer.msaaSamples());
  const RasterTriangleEmitter triangleEmitter(scene.get(), camera, rasterizer.lod(), rasterizer,
                                              cancelled, rasterizer.cullMode(), true);
  const ShadowMaps shadowMaps = buildShadowMaps(rasterizer, scene, camera, cancelled);
  if (pattern.count > 1) {
    renderMSAAFrame(rasterizer, scene, tilePlan, pattern, triangleEmitter, shadowMaps, renderClip,
                    cancelled, buffer);
  } else {
    renderSingleSampleFrame(rasterizer, scene, tilePlan, triangleEmitter, shadowMaps, renderClip,
                            cancelled, buffer);
  }

  if (!cancelled.load() && rasterizer.postProcessAA() == Rasterizer::PostProcessAA::FXAA) {
    render::postprocess::applyFxaa(buffer);
  }
}
