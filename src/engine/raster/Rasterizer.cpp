#include "engine/raster/Rasterizer.h"

#include "RasterMSAA.h"
#include "RasterPass.h"
#include "RasterPipelineTypes.h"
#include "RasterShadowMaps.h"
#include "RasterTemporalResources.h"
#include "RasterTriangleEmitter.h"

#include "core/Buffer.h"
#include "core/math/Vector.h"
#include "render/TilePlan.h"
#include "render/cameras/Camera.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/Light.h"
#include "render/postprocess/Fxaa.h"
#include "render/postprocess/Smaa.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/Tonemap.h"
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
  using engine::raster::detail::AlphaTestState;
  using engine::raster::detail::DirectionalShadowCascade;
  using engine::raster::detail::directionalShadowFitForPoints;
  using engine::raster::detail::DirectionalShadowMap;
  using engine::raster::detail::copyRasterBuffer;
  using engine::raster::detail::fullBufferView;
  using engine::raster::detail::MSAAFragmentShadeCache;
  using engine::raster::detail::MSAASamplePattern;
  using engine::raster::detail::MSAATileScratch;
  using engine::raster::detail::NoStencilPolicy;
  using engine::raster::detail::PassBuffers;
  using engine::raster::detail::rasterBufferMatches;
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
  using engine::raster::detail::TemporalJitter;
  using engine::raster::detail::TemporalResetCondition;
  using engine::raster::detail::TemporalResourceContract;
  using engine::raster::detail::validateTemporalResourceContract;
  using engine::raster::detail::viewDepthRange;
  using engine::raster::detail::withMSAAFragmentShadingPolicy;
  using engine::raster::detail::withPreparedTrianglePolicies;

  template<class T>
  bool bufferMatches(const Buffer<T>* buffer, int width, int height) {
    return rasterBufferMatches(buffer, width, height);
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

  void loadColorAttachment(const Rasterizer& rasterizer, Buffer<Colord>& target,
                           const Buffer<Colord>& source) {
    if (rasterizer.colorLoadOp() == Rasterizer::AttachmentLoadOp::Load) {
      if (&target != &source) {
        copyRasterBuffer(target, source);
      }
    } else {
      target.clear(rasterizer.backgroundColor());
    }
  }

  Colord colorFromPackedRgb(unsigned int rgb) {
    return Colord(static_cast<double>((rgb >> 16) & 0xFF) / 255.0,
                  static_cast<double>((rgb >> 8) & 0xFF) / 255.0,
                  static_cast<double>(rgb & 0xFF) / 255.0);
  }

  void loadColorAttachmentFromDisplay(const Rasterizer& rasterizer, Buffer<Colord>& target,
                                      const Buffer<unsigned int>& source) {
    if (rasterizer.colorLoadOp() == Rasterizer::AttachmentLoadOp::Load) {
      for (int y = 0; y != target.height(); ++y)
        for (int x = 0; x != target.width(); ++x)
          target[y][x] = colorFromPackedRgb(source[y][x]);
    } else {
      target.clear(rasterizer.backgroundColor());
    }
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

  double halton(int index, int base) {
    double f = 1.0;
    double result = 0.0;
    while (index > 0) {
      f /= static_cast<double>(base);
      result += f * static_cast<double>(index % base);
      index /= base;
    }
    return result;
  }

  TemporalJitter temporalJitterForFrame(int frame) {
    const int index = frame + 1;
    return {halton(index, 2) - 0.5, halton(index, 3) - 0.5};
  }

  void clearTemporalMotionVectors(Buffer<Vector2d>& motionVectors, const TemporalJitter& current,
                                  const TemporalJitter& previous) {
    const Vector2d delta(current.x - previous.x, current.y - previous.y);
    motionVectors.clear(delta);
  }

  bool finiteDepth(double depth) {
    return std::isfinite(depth);
  }

  Colord sampleHistoryColor(const Buffer<Colord>& history, double x, double y) {
    const int sx = static_cast<int>(std::round(x));
    const int sy = static_cast<int>(std::round(y));
    if (sx < 0 || sy < 0 || sx >= history.width() || sy >= history.height()) {
      return Colord::black();
    }
    return history[sy][sx];
  }

  bool historySampleUsable(const Buffer<double>& historyDepth, double x, double y,
                           double currentDepth) {
    const int sx = static_cast<int>(std::round(x));
    const int sy = static_cast<int>(std::round(y));
    if (sx < 0 || sy < 0 || sx >= historyDepth.width() || sy >= historyDepth.height()) {
      return false;
    }
    const double previousDepth = historyDepth[sy][sx];
    if (!finiteDepth(currentDepth) || !finiteDepth(previousDepth)) {
      return false;
    }
    return std::abs(previousDepth - currentDepth) <= std::max(1e-4, currentDepth * 0.02);
  }

  void applyTemporalAccumulation(const TemporalResourceContract& contract,
                                 Buffer<Colord>& currentColor, double currentFrameWeight) {
    const auto validation =
      validateTemporalResourceContract(contract, currentColor.width(), currentColor.height());
    const double alpha = std::clamp(currentFrameWeight, 0.0, 1.0);

    for (int y = 0; y != currentColor.height(); ++y) {
      for (int x = 0; x != currentColor.width(); ++x) {
        Colord resolved = currentColor[y][x];
        if (validation.canAccumulate) {
          const Vector2d motion = (*contract.motionVectors)[y][x];
          const double hx = static_cast<double>(x) - motion.x();
          const double hy = static_cast<double>(y) - motion.y();
          if (historySampleUsable(*contract.historyDepth, hx, hy, (*contract.currentDepth)[y][x])) {
            const Colord history = sampleHistoryColor(*contract.historyColor, hx, hy);
            resolved = history * (1.0 - alpha) + currentColor[y][x] * alpha;
          }
        }
        currentColor[y][x] = resolved;
        (*contract.nextHistoryColor)[y][x] = resolved;
      }
    }
  }

  void copyDepthHistory(Buffer<double>& target, const Buffer<double>& source) {
    copyRasterBuffer(target, source);
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
  std::unique_ptr<Buffer<Colord>> historyColor;
  std::unique_ptr<Buffer<Colord>> nextHistoryColor;
  std::unique_ptr<Buffer<double>> historyDepth;
  std::unique_ptr<Buffer<double>> currentDepth;
  std::unique_ptr<Buffer<Vector2d>> motionVectors;
  TemporalJitter previousJitter;
  bool temporalHistoryValid{false};
  bool temporalInvalidated{false};
  TemporalResetCondition pendingTemporalReset{TemporalResetCondition::FirstFrame};
  int temporalFrameIndex{0};
  const render::Camera* temporalCamera{nullptr};
  const render::Scene* temporalScene{nullptr};

  void renderFrame(const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
                   const std::shared_ptr<render::Camera>& camera,
                   const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

  void renderSingleSampleFrame(const Rasterizer& rasterizer,
                               const std::shared_ptr<render::Scene>& scene,
                               const render::TilePlan& tilePlan,
                               const RasterTriangleEmitter& triangleEmitter,
                               const ShadowMaps& shadowMaps, const Recti& renderClip,
                               const std::atomic<bool>& cancelled, Buffer<Colord>& buffer,
                               const Vector2d& sampleOffset);

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
                             const Vector2d& sampleOffset, bool useExternalAttachments = true,
                             MSAAFragmentShadeCache* shadeCache = nullptr,
                             Buffer<double>* depthCapture = nullptr);
  void renderTriangleStreamPass(const Rasterizer& rasterizer,
                                const std::shared_ptr<render::Scene>& scene,
                                const RasterTriangleEmitter& triangleEmitter,
                                const render::TilePlan& tilePlan, const ShadowMaps& shadowMaps,
                                const Recti& renderClip, const std::atomic<bool>& cancelled,
                                Buffer<Colord>& buffer, const Vector2d& sampleOffset,
                                Buffer<double>* depthCapture = nullptr);

  ShadowMaps buildShadowMaps(const Rasterizer& rasterizer,
                             const std::shared_ptr<render::Scene>& scene,
                             const std::shared_ptr<render::Camera>& camera,
                             const std::atomic<bool>& cancelled);

  static RasterTriangleSet collectRasterTriangles(const RasterTriangleEmitter& triangleEmitter,
                                                  const render::TilePlan& tilePlan);
  void prepareTemporalResources(int width, int height);
  TemporalResetCondition temporalResetCondition(int width, int height) const;
  void applyTemporalAA(const Rasterizer& rasterizer, Buffer<Colord>& buffer,
                       const TemporalJitter& currentJitter);
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
  result->setMSAAShadingMode(m_msaaShadingMode);
  result->setNearClipDepth(m_nearClipDepth);
  result->setFarClipDepth(m_farClipDepth);
  result->setPostProcessAA(m_postProcessAA);
  result->setTemporalCurrentFrameWeight(m_temporalCurrentFrameWeight);
  result->setShadowMapsEnabled(m_shadowMapsEnabled);
  result->setShadowMapSize(m_shadowMapSize);
  result->setShadowCascadeCount(m_shadowCascadeCount);
  result->setShadowCascadeSplitLambda(m_shadowCascadeSplitLambda);
  result->setShadowBias(m_shadowBias);
  result->setShadowSlopeBias(m_shadowSlopeBias);
  result->setShadowFilterRadius(m_shadowFilterRadius);
  result->setShadowFilterMode(m_shadowFilterMode);
  result->m_cullMode = m_cullMode;
  result->m_hasCullModeOverride = m_hasCullModeOverride;
  result->m_viewportEnabled = m_viewportEnabled;
  result->m_viewportRect = m_viewportRect;
  result->m_scissorTestEnabled = m_scissorTestEnabled;
  result->m_scissorRect = m_scissorRect;
  result->setColorLoadOp(m_colorLoadOp);
  result->setColorStoreOp(m_colorStoreOp);
  result->setDepthFunc(m_depthFunc);
  result->setDepthBias(m_depthBias);
  result->setDepthClearValue(m_depthClearValue);
  result->setDepthLoadOp(m_depthLoadOp);
  result->setDepthStoreOp(m_depthStoreOp);
  result->setDepthWriteEnabled(m_depthWriteEnabled);
  result->setStencilTestEnabled(m_stencilTestEnabled);
  result->setStencilFunc(m_stencilFunc, m_stencilReference, m_stencilMask);
  result->setStencilClearValue(m_stencilClearValue);
  result->setStencilLoadOp(m_stencilLoadOp);
  result->setStencilStoreOp(m_stencilStoreOp);
  result->setStencilWriteMask(m_stencilWriteMask);
  result->setStencilOps(m_stencilFailOp, m_stencilDepthFailOp, m_stencilPassOp);
  result->setAlphaTestEnabled(m_alphaTestEnabled);
  result->setAlphaFunc(m_alphaFunc, m_alphaReference);
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

void Rasterizer::setPostProcessAA(PostProcessAA aa) {
  if (m_postProcessAA != aa) {
    m_postProcessAA = aa;
    invalidateTemporalHistory();
  }
}

void Rasterizer::invalidateTemporalHistory() {
  p->temporalInvalidated = true;
}

bool Rasterizer::temporalHistoryValid() const {
  return p->temporalHistoryValid;
}

int Rasterizer::temporalFrameIndex() const {
  return p->temporalFrameIndex;
}

void Rasterizer::render(Buffer<unsigned int>& buffer) {
  Buffer<Colord> hdr(buffer.width(), buffer.height());
  loadColorAttachmentFromDisplay(*this, hdr, buffer);

  render(hdr);

  if (m_colorStoreOp == AttachmentStoreOp::Discard) {
    return;
  }

  auto outputTonemap = tonemap();
  for (int y = 0; y < hdr.height(); ++y) {
    for (int x = 0; x < hdr.width(); ++x) {
      buffer[y][x] = outputTonemap->apply(hdr[y][x]).rgb();
    }
  }
}

void Rasterizer::render(Buffer<Colord>& buffer) {
  // Caller is expected to call uncancel() between renders. Matches
  // the Wireframe / Raytracer convention.

  std::unique_ptr<Buffer<Colord>> transientColor;
  Buffer<Colord>* colorTarget = &buffer;
  if (m_colorStoreOp == AttachmentStoreOp::Discard) {
    transientColor = std::make_unique<Buffer<Colord>>(buffer.width(), buffer.height());
    colorTarget = transientColor.get();
  }

  loadColorAttachment(*this, *colorTarget, buffer);
  clearDiagnosticOutputsForRender(*this, colorTarget->width(), colorTarget->height());

  if (!m_scene || !m_camera)
    return;

  // Same view-plane setup the other engines perform — the camera
  // projection math depends on the cached basis vectors.
  const Recti viewport = configuredViewportRect(*this, colorTarget->rect());
  if (rasterRectEmpty(viewport))
    return;
  m_camera->viewPlane()->setup(m_camera->matrix(), viewport);

  // From here down the render is expressed in pipeline terms. The
  // Rasterizer object contributes configuration; Private drives the
  // concrete passes and keeps task state available for activeTiles().
  p->tasks.clear();
  p->renderFrame(*this, m_scene, m_camera, m_cancelled, *colorTarget);
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
                                          cancelled, Rasterizer::CullMode::Both, true, false);
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
  Buffer<Colord>& buffer, const Vector2d& sampleOffset, bool useExternalAttachments,
  MSAAFragmentShadeCache* shadeCache, Buffer<double>* depthCapture) {
  // A pass freezes current depth/stencil/shader state into policies,
  // then hands the triangle set to either the direct or tiled path.
  PassBuffers passBuffers(rasterizer, tilePlan, buffer, useExternalAttachments);
  RasterFullBufferView<std::uint8_t> stencilView;
  if (passBuffers.stencil()) {
    stencilView = fullBufferView(*passBuffers.stencil());
  }
  const RasterDiagnosticBufferViews diagnostics =
    diagnosticViews(rasterizer, tilePlan.width(), tilePlan.height());
  const AlphaTestState alphaTest{rasterizer.alphaTestEnabled(), rasterizer.alphaFunc(),
                                 rasterizer.alphaReference()};
  withPreparedTrianglePolicies(
    scene.get(), rasterizer, shadowMaps, fullBufferView(passBuffers.depth()), stencilView,
    [&](auto stencil, auto depth, auto fragmentPolicy) {
      withMSAAFragmentShadingPolicy(
        rasterizer, shadeCache, fragmentPolicy, [&](auto msaaFragmentPolicy) {
          rasterizeTriangleSetWithPolicies(
            triangleSet, tilePlan, *threadPool, tasks, cancelled,
            colorOutputPolicy(rasterizer, fullBufferView(passBuffers.color())), renderClip,
            sampleOffset, stencil, depth, msaaFragmentPolicy, alphaTest, diagnostics);
        });
    });

  if (depthCapture && bufferMatches(depthCapture, tilePlan.width(), tilePlan.height())) {
    copyRasterBuffer(*depthCapture, passBuffers.depth());
  }
}

void Rasterizer::Private::renderTriangleStreamPass(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const RasterTriangleEmitter& triangleEmitter, const render::TilePlan& tilePlan,
  const ShadowMaps& shadowMaps, const Recti& renderClip, const std::atomic<bool>& cancelled,
  Buffer<Colord>& buffer, const Vector2d& sampleOffset, Buffer<double>* depthCapture) {
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
  const AlphaTestState alphaTest{rasterizer.alphaTestEnabled(), rasterizer.alphaFunc(),
                                 rasterizer.alphaReference()};

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
                                              depth, fragmentPolicy, alphaTest, diagnostics);
      });
    });

  if (depthCapture && bufferMatches(depthCapture, tilePlan.width(), tilePlan.height())) {
    copyRasterBuffer(*depthCapture, passBuffers.depth());
  }
}

void Rasterizer::Private::renderSingleSampleFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& tilePlan, const RasterTriangleEmitter& triangleEmitter,
  const ShadowMaps& shadowMaps, const Recti& renderClip, const std::atomic<bool>& cancelled,
  Buffer<Colord>& buffer, const Vector2d& sampleOffset) {
  if (tilePlan.isSingleTile()) {
    renderTriangleStreamPass(rasterizer, scene, triangleEmitter, tilePlan, shadowMaps, renderClip,
                             cancelled, buffer, sampleOffset,
                             rasterizer.postProcessAA() == Rasterizer::PostProcessAA::TAA
                               ? currentDepth.get()
                               : nullptr);
    return;
  }

  const RasterTriangleSet triangleSet = collectRasterTriangles(triangleEmitter, tilePlan);
  if (cancelled.load() || triangleSet.empty())
    return;

  renderTriangleSetPass(rasterizer, scene, triangleSet, tilePlan, shadowMaps, renderClip, cancelled,
                        buffer, sampleOffset, true,
                        nullptr,
                        rasterizer.postProcessAA() == Rasterizer::PostProcessAA::TAA
                          ? currentDepth.get()
                          : nullptr);
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
  Buffer<Colord> loadedColor(tilePlan.width(), tilePlan.height());
  copyRasterBuffer(loadedColor, buffer);
  buffer.clear(Colord::black());
  MSAAFragmentShadeCache shadeCache;
  MSAAFragmentShadeCache* shadeCachePtr =
    rasterizer.msaaShadingMode() == Rasterizer::MSAAShadingMode::PerFragment ? &shadeCache
                                                                             : nullptr;
  for (int sampleIndex = 0; sampleIndex != pattern.count; ++sampleIndex) {
    if (cancelled.load())
      return;

    Buffer<Colord> sampleBuffer(tilePlan.width(), tilePlan.height());
    copyRasterBuffer(sampleBuffer, loadedColor);

    renderTriangleSetPass(rasterizer, scene, triangleSet, tilePlan, shadowMaps, renderClip,
                          cancelled, sampleBuffer, pattern.offsets[sampleIndex], false,
                          shadeCachePtr);

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
  MSAAFragmentShadeCache shadeCache;
  MSAAFragmentShadeCache* shadeCachePtr =
    rasterizer.msaaShadingMode() == Rasterizer::MSAAShadingMode::PerFragment ? &shadeCache
                                                                             : nullptr;

  for (int sampleIndex = 0; sampleIndex != pattern.count; ++sampleIndex) {
    if (cancelled.load())
      return;

    // This is simple supersampling scoped to one tile: rerun
    // coverage/depth at a fixed subpixel offset, accumulate local
    // colors, and resolve the tile into the output framebuffer.
    scratch.clearSample(rasterizer, &buffer);

    RasterTileBufferView<std::uint8_t> stencilView;
    if (scratch.stencil()) {
      stencilView = tileBufferView(*scratch.stencil(), rect);
    }
    const RasterDiagnosticBufferViews diagnostics =
      diagnosticViews(rasterizer, buffer.width(), buffer.height());
    const AlphaTestState alphaTest{rasterizer.alphaTestEnabled(), rasterizer.alphaFunc(),
                                   rasterizer.alphaReference()};

    withPreparedTrianglePolicies(
      scene.get(), rasterizer, shadowMaps, tileBufferView(scratch.depth(), rect), stencilView,
      [&](auto stencil, auto depth, auto fragmentPolicy) {
        withMSAAFragmentShadingPolicy(
          rasterizer, shadeCachePtr, fragmentPolicy, [&](auto msaaFragmentPolicy) {
            rasterizeTileWithPolicies(
              triangleSet, rect, tileIndex,
              colorOutputPolicy(rasterizer, tileBufferView(scratch.sampleColor(), rect)),
              renderClip, pattern.offsets[sampleIndex], cancelled, stencil, depth,
              msaaFragmentPolicy, alphaTest, diagnostics);
          });
      });

    if (cancelled.load())
      return;
    scratch.accumulateSample();
  }

  scratch.resolveTo(buffer, pattern.count);
}

void Rasterizer::Private::prepareTemporalResources(int width, int height) {
  if (!bufferMatches(historyColor.get(), width, height)) {
    historyColor = std::make_unique<Buffer<Colord>>(width, height);
    nextHistoryColor = std::make_unique<Buffer<Colord>>(width, height);
    historyDepth = std::make_unique<Buffer<double>>(width, height);
    currentDepth = std::make_unique<Buffer<double>>(width, height);
    motionVectors = std::make_unique<Buffer<Vector2d>>(width, height);
    temporalHistoryValid = false;
    temporalFrameIndex = 0;
    previousJitter = TemporalJitter();
    pendingTemporalReset = TemporalResetCondition::ResourceResize;
    return;
  }

  if (!bufferMatches(nextHistoryColor.get(), width, height))
    nextHistoryColor = std::make_unique<Buffer<Colord>>(width, height);
  if (!bufferMatches(historyDepth.get(), width, height))
    historyDepth = std::make_unique<Buffer<double>>(width, height);
  if (!bufferMatches(currentDepth.get(), width, height))
    currentDepth = std::make_unique<Buffer<double>>(width, height);
  if (!bufferMatches(motionVectors.get(), width, height))
    motionVectors = std::make_unique<Buffer<Vector2d>>(width, height);
}

TemporalResetCondition Rasterizer::Private::temporalResetCondition(int width, int height) const {
  if (!bufferMatches(historyColor.get(), width, height) ||
      !bufferMatches(historyDepth.get(), width, height)) {
    return TemporalResetCondition::ResourceResize;
  }
  if (pendingTemporalReset != TemporalResetCondition::None) {
    return pendingTemporalReset;
  }
  if (temporalInvalidated) {
    return TemporalResetCondition::HistoryInvalidated;
  }
  if (!temporalHistoryValid) {
    return TemporalResetCondition::FirstFrame;
  }
  return TemporalResetCondition::None;
}

void Rasterizer::Private::applyTemporalAA(const Rasterizer& rasterizer, Buffer<Colord>& buffer,
                                          const TemporalJitter& currentJitter) {
  const int width = buffer.width();
  const int height = buffer.height();
  const TemporalResetCondition resetCondition = temporalResetCondition(width, height);
  if (resetCondition != TemporalResetCondition::None) {
    temporalFrameIndex = 0;
  }

  clearTemporalMotionVectors(*motionVectors, currentJitter, previousJitter);

  TemporalResourceContract contract;
  contract.historyColor = historyColor.get();
  contract.nextHistoryColor = nextHistoryColor.get();
  contract.currentDepth = currentDepth.get();
  contract.historyDepth = historyDepth.get();
  contract.motionVectors = motionVectors.get();
  contract.currentJitter = currentJitter;
  contract.previousJitter = previousJitter;
  contract.resetCondition = resetCondition;

  applyTemporalAccumulation(contract, buffer, rasterizer.temporalCurrentFrameWeight());
  copyRasterBuffer(*historyColor, *nextHistoryColor);
  copyDepthHistory(*historyDepth, *currentDepth);
  previousJitter = currentJitter;
  temporalHistoryValid = true;
  temporalCamera = rasterizer.camera().get();
  temporalScene = rasterizer.scene().get();
  temporalInvalidated = false;
  pendingTemporalReset = TemporalResetCondition::None;
  ++temporalFrameIndex;
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

  const bool useTemporalAA = rasterizer.postProcessAA() == Rasterizer::PostProcessAA::TAA;
  if (useTemporalAA) {
    if (temporalHistoryValid && temporalScene != scene.get()) {
      pendingTemporalReset = TemporalResetCondition::SceneDiscontinuity;
    } else if (temporalHistoryValid && temporalCamera != camera.get()) {
      pendingTemporalReset = TemporalResetCondition::CameraCut;
    }
    prepareTemporalResources(width, height);
    currentDepth->clear(std::numeric_limits<double>::infinity());
  }
  const TemporalResetCondition resetCondition =
    useTemporalAA ? temporalResetCondition(width, height) : TemporalResetCondition::None;
  const int jitterFrame =
    resetCondition == TemporalResetCondition::None ? temporalFrameIndex : 0;
  const TemporalJitter currentJitter =
    useTemporalAA ? temporalJitterForFrame(jitterFrame) : TemporalJitter();
  const Vector2d sampleOffset(currentJitter.x, currentJitter.y);

  const render::TilePlan tilePlan = render::TilePlan::forBuffer(width, height, queueSize);
  const MSAASamplePattern pattern(rasterizer.msaaSamples());
  const RasterTriangleEmitter triangleEmitter(scene.get(), camera, rasterizer.lod(), rasterizer,
                                              cancelled, rasterizer.cullMode(),
                                              rasterizer.hasCullModeOverride(), true);
  const ShadowMaps shadowMaps = buildShadowMaps(rasterizer, scene, camera, cancelled);
  if (pattern.count > 1) {
    renderMSAAFrame(rasterizer, scene, tilePlan, pattern, triangleEmitter, shadowMaps, renderClip,
                    cancelled, buffer);
  } else {
    renderSingleSampleFrame(rasterizer, scene, tilePlan, triangleEmitter, shadowMaps, renderClip,
                            cancelled, buffer, sampleOffset);
  }

  if (cancelled.load()) {
    return;
  }

  if (useTemporalAA && pattern.count == 1) {
    applyTemporalAA(rasterizer, buffer, currentJitter);
  } else {
    switch (rasterizer.postProcessAA()) {
      case Rasterizer::PostProcessAA::None:
      case Rasterizer::PostProcessAA::TAA:
        break;
      case Rasterizer::PostProcessAA::FXAA:
        render::postprocess::applyFxaa(buffer);
        break;
      case Rasterizer::PostProcessAA::SMAA:
        render::postprocess::applySmaa(buffer);
        break;
    }
  }
}
