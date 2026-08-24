#include "engine/raster/Rasterizer.h"

#include "engine/raster/detail/RasterMSAA.h"
#include "engine/raster/detail/RasterPass.h"
#include "engine/raster/detail/RasterPipelineTypes.h"
#include "engine/raster/detail/RasterShadowMapBuilder.h"
#include "engine/raster/detail/RasterTemporalResources.h"
#include "engine/raster/detail/RasterTriangleEmitter.h"

#include "core/Buffer.h"
#include "core/math/Vector.h"
#include "core/util/BufferUtils.h"
#include "render/TilePlan.h"
#include "render/TimingHelpers.h"
#include "render/cameras/Camera.h"
#include "render/lights/Light.h"
#include "render/postprocess/Fxaa.h"
#include "render/postprocess/Smaa.h"
#include "render/primitives/Scene.h"
#include "render/tonemap/Tonemap.h"
#include "render/viewplanes/ViewPlane.h"

#include "engine/TileRenderTask.h"

#include <QThread>
#include <QThreadPool>

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <list>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace engine::raster;

namespace {
  constexpr double kMinimumRasterClipDepth = 1e-6;
  using RasterClock = std::chrono::steady_clock;

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
  using engine::raster::detail::AlphaTestState;
  using engine::raster::detail::colorOutputPolicy;
  using engine::raster::detail::DepthReadOnlyPolicy;
  using engine::raster::detail::DepthState;
  using engine::raster::detail::DepthWritePolicy;
  using engine::raster::detail::fullBufferView;
  using engine::raster::detail::intersectRasterRects;
  using engine::raster::detail::MSAAFragmentShadeCache;
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
  using engine::raster::detail::rasterRectEmpty;
  using engine::raster::detail::RasterShadowMapBuilder;
  using engine::raster::detail::RasterTileBufferView;
  using engine::raster::detail::RasterTriangle;
  using engine::raster::detail::RasterTriangleEmitter;
  using engine::raster::detail::RasterTriangleSet;
  using engine::raster::detail::resolveMSAA;
  using engine::raster::detail::ShadowMaps;
  using engine::raster::detail::TemporalJitter;
  using engine::raster::detail::TemporalResetCondition;
  using engine::raster::detail::TemporalResourceContract;
  using engine::raster::detail::tileBufferView;
  using engine::raster::detail::validateTemporalResourceContract;
  using engine::raster::detail::withMSAAFragmentShadingPolicy;
  using engine::raster::detail::withPreparedTrianglePolicies;

  template<class T>
  RasterFullBufferView<T> diagnosticView(Buffer<T>* buffer, int width, int height) {
    if (!core::util::bufferDimensionsMatch(buffer, width, height))
      return RasterFullBufferView<T>();
    return fullBufferView(*buffer);
  }

  struct RasterMetricCounterBuffers {
    Buffer<std::uint32_t>* coverage = nullptr;
    Buffer<std::uint32_t>* depthTest = nullptr;
    Buffer<std::uint32_t>* depthPass = nullptr;
    Buffer<std::uint32_t>* shade = nullptr;
    Buffer<std::uint32_t>* colorWrite = nullptr;
  };

  struct RasterMetricCounterAtomics {
    std::atomic<std::uint64_t>* coveredSamples = nullptr;
    std::atomic<std::uint64_t>* stencilTests = nullptr;
    std::atomic<std::uint64_t>* stencilFails = nullptr;
    std::atomic<std::uint64_t>* depthTests = nullptr;
    std::atomic<std::uint64_t>* depthPasses = nullptr;
    std::atomic<std::uint64_t>* depthFails = nullptr;
    std::atomic<std::uint64_t>* shadedFragments = nullptr;
    std::atomic<std::uint64_t>* alphaTestFails = nullptr;
    std::atomic<std::uint64_t>* colorWrites = nullptr;
    std::atomic<std::uint64_t>* conservativeDepthRejectedTriangleTiles = nullptr;
  };

  RasterDiagnosticBufferViews diagnosticViews(const Rasterizer& rasterizer, int width, int height,
                                              const RasterMetricCounterBuffers& metricBuffers,
                                              const RasterMetricCounterAtomics& metricAtomics) {
    const auto& outputs = rasterizer.diagnosticOutputBuffers();
    return {diagnosticView(outputs.depth, width, height),
            diagnosticView(outputs.worldPosition, width, height),
            diagnosticView(outputs.normal, width, height),
            diagnosticView(outputs.primitive, width, height),
            diagnosticView(outputs.material, width, height),
            diagnosticView(outputs.face, width, height),
            diagnosticView(outputs.stencil, width, height),
            diagnosticView(outputs.coverageCount, width, height),
            diagnosticView(outputs.depthTestCount, width, height),
            diagnosticView(outputs.depthPassCount, width, height),
            diagnosticView(outputs.shadeCount, width, height),
            diagnosticView(outputs.colorWriteCount, width, height),
            diagnosticView(metricBuffers.coverage, width, height),
            diagnosticView(metricBuffers.depthTest, width, height),
            diagnosticView(metricBuffers.depthPass, width, height),
            diagnosticView(metricBuffers.shade, width, height),
            diagnosticView(metricBuffers.colorWrite, width, height),
            metricAtomics.coveredSamples,
            metricAtomics.stencilTests,
            metricAtomics.stencilFails,
            metricAtomics.depthTests,
            metricAtomics.depthPasses,
            metricAtomics.depthFails,
            metricAtomics.shadedFragments,
            metricAtomics.alphaTestFails,
            metricAtomics.colorWrites,
            metricAtomics.conservativeDepthRejectedTriangleTiles};
  }

  template<class T>
  void clearDiagnosticBuffer(Buffer<T>* buffer, int width, int height, const T& value) {
    if (core::util::bufferDimensionsMatch(buffer, width, height)) {
      buffer->clear(value);
    }
  }

  void clearDiagnosticOutputsForRender(const Rasterizer& rasterizer, int width, int height) {
    const auto& outputs = rasterizer.diagnosticOutputBuffers();
    clearDiagnosticBuffer(outputs.depth, width, height, rasterizer.depthClearValue());
    clearDiagnosticBuffer(outputs.worldPosition, width, height, Vector3d::undefined);
    clearDiagnosticBuffer(outputs.normal, width, height, Vector3d::undefined);
    clearDiagnosticBuffer(outputs.primitive, width, height,
                          static_cast<const render::Primitive*>(nullptr));
    clearDiagnosticBuffer(outputs.material, width, height,
                          static_cast<const render::Material*>(nullptr));
    clearDiagnosticBuffer(outputs.face, width, height, std::numeric_limits<std::uint64_t>::max());
    clearDiagnosticBuffer(outputs.stencil, width, height, rasterizer.stencilClearValue());
    clearDiagnosticBuffer(outputs.coverageCount, width, height, 0u);
    clearDiagnosticBuffer(outputs.depthTestCount, width, height, 0u);
    clearDiagnosticBuffer(outputs.depthPassCount, width, height, 0u);
    clearDiagnosticBuffer(outputs.shadeCount, width, height, 0u);
    clearDiagnosticBuffer(outputs.colorWriteCount, width, height, 0u);
  }

  void loadColorAttachment(const Rasterizer& rasterizer, Buffer<Colord>& target,
                           const Buffer<Colord>& source) {
    if (rasterizer.colorLoadOp() == Rasterizer::AttachmentLoadOp::Load) {
      if (&target != &source) {
        core::util::copyBuffer(target, source);
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
    Recti result =
      intersectRasterRects(framebufferRect, configuredViewportRect(rasterizer, framebufferRect));
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

  struct RasterTilingStats {
    std::size_t triangles{0};
    std::size_t tileReferences{0};
    std::size_t nonEmptyTiles{0};
    std::size_t maxTriangleReferencesPerTile{0};
    double p95TriangleReferencesPerTile{0.0};
    double projectedBoundsPixels{0.0};
    double maxProjectedBoundsPixels{0.0};
  };

  struct RasterQueueChoice {
    render::TilePlan tilePlan;
    RasterTriangleSet triangleSet;
    int queueSize{1};
    std::string decision;
    std::string reason;

    RasterQueueChoice(render::TilePlan plan, RasterTriangleSet set, int queue,
                      std::string queueDecision, std::string queueReason)
        : tilePlan(std::move(plan)),
          triangleSet(std::move(set)),
          queueSize(queue),
          decision(std::move(queueDecision)),
          reason(std::move(queueReason)) {
    }
  };

  double projectedBoundsArea(const RasterTriangle& triangle) {
    const double minX =
      std::min({triangle.vertices[0].x, triangle.vertices[1].x, triangle.vertices[2].x});
    const double maxX =
      std::max({triangle.vertices[0].x, triangle.vertices[1].x, triangle.vertices[2].x});
    const double minY =
      std::min({triangle.vertices[0].y, triangle.vertices[1].y, triangle.vertices[2].y});
    const double maxY =
      std::max({triangle.vertices[0].y, triangle.vertices[1].y, triangle.vertices[2].y});
    return std::max(0.0, maxX - minX) * std::max(0.0, maxY - minY);
  }

  double triangleDepthKey(const RasterTriangle& triangle) {
    return std::min({triangle.vertices[0].depthOverW / triangle.vertices[0].invW,
                     triangle.vertices[1].depthOverW / triangle.vertices[1].invW,
                     triangle.vertices[2].depthOverW / triangle.vertices[2].invW});
  }

  void sortFrontToBack(std::vector<RasterTriangle>& triangles) {
    std::stable_sort(triangles.begin(), triangles.end(),
                     [](const RasterTriangle& lhs, const RasterTriangle& rhs) {
                       return triangleDepthKey(lhs) < triangleDepthKey(rhs);
                     });
  }

  bool passSupportsConservativeDepthOcclusion(const Rasterizer& rasterizer) {
    if (rasterizer.alphaTestEnabled() || rasterizer.blendingEnabled() ||
        rasterizer.stencilTestEnabled()) {
      return false;
    }
    if (!rasterizer.depthWriteEnabled() || rasterizer.depthFunc() != Rasterizer::DepthFunc::Less) {
      return false;
    }
    return rasterizer.colorWriteMask() == Rasterizer::ColorWriteAll;
  }

  const char* depthPrepassModeName(Rasterizer::DepthPrepassMode mode) {
    switch (mode) {
    case Rasterizer::DepthPrepassMode::Off:
      return "off";
    case Rasterizer::DepthPrepassMode::On:
      return "on";
    case Rasterizer::DepthPrepassMode::Auto:
      return "auto";
    }
    return "off";
  }

  bool rasterPassHasExpensiveShading(const Rasterizer& rasterizer) {
    const auto scene = rasterizer.scene();
    return static_cast<bool>(rasterizer.fragmentShader()) || rasterizer.shadowMapsEnabled() ||
           (scene && !scene->lights().empty());
  }

  std::string depthPrepassDecision(const Rasterizer& rasterizer, const render::TilePlan& tilePlan) {
    if (rasterizer.depthPrepassMode() == Rasterizer::DepthPrepassMode::Off)
      return "disabled";
    if (rasterizer.msaaSamples() != 1)
      return "suppressed_msaa";
    if (tilePlan.isSingleTile())
      return "suppressed_streaming_single_tile";
    if (!passSupportsConservativeDepthOcclusion(rasterizer))
      return "suppressed_non_opaque_or_unsupported_state";
    if (rasterizer.depthPrepassMode() == Rasterizer::DepthPrepassMode::Auto &&
        !rasterPassHasExpensiveShading(rasterizer) && !rasterizer.visibilitySet())
      return "suppressed_auto_no_expensive_shading_or_hierarchical_consumer";
    return "enabled";
  }

  RasterTilingStats tilingStats(const RasterTriangleSet& triangleSet,
                                const render::TilePlan& tilePlan) {
    RasterTilingStats stats;
    stats.triangles = triangleSet.triangles().size();
    for (const auto& triangle : triangleSet.triangles()) {
      const double area = projectedBoundsArea(triangle);
      stats.projectedBoundsPixels += area;
      stats.maxProjectedBoundsPixels = std::max(stats.maxProjectedBoundsPixels, area);
    }
    std::vector<std::size_t> references;
    references.reserve(tilePlan.size());
    for (std::size_t tile = 0; tile != tilePlan.size(); ++tile) {
      const std::size_t count = triangleSet.tileGrid().triangleIndices(tile).size();
      references.push_back(count);
      stats.tileReferences += count;
      if (count > 0) {
        ++stats.nonEmptyTiles;
      }
      stats.maxTriangleReferencesPerTile = std::max(stats.maxTriangleReferencesPerTile, count);
    }
    if (!references.empty()) {
      std::sort(references.begin(), references.end());
      const std::size_t index =
        static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(references.size())) - 1.0);
      stats.p95TriangleReferencesPerTile =
        static_cast<double>(references[std::min(index, references.size() - 1)]);
    }
    return stats;
  }

  int automaticQueueCandidate(int threads) {
    return std::max(1, threads * 4);
  }

  double averageTilesPerTriangle(const RasterTilingStats& stats) {
    return stats.triangles > 0
             ? static_cast<double>(stats.tileReferences) / static_cast<double>(stats.triangles)
             : 0.0;
  }

  double tileLoadImbalance(const RasterTilingStats& stats) {
    if (stats.nonEmptyTiles == 0 || stats.tileReferences == 0)
      return 0.0;
    const double averageTileReferences =
      static_cast<double>(stats.tileReferences) / static_cast<double>(stats.nonEmptyTiles);
    return averageTileReferences > 0.0
             ? static_cast<double>(stats.maxTriangleReferencesPerTile) / averageTileReferences
             : 0.0;
  }

  std::string tiledRasterizationRejectionReason(const RasterTilingStats& stats, int width,
                                                int height, int queueSize, int threads,
                                                int msaaSamples) {
    if (threads <= 1 || queueSize <= 1 || stats.triangles == 0)
      return "single_worker_or_empty";

    const double framePixels = static_cast<double>(std::max(1, width * height));
    if (framePixels < 16384.0)
      return "small_frame";

    const double triangles = static_cast<double>(stats.triangles);
    const double avgTilesPerTriangle = averageTilesPerTriangle(stats);
    const double avgProjectedBounds =
      triangles > 0.0 ? stats.projectedBoundsPixels / triangles : 0.0;
    const double trianglesPerFramePixel = triangles / framePixels;
    const double imbalance = tileLoadImbalance(stats);

    // Grounded in the #168 measurements: tiled rendering wins for screen-heavy
    // scenes with moderate projected triangle counts, but loses badly when dense
    // tessellation makes triangle preparation and tile-list duplication dominate.
    if (avgTilesPerTriangle > 2.25)
      return "tile_reference_duplication";
    if (imbalance > 6.0 &&
        stats.maxTriangleReferencesPerTile > stats.p95TriangleReferencesPerTile * 3.0)
      return "tile_load_imbalance";
    if (trianglesPerFramePixel > 1.0 / 32.0)
      return "dense_triangle_load";
    if (triangles < static_cast<double>(threads * 16))
      return "too_few_triangles";

    const double sampleMultiplier = static_cast<double>(std::max(1, msaaSamples));
    const double projectedWork = stats.projectedBoundsPixels * sampleMultiplier;
    if (projectedWork < framePixels * 0.20)
      return "low_projected_work";

    return avgProjectedBounds >= 16.0 ? "" : "tiny_projected_triangles";
  }

  std::vector<int> adaptiveQueueSizes(int requestedQueueSize, int threads) {
    std::vector<int> queueSizes;
    auto add = [&](int queueSize) {
      queueSize = std::max(1, queueSize);
      if (std::find(queueSizes.begin(), queueSizes.end(), queueSize) == queueSizes.end()) {
        queueSizes.push_back(queueSize);
      }
    };

    add(requestedQueueSize);
    for (int queueSize = requestedQueueSize / 2; queueSize > threads; queueSize /= 2) {
      add(queueSize);
    }
    add(threads);
    add(1);
    return queueSizes;
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
    core::util::copyBuffer(target, source);
  }

  void writeTonemappedTile(Buffer<unsigned int>& target, const Buffer<Colord>& source,
                           const render::Tonemap& tonemap, const Recti& rect) {
    for (int y = rect.top(); y != rect.bottom(); ++y) {
      for (int x = rect.left(); x != rect.right(); ++x) {
        target[y][x] = tonemap.apply(source[y][x]).rgb();
      }
    }
  }
}

// Pimpl: hides Qt threading and render-pass orchestration from the
// public header, and gives the .cpp room for local pipeline types.
struct Rasterizer::Private {
  Private()
      : threadPool(std::make_unique<QThreadPool>()),
        queueSize(automaticQueueCandidate(QThread::idealThreadCount())),
        lastResolvedQueueSize(1) {
    threadPool->setMaxThreadCount(std::max(1, QThread::idealThreadCount()));
  }

  std::unique_ptr<QThreadPool> threadPool;
  std::list<std::shared_ptr<engine::TileRenderTask>> tasks;
  int queueSize;
  int lastResolvedQueueSize;
  bool automaticQueueSize{true};
  std::unique_ptr<Buffer<Colord>> historyColor;
  std::unique_ptr<Buffer<Colord>> nextHistoryColor;
  std::unique_ptr<Buffer<double>> historyDepth;
  std::unique_ptr<Buffer<double>> currentDepth;
  std::unique_ptr<Buffer<Vector2d>> motionVectors;
  std::vector<std::unique_ptr<MSAATileScratch>> msaaTileScratch;
  TemporalJitter previousJitter;
  bool temporalHistoryValid{false};
  bool temporalInvalidated{false};
  TemporalResetCondition pendingTemporalReset{TemporalResetCondition::FirstFrame};
  int temporalFrameIndex{0};
  const render::Camera* temporalCamera{nullptr};
  const render::Scene* temporalScene{nullptr};
  std::unique_ptr<Buffer<std::uint32_t>> metricCoverageCount;
  std::unique_ptr<Buffer<std::uint32_t>> metricDepthTestCount;
  std::unique_ptr<Buffer<std::uint32_t>> metricDepthPassCount;
  std::unique_ptr<Buffer<std::uint32_t>> metricShadeCount;
  std::unique_ptr<Buffer<std::uint32_t>> metricColorWriteCount;
  std::atomic<std::uint64_t> metricCoveredSamples{0};
  std::atomic<std::uint64_t> metricStencilTests{0};
  std::atomic<std::uint64_t> metricStencilFails{0};
  std::atomic<std::uint64_t> metricDepthTests{0};
  std::atomic<std::uint64_t> metricDepthPasses{0};
  std::atomic<std::uint64_t> metricDepthFails{0};
  std::atomic<std::uint64_t> metricShadedFragments{0};
  std::atomic<std::uint64_t> metricAlphaTestFails{0};
  std::atomic<std::uint64_t> metricColorWrites{0};
  std::atomic<std::uint64_t> metricConservativeDepthRejectedTriangleTiles{0};

  void renderFrame(Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
                   const std::shared_ptr<render::Camera>& camera,
                   const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

  void renderSingleSampleFrame(const Rasterizer& rasterizer,
                               const std::shared_ptr<render::Scene>& scene,
                               const render::TilePlan& tilePlan,
                               const RasterTriangleEmitter& triangleEmitter,
                               const ShadowMaps& shadowMaps, const Recti& renderClip,
                               const std::atomic<bool>& cancelled, Buffer<Colord>& buffer,
                               const Vector2d& sampleOffset);

  void renderAutomaticSingleSampleFrame(const Rasterizer& rasterizer,
                                        const std::shared_ptr<render::Scene>& scene,
                                        const render::TilePlan& candidateTilePlan,
                                        const RasterTriangleEmitter& triangleEmitter,
                                        const ShadowMaps& shadowMaps, const Recti& renderClip,
                                        const std::atomic<bool>& cancelled, Buffer<Colord>& buffer,
                                        const Vector2d& sampleOffset);

  void renderMSAAFrame(const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
                       const render::TilePlan& tilePlan, const MSAASamplePattern& pattern,
                       const RasterTriangleEmitter& triangleEmitter, const ShadowMaps& shadowMaps,
                       const Recti& renderClip, const std::atomic<bool>& cancelled,
                       Buffer<Colord>& buffer);

  void renderAutomaticMSAAFrame(const Rasterizer& rasterizer,
                                const std::shared_ptr<render::Scene>& scene,
                                const render::TilePlan& candidateTilePlan,
                                const MSAASamplePattern& pattern,
                                const RasterTriangleEmitter& triangleEmitter,
                                const ShadowMaps& shadowMaps, const Recti& renderClip,
                                const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

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
  void renderTriangleListPass(const Rasterizer& rasterizer,
                              const std::shared_ptr<render::Scene>& scene,
                              const std::vector<RasterTriangle>& triangles,
                              const render::TilePlan& tilePlan, const ShadowMaps& shadowMaps,
                              const Recti& renderClip, const std::atomic<bool>& cancelled,
                              Buffer<Colord>& buffer, const Vector2d& sampleOffset,
                              Buffer<double>* depthCapture = nullptr);

  static RasterTriangleSet collectRasterTriangles(const RasterTriangleEmitter& triangleEmitter,
                                                  const render::TilePlan& tilePlan,
                                                  bool sortFrontToBack,
                                                  double* tileBinningSeconds = nullptr);
  static RasterTriangleSet triangleSetForPlan(const std::vector<RasterTriangle>& triangles,
                                              const render::TilePlan& tilePlan,
                                              bool sortFrontToBack);
  static RasterQueueChoice chooseAutomaticQueue(const RasterTriangleSet& candidateSet,
                                                const render::TilePlan& candidateTilePlan,
                                                bool sortFrontToBack, int threads, int msaaSamples,
                                                std::vector<int>* evaluatedQueueSizes,
                                                double* adaptiveBinningSeconds = nullptr);
  void prepareTemporalResources(int width, int height);
  void prepareMSAATileScratch(const Rasterizer& rasterizer, const render::TilePlan& tilePlan);
  TemporalResetCondition temporalResetCondition(int width, int height) const;
  void applyTemporalAA(const Rasterizer& rasterizer, Buffer<Colord>& buffer,
                       const TemporalJitter& currentJitter);
  void resetMetrics(Rasterizer& rasterizer, int width, int height);
  RasterMetricCounterBuffers metricCounterBuffers();
  RasterMetricCounterAtomics metricCounterAtomics();
  void recordTileMetrics(Rasterizer& rasterizer, const RasterTriangleSet& triangleSet,
                         const render::TilePlan& tilePlan);
  void recordSchedulingMetrics(Rasterizer& rasterizer, bool automatic,
                               const std::vector<int>& evaluatedQueueSizes,
                               const std::string& decision, const std::string& reason) const;
  void publishFragmentMetrics(Rasterizer& rasterizer) const;
  void publishDiagnosticImageStatistics(Rasterizer& rasterizer) const;
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

QJsonObject
engine::raster::rasterRenderMetricsToJson(const Rasterizer::RasterRenderMetrics& metrics) {
  auto distributionToJson = [](const Rasterizer::MetricDistribution& distribution) {
    QJsonObject object;
    object["max"] = static_cast<double>(distribution.max);
    object["p50"] = distribution.p50;
    object["p90"] = distribution.p90;
    object["p95"] = distribution.p95;
    object["p99"] = distribution.p99;
    return object;
  };

  QJsonArray sourceKinds;
  for (const auto& sourceKind : metrics.input.sourceKinds) {
    sourceKinds.push_back(QString::fromStdString(sourceKind));
  }

  QJsonObject input;
  input["leafPrimitiveCount"] = static_cast<double>(metrics.input.leafPrimitiveCount);
  input["meshCount"] = static_cast<double>(metrics.input.meshCount);
  input["materialCount"] = static_cast<double>(metrics.input.materialCount);
  input["lightCount"] = static_cast<double>(metrics.input.lightCount);
  input["sourceKinds"] = sourceKinds;

  QJsonObject tessellation;
  tessellation["generatedMeshVertices"] =
    static_cast<double>(metrics.tessellation.generatedMeshVertices);
  tessellation["generatedMeshFaces"] = static_cast<double>(metrics.tessellation.generatedMeshFaces);
  tessellation["preparedTrianglesBeforeCulling"] =
    static_cast<double>(metrics.tessellation.preparedTrianglesBeforeCulling);
  tessellation["trianglesRejectedByCulling"] =
    static_cast<double>(metrics.tessellation.trianglesRejectedByCulling);
  tessellation["trianglesRejectedByWindingOrDegeneracy"] =
    static_cast<double>(metrics.tessellation.trianglesRejectedByWindingOrDegeneracy);
  tessellation["trianglesAfterCulling"] =
    static_cast<double>(metrics.tessellation.trianglesAfterCulling);
  tessellation["trianglesAfterClipping"] =
    static_cast<double>(metrics.tessellation.trianglesAfterClipping);
  tessellation["lodVariantCacheHits"] =
    static_cast<double>(metrics.tessellation.lodVariantCacheHits);
  tessellation["lodVariantCacheMisses"] =
    static_cast<double>(metrics.tessellation.lodVariantCacheMisses);
  tessellation["screenSpaceLodReductions"] =
    static_cast<double>(metrics.tessellation.screenSpaceLodReductions);
  tessellation["maxProjectedPrimitivePixels"] = metrics.tessellation.maxProjectedPrimitivePixels;

  QJsonObject tiling;
  tiling["tileCount"] = static_cast<double>(metrics.tiling.tileCount);
  tiling["nonEmptyTileCount"] = static_cast<double>(metrics.tiling.nonEmptyTileCount);
  tiling["triangleReferences"] = static_cast<double>(metrics.tiling.triangleReferences);
  tiling["maxTriangleReferencesPerTile"] =
    static_cast<double>(metrics.tiling.maxTriangleReferencesPerTile);
  tiling["p95TriangleReferencesPerTile"] = metrics.tiling.p95TriangleReferencesPerTile;

  QJsonArray evaluatedQueueSizes;
  for (const std::uint64_t queueSize : metrics.scheduling.evaluatedQueueSizes) {
    evaluatedQueueSizes.push_back(static_cast<double>(queueSize));
  }

  QJsonObject scheduling;
  scheduling["automaticQueueSize"] = metrics.scheduling.automaticQueueSize;
  scheduling["configuredQueueSize"] = static_cast<double>(metrics.scheduling.configuredQueueSize);
  scheduling["resolvedQueueSize"] = static_cast<double>(metrics.scheduling.resolvedQueueSize);
  scheduling["evaluatedQueueSizes"] = evaluatedQueueSizes;
  scheduling["decision"] = QString::fromStdString(metrics.scheduling.decision);
  scheduling["reason"] = QString::fromStdString(metrics.scheduling.reason);

  QJsonObject fragments;
  fragments["coveredSamples"] = static_cast<double>(metrics.fragments.coveredSamples);
  fragments["stencilTests"] = static_cast<double>(metrics.fragments.stencilTests);
  fragments["stencilFails"] = static_cast<double>(metrics.fragments.stencilFails);
  fragments["depthTests"] = static_cast<double>(metrics.fragments.depthTests);
  fragments["depthPasses"] = static_cast<double>(metrics.fragments.depthPasses);
  fragments["depthFails"] = static_cast<double>(metrics.fragments.depthFails);
  fragments["shadedFragments"] = static_cast<double>(metrics.fragments.shadedFragments);
  fragments["alphaTestFails"] = static_cast<double>(metrics.fragments.alphaTestFails);
  fragments["colorWrites"] = static_cast<double>(metrics.fragments.colorWrites);
  fragments["conservativeDepthRejectedTriangleTiles"] =
    static_cast<double>(metrics.fragments.conservativeDepthRejectedTriangleTiles);
  fragments["coverageMinusShadedFragments"] =
    static_cast<double>(metrics.fragments.coverageMinusShadedFragments);
  fragments["depthTestsMinusColorWrites"] =
    static_cast<double>(metrics.fragments.depthTestsMinusColorWrites);

  QJsonObject diagnosticImages;
  diagnosticImages["coverage"] = distributionToJson(metrics.diagnosticImages.coverage);
  diagnosticImages["depthTest"] = distributionToJson(metrics.diagnosticImages.depthTest);
  diagnosticImages["depthPass"] = distributionToJson(metrics.diagnosticImages.depthPass);
  diagnosticImages["shade"] = distributionToJson(metrics.diagnosticImages.shade);
  diagnosticImages["colorWrite"] = distributionToJson(metrics.diagnosticImages.colorWrite);

  QJsonObject depthPrepass;
  depthPrepass["requested"] = QString::fromStdString(metrics.depthPrepass.requested);
  depthPrepass["enabled"] = metrics.depthPrepass.enabled;
  depthPrepass["decision"] = QString::fromStdString(metrics.depthPrepass.decision);
  depthPrepass["inputTriangles"] = static_cast<double>(metrics.depthPrepass.inputTriangles);
  depthPrepass["prepassSeconds"] = metrics.depthPrepass.prepassSeconds;
  depthPrepass["colorPassSeconds"] = metrics.depthPrepass.colorPassSeconds;
  depthPrepass["totalMeasuredSeconds"] = metrics.depthPrepass.totalMeasuredSeconds;

  QJsonObject timings;
  timings["tessellationTriangleEmissionSeconds"] =
    metrics.timings.tessellationTriangleEmissionSeconds;
  timings["tileBinningSeconds"] = metrics.timings.tileBinningSeconds;
  timings["rasterLoopSeconds"] = metrics.timings.rasterLoopSeconds;
  timings["msaaResolveSeconds"] = metrics.timings.msaaResolveSeconds;
  timings["postprocessSeconds"] = metrics.timings.postprocessSeconds;
  timings["totalRenderSeconds"] = metrics.timings.totalRenderSeconds;

  QJsonObject object;
  object["input"] = input;
  object["tessellation"] = tessellation;
  object["tiling"] = tiling;
  object["scheduling"] = scheduling;
  object["fragments"] = fragments;
  object["depthPrepass"] = depthPrepass;
  object["diagnosticImages"] = diagnosticImages;
  object["timings"] = timings;
  return object;
}

void Rasterizer::Private::resetMetrics(Rasterizer& rasterizer, int width, int height) {
  rasterizer.m_lastMetrics = Rasterizer::RasterRenderMetrics();
  if (rasterizer.scene()) {
    rasterizer.m_lastMetrics.input.lightCount = rasterizer.scene()->lights().size();
  }

  auto prepareCounter = [&](std::unique_ptr<Buffer<std::uint32_t>>& buffer) {
    if (!core::util::bufferDimensionsMatch(buffer.get(), width, height)) {
      buffer = std::make_unique<Buffer<std::uint32_t>>(width, height);
    }
    buffer->clear(0u);
  };
  prepareCounter(metricCoverageCount);
  prepareCounter(metricDepthTestCount);
  prepareCounter(metricDepthPassCount);
  prepareCounter(metricShadeCount);
  prepareCounter(metricColorWriteCount);

  metricCoveredSamples.store(0, std::memory_order_relaxed);
  metricStencilTests.store(0, std::memory_order_relaxed);
  metricStencilFails.store(0, std::memory_order_relaxed);
  metricDepthTests.store(0, std::memory_order_relaxed);
  metricDepthPasses.store(0, std::memory_order_relaxed);
  metricDepthFails.store(0, std::memory_order_relaxed);
  metricShadedFragments.store(0, std::memory_order_relaxed);
  metricAlphaTestFails.store(0, std::memory_order_relaxed);
  metricColorWrites.store(0, std::memory_order_relaxed);
  metricConservativeDepthRejectedTriangleTiles.store(0, std::memory_order_relaxed);
}

RasterMetricCounterBuffers Rasterizer::Private::metricCounterBuffers() {
  return {metricCoverageCount.get(), metricDepthTestCount.get(), metricDepthPassCount.get(),
          metricShadeCount.get(), metricColorWriteCount.get()};
}

RasterMetricCounterAtomics Rasterizer::Private::metricCounterAtomics() {
  return {&metricCoveredSamples,  &metricStencilTests,
          &metricStencilFails,    &metricDepthTests,
          &metricDepthPasses,     &metricDepthFails,
          &metricShadedFragments, &metricAlphaTestFails,
          &metricColorWrites,     &metricConservativeDepthRejectedTriangleTiles};
}

void Rasterizer::Private::recordTileMetrics(Rasterizer& rasterizer,
                                            const RasterTriangleSet& triangleSet,
                                            const render::TilePlan& tilePlan) {
  auto& tiling = rasterizer.m_lastMetrics.tiling;
  tiling.tileCount = tilePlan.size();
  tiling.nonEmptyTileCount = 0;
  tiling.triangleReferences = 0;
  tiling.maxTriangleReferencesPerTile = 0;

  std::vector<std::uint64_t> references;
  references.reserve(tilePlan.size());
  for (std::size_t tile = 0; tile != tilePlan.size(); ++tile) {
    const std::uint64_t count = triangleSet.tileGrid().triangleIndices(tile).size();
    references.push_back(count);
    tiling.triangleReferences += count;
    if (count > 0) {
      ++tiling.nonEmptyTileCount;
    }
    tiling.maxTriangleReferencesPerTile = std::max(tiling.maxTriangleReferencesPerTile, count);
  }

  if (!references.empty()) {
    std::sort(references.begin(), references.end());
    const std::size_t index =
      static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(references.size())) - 1.0);
    tiling.p95TriangleReferencesPerTile =
      static_cast<double>(references[std::min(index, references.size() - 1)]);
  }
}

void Rasterizer::Private::recordSchedulingMetrics(Rasterizer& rasterizer, bool automatic,
                                                  const std::vector<int>& evaluatedQueueSizes,
                                                  const std::string& decision,
                                                  const std::string& reason) const {
  auto& scheduling = rasterizer.m_lastMetrics.scheduling;
  scheduling.automaticQueueSize = automatic;
  scheduling.configuredQueueSize = static_cast<std::uint64_t>(std::max(1, queueSize));
  scheduling.resolvedQueueSize = static_cast<std::uint64_t>(std::max(1, lastResolvedQueueSize));
  scheduling.evaluatedQueueSizes.clear();
  scheduling.evaluatedQueueSizes.reserve(evaluatedQueueSizes.size());
  for (const int evaluatedQueueSize : evaluatedQueueSizes) {
    scheduling.evaluatedQueueSizes.push_back(
      static_cast<std::uint64_t>(std::max(1, evaluatedQueueSize)));
  }
  scheduling.decision = decision;
  scheduling.reason = reason;
}

void Rasterizer::Private::publishFragmentMetrics(Rasterizer& rasterizer) const {
  auto& fragments = rasterizer.m_lastMetrics.fragments;
  fragments.coveredSamples = metricCoveredSamples.load(std::memory_order_relaxed);
  fragments.stencilTests = metricStencilTests.load(std::memory_order_relaxed);
  fragments.stencilFails = metricStencilFails.load(std::memory_order_relaxed);
  fragments.depthTests = metricDepthTests.load(std::memory_order_relaxed);
  fragments.depthPasses = metricDepthPasses.load(std::memory_order_relaxed);
  fragments.depthFails = metricDepthFails.load(std::memory_order_relaxed);
  fragments.shadedFragments = metricShadedFragments.load(std::memory_order_relaxed);
  fragments.alphaTestFails = metricAlphaTestFails.load(std::memory_order_relaxed);
  fragments.colorWrites = metricColorWrites.load(std::memory_order_relaxed);
  fragments.conservativeDepthRejectedTriangleTiles =
    metricConservativeDepthRejectedTriangleTiles.load(std::memory_order_relaxed);
  fragments.coverageMinusShadedFragments = fragments.coveredSamples >= fragments.shadedFragments
                                             ? fragments.coveredSamples - fragments.shadedFragments
                                             : 0;
  fragments.depthTestsMinusColorWrites = fragments.depthTests >= fragments.colorWrites
                                           ? fragments.depthTests - fragments.colorWrites
                                           : 0;
}

void Rasterizer::Private::publishDiagnosticImageStatistics(Rasterizer& rasterizer) const {
  auto distributionFor = [](const Buffer<std::uint32_t>* buffer) {
    Rasterizer::MetricDistribution distribution;
    if (!buffer || buffer->width() <= 0 || buffer->height() <= 0) {
      return distribution;
    }

    std::vector<std::uint32_t> values;
    values.reserve(static_cast<std::size_t>(buffer->width() * buffer->height()));
    for (int y = 0; y != buffer->height(); ++y) {
      for (int x = 0; x != buffer->width(); ++x) {
        values.push_back((*buffer)[y][x]);
      }
    }
    std::sort(values.begin(), values.end());
    distribution.max = values.back();

    const auto percentile = [&](double q) {
      if (values.empty())
        return 0.0;
      const double scaled = q * static_cast<double>(values.size() - 1);
      const std::size_t index = static_cast<std::size_t>(std::round(scaled));
      return static_cast<double>(values[std::min(index, values.size() - 1)]);
    };
    distribution.p50 = percentile(0.50);
    distribution.p90 = percentile(0.90);
    distribution.p95 = percentile(0.95);
    distribution.p99 = percentile(0.99);
    return distribution;
  };

  auto& stats = rasterizer.m_lastMetrics.diagnosticImages;
  stats.coverage = distributionFor(metricCoverageCount.get());
  stats.depthTest = distributionFor(metricDepthTestCount.get());
  stats.depthPass = distributionFor(metricDepthPassCount.get());
  stats.shade = distributionFor(metricShadeCount.get());
  stats.colorWrite = distributionFor(metricColorWriteCount.get());
}

std::shared_ptr<render::RenderEngine> Rasterizer::cloneForRender() const {
  auto result = std::make_shared<Rasterizer>(m_camera ? m_camera->clone() : nullptr, m_scene);
  copyRenderEngineStateTo(*result);
  result->setLod(m_lod);
  result->setTessellationQuality(m_tessellationQuality);
  if (hasMaximumScreenSpaceErrorOverride()) {
    result->setMaximumScreenSpaceError(m_maximumScreenSpaceError);
  }
  result->setMaximumThreads(p->threadPool->maxThreadCount());
  if (p->automaticQueueSize) {
    result->p->queueSize = p->queueSize;
    result->setAutomaticQueueSize();
  } else {
    result->setQueueSize(p->queueSize);
  }
  result->setMSAASamples(m_msaaSamples);
  result->setMSAAShadingMode(m_msaaShadingMode);
  result->setNearClipDepth(m_nearClipDepth);
  result->setFarClipDepth(m_farClipDepth);
  result->setPostProcessAA(m_postProcessAA);
  result->setTemporalCurrentFrameWeight(m_temporalCurrentFrameWeight);
  result->setShadowMapsEnabled(m_shadowMapsEnabled);
  result->setExternalShadowMaps(m_externalShadowMaps);
  result->setVisibilitySet(m_visibilitySet);
  result->setShadowMapSize(m_shadowMapSize);
  result->setShadowCascadeCount(m_shadowCascadeCount);
  result->setShadowCascadeSplitLambda(m_shadowCascadeSplitLambda);
  result->setShadowBias(m_shadowBias);
  result->setShadowSlopeBias(m_shadowSlopeBias);
  result->setShadowFilterRadius(m_shadowFilterRadius);
  result->setShadowFilterMode(m_shadowFilterMode);
  result->setDepthPrepassMode(m_depthPrepassMode);
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

double Rasterizer::presetScreenSpaceError(TessellationQuality quality) {
  switch (quality) {
  case TessellationQuality::Preview:
    return 8.0;
  case TessellationQuality::Balanced:
    return 2.0;
  case TessellationQuality::Final:
    return 0.0;
  }
  return 2.0;
}

void Rasterizer::setMaximumThreads(int threads) {
  p->threadPool->setMaxThreadCount(std::max(1, threads));
  if (p->automaticQueueSize) {
    p->queueSize = automaticQueueCandidate(p->threadPool->maxThreadCount());
  }
}

int Rasterizer::queueSize() const {
  return p->queueSize;
}

bool Rasterizer::hasExplicitQueueSize() const {
  return !p->automaticQueueSize;
}

int Rasterizer::lastResolvedQueueSize() const {
  return p->lastResolvedQueueSize;
}

void Rasterizer::setQueueSize(int queue) {
  p->queueSize = std::max(1, queue);
  p->automaticQueueSize = false;
}

void Rasterizer::setAutomaticQueueSize() {
  p->automaticQueueSize = true;
  p->queueSize = automaticQueueCandidate(p->threadPool->maxThreadCount());
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
  m_nearClipDepth = finiteAtLeast(kMinimumRasterClipDepth, depth);
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

std::shared_ptr<const ShadowMaps> Rasterizer::buildShadowMaps() const {
  if (!m_scene || !m_camera)
    return std::make_shared<ShadowMaps>();
  return std::make_shared<ShadowMaps>(
    RasterShadowMapBuilder(*this, m_scene, m_camera, *p->threadPool, m_cancelled).build());
}

void Rasterizer::setExternalShadowMaps(std::shared_ptr<const ShadowMaps> shadowMaps) {
  m_externalShadowMaps = std::move(shadowMaps);
}

void Rasterizer::setVisibilitySet(std::shared_ptr<const RasterVisibilitySet> visibilitySet) {
  m_visibilitySet = std::move(visibilitySet);
}

void Rasterizer::clearVisibilitySet() {
  m_visibilitySet.reset();
}

std::shared_ptr<const RasterVisibilitySet> Rasterizer::visibilitySet() const {
  return m_visibilitySet;
}

bool Rasterizer::renderFirstDirectionalShadowMap(Buffer<double>& depthBuffer) {
  if (!m_scene || !m_camera)
    return false;
  return RasterShadowMapBuilder(*this, m_scene, m_camera, *p->threadPool, m_cancelled)
    .renderFirstDirectionalDepth(depthBuffer);
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
  const render::TilePlan tilePlan =
    render::TilePlan::forBuffer(hdr.width(), hdr.height(), p->lastResolvedQueueSize);
  if (tilePlan.isSingleTile()) {
    writeTonemappedTile(buffer, hdr, *outputTonemap, tilePlan.fullRect());
  } else {
    engine::dispatchTileTasks(tilePlan, *p->threadPool, p->tasks,
                              [&](const Recti& rect, std::size_t) {
                                writeTonemappedTile(buffer, hdr, *outputTonemap, rect);
                              });
  }
}

void Rasterizer::render(Buffer<Colord>& buffer) {
  // Caller is expected to call uncancel() between renders. Matches
  // the Wireframe / Raytracer convention.
  const auto renderStart = RasterClock::now();

  std::unique_ptr<Buffer<Colord>> transientColor;
  Buffer<Colord>* colorTarget = &buffer;
  if (m_colorStoreOp == AttachmentStoreOp::Discard) {
    transientColor = std::make_unique<Buffer<Colord>>(buffer.width(), buffer.height());
    colorTarget = transientColor.get();
  }

  p->resetMetrics(*this, colorTarget->width(), colorTarget->height());
  loadColorAttachment(*this, *colorTarget, buffer);
  clearDiagnosticOutputsForRender(*this, colorTarget->width(), colorTarget->height());

  auto finishMetrics = [&]() {
    p->publishFragmentMetrics(*this);
    p->publishDiagnosticImageStatistics(*this);
    m_lastMetrics.timings.totalRenderSeconds = render::detail::secondsBetween(renderStart, RasterClock::now());
  };

  if (!m_scene || !m_camera) {
    finishMetrics();
    return;
  }

  // Same view-plane setup the other engines perform — the camera
  // projection math depends on the cached basis vectors.
  const Recti viewport = configuredViewportRect(*this, colorTarget->rect());
  if (rasterRectEmpty(viewport)) {
    finishMetrics();
    return;
  }
  m_camera->viewPlane()->setup(m_camera->matrix(), viewport);

  // From here down the render is expressed in pipeline terms. The
  // Rasterizer object contributes configuration; Private drives the
  // concrete passes and keeps task state available for activeTiles().
  p->tasks.clear();
  p->renderFrame(*this, m_scene, m_camera, m_cancelled, *colorTarget);
  finishMetrics();
}

RasterTriangleSet Rasterizer::Private::collectRasterTriangles(
  const RasterTriangleEmitter& triangleEmitter, const render::TilePlan& tilePlan,
  bool shouldSortFrontToBack, double* tileBinningSeconds) {
  // The emitter streams triangles, the set owns them and their tile
  // bins. Keeping those roles separate makes the later tile raster
  // pass independent of scene traversal and tessellation.
  std::vector<RasterTriangle> triangles;
  triangleEmitter.forEachTriangle(
    [&](const RasterTriangle& triangle) { triangles.push_back(triangle); });
  if (shouldSortFrontToBack) {
    sortFrontToBack(triangles);
  }

  RasterTriangleSet triangleSet(tilePlan);
  double binningSeconds = 0.0;
  for (const RasterTriangle& triangle : triangles) {
    const auto start = RasterClock::now();
    triangleSet.add(triangle);
    binningSeconds += render::detail::secondsBetween(start, RasterClock::now());
  }
  if (tileBinningSeconds) {
    *tileBinningSeconds += binningSeconds;
  }
  return triangleSet;
}

RasterTriangleSet
Rasterizer::Private::triangleSetForPlan(const std::vector<RasterTriangle>& triangles,
                                        const render::TilePlan& tilePlan,
                                        bool shouldSortFrontToBack) {
  std::vector<RasterTriangle> ordered = triangles;
  if (shouldSortFrontToBack) {
    sortFrontToBack(ordered);
  }
  RasterTriangleSet triangleSet(tilePlan);
  for (const auto& triangle : ordered) {
    triangleSet.add(triangle);
  }
  return triangleSet;
}

RasterQueueChoice Rasterizer::Private::chooseAutomaticQueue(
  const RasterTriangleSet& candidateSet, const render::TilePlan& candidateTilePlan,
  bool sortFrontToBack, int threads, int msaaSamples, std::vector<int>* evaluatedQueueSizes,
  double* adaptiveBinningSeconds) {
  const std::vector<int> queueSizes =
    adaptiveQueueSizes(static_cast<int>(candidateTilePlan.size()), threads);
  std::string lastReason = "single_tile_fallback";

  for (const int queueSize : queueSizes) {
    const render::TilePlan tilePlan =
      queueSize == static_cast<int>(candidateTilePlan.size())
        ? candidateTilePlan
        : render::TilePlan::forBuffer(candidateTilePlan.width(), candidateTilePlan.height(),
                                      queueSize);
    const int actualQueueSize = static_cast<int>(tilePlan.size());
    if (evaluatedQueueSizes) {
      evaluatedQueueSizes->push_back(actualQueueSize);
    }

    RasterTriangleSet triangleSet = [&] {
      if (actualQueueSize == static_cast<int>(candidateTilePlan.size())) {
        return candidateSet;
      }
      const auto rebinStart = RasterClock::now();
      RasterTriangleSet rebinned =
        triangleSetForPlan(candidateSet.triangles(), tilePlan, sortFrontToBack);
      if (adaptiveBinningSeconds) {
        *adaptiveBinningSeconds += render::detail::secondsBetween(rebinStart, RasterClock::now());
      }
      return rebinned;
    }();
    const RasterTilingStats stats = tilingStats(triangleSet, tilePlan);
    if (actualQueueSize == 1) {
      return RasterQueueChoice(std::move(tilePlan), std::move(triangleSet), actualQueueSize,
                               "single_tile", lastReason);
    }

    lastReason = tiledRasterizationRejectionReason(stats, tilePlan.width(), tilePlan.height(),
                                                   actualQueueSize, threads, msaaSamples);
    if (lastReason.empty()) {
      return RasterQueueChoice(std::move(tilePlan), std::move(triangleSet), actualQueueSize,
                               "tiled", "metrics_accepted");
    }
  }

  const render::TilePlan tilePlan =
    render::TilePlan::forBuffer(candidateTilePlan.width(), candidateTilePlan.height(), 1);
  const auto rebinStart = RasterClock::now();
  RasterTriangleSet triangleSet =
    triangleSetForPlan(candidateSet.triangles(), tilePlan, sortFrontToBack);
  if (adaptiveBinningSeconds) {
    *adaptiveBinningSeconds += render::detail::secondsBetween(rebinStart, RasterClock::now());
  }
  return RasterQueueChoice(std::move(tilePlan), std::move(triangleSet), 1, "single_tile",
                           lastReason);
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
    diagnosticViews(rasterizer, tilePlan.width(), tilePlan.height(), metricCounterBuffers(),
                    metricCounterAtomics());
  const AlphaTestState alphaTest{rasterizer.alphaTestEnabled(), rasterizer.alphaFunc(),
                                 rasterizer.alphaReference()};
  const bool conservativeDepthOcclusion = passSupportsConservativeDepthOcclusion(rasterizer);
  const std::string prepassDecision = depthPrepassDecision(rasterizer, tilePlan);
  auto& prepassMetrics = const_cast<Rasterizer&>(rasterizer).m_lastMetrics.depthPrepass;
  prepassMetrics.requested = depthPrepassModeName(rasterizer.depthPrepassMode());
  prepassMetrics.decision = prepassDecision;

  if (prepassDecision == "enabled") {
    prepassMetrics.enabled = true;
    prepassMetrics.inputTriangles += triangleSet.triangles().size();
    const auto prepassStart = RasterClock::now();
    rasterizeDepthOnlyTriangleSetWithPolicies(
      triangleSet, tilePlan, *threadPool, tasks, cancelled, sampleOffset, NoStencilPolicy{},
      DepthWritePolicy<RasterFullBufferView<double>>{
        fullBufferView(passBuffers.depth()),
        DepthState{Rasterizer::DepthFunc::Less, rasterizer.depthBias()}});
    prepassMetrics.prepassSeconds += render::detail::secondsBetween(prepassStart, RasterClock::now());
    if (cancelled.load())
      return;

    const auto colorPassStart = RasterClock::now();
    withPreparedTrianglePolicies(
      scene.get(), rasterizer, shadowMaps, fullBufferView(passBuffers.depth()), stencilView,
      [&](auto, auto, auto fragmentPolicy) {
        withMSAAFragmentShadingPolicy(
          rasterizer, shadeCache, fragmentPolicy, [&](auto msaaFragmentPolicy) {
            rasterizeTriangleSetWithPolicies(
              triangleSet, tilePlan, *threadPool, tasks, cancelled,
              colorOutputPolicy(rasterizer, fullBufferView(passBuffers.color())), renderClip,
              sampleOffset, NoStencilPolicy{},
              DepthReadOnlyPolicy<RasterFullBufferView<double>>{
                fullBufferView(passBuffers.depth()),
                DepthState{Rasterizer::DepthFunc::LessEqual, rasterizer.depthBias()}},
              msaaFragmentPolicy, alphaTest, conservativeDepthOcclusion, diagnostics);
          });
      });
    prepassMetrics.colorPassSeconds += render::detail::secondsBetween(colorPassStart, RasterClock::now());
    prepassMetrics.totalMeasuredSeconds =
      prepassMetrics.prepassSeconds + prepassMetrics.colorPassSeconds;
  } else {
    withPreparedTrianglePolicies(
      scene.get(), rasterizer, shadowMaps, fullBufferView(passBuffers.depth()), stencilView,
      [&](auto stencil, auto depth, auto fragmentPolicy) {
        withMSAAFragmentShadingPolicy(
          rasterizer, shadeCache, fragmentPolicy, [&](auto msaaFragmentPolicy) {
            rasterizeTriangleSetWithPolicies(
              triangleSet, tilePlan, *threadPool, tasks, cancelled,
              colorOutputPolicy(rasterizer, fullBufferView(passBuffers.color())), renderClip,
              sampleOffset, stencil, depth, msaaFragmentPolicy, alphaTest,
              conservativeDepthOcclusion, diagnostics);
          });
      });
  }

  if (depthCapture &&
      core::util::bufferDimensionsMatch(depthCapture, tilePlan.width(), tilePlan.height())) {
    core::util::copyBuffer(*depthCapture, passBuffers.depth());
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
    diagnosticViews(rasterizer, tilePlan.width(), tilePlan.height(), metricCounterBuffers(),
                    metricCounterAtomics());
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

  if (depthCapture &&
      core::util::bufferDimensionsMatch(depthCapture, tilePlan.width(), tilePlan.height())) {
    core::util::copyBuffer(*depthCapture, passBuffers.depth());
  }
}

void Rasterizer::Private::renderTriangleListPass(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const std::vector<RasterTriangle>& triangles, const render::TilePlan& tilePlan,
  const ShadowMaps& shadowMaps, const Recti& renderClip, const std::atomic<bool>& cancelled,
  Buffer<Colord>& buffer, const Vector2d& sampleOffset, Buffer<double>* depthCapture) {
  PassBuffers passBuffers(rasterizer, tilePlan, buffer);
  auto colorView = colorOutputPolicy(rasterizer, fullBufferView(passBuffers.color()));
  auto depthView = fullBufferView(passBuffers.depth());
  RasterFullBufferView<std::uint8_t> stencilView;
  if (passBuffers.stencil()) {
    stencilView = fullBufferView(*passBuffers.stencil());
  }
  const RasterDiagnosticBufferViews diagnostics =
    diagnosticViews(rasterizer, tilePlan.width(), tilePlan.height(), metricCounterBuffers(),
                    metricCounterAtomics());
  const AlphaTestState alphaTest{rasterizer.alphaTestEnabled(), rasterizer.alphaFunc(),
                                 rasterizer.alphaReference()};

  const Recti clipRect = intersectRasterRects(tilePlan.fullRect(), renderClip);
  if (rasterRectEmpty(clipRect))
    return;

  withPreparedTrianglePolicies(scene.get(), rasterizer, shadowMaps, depthView, stencilView,
                               [&](auto stencil, auto depth, auto fragmentPolicy) {
                                 for (const auto& triangle : triangles) {
                                   if (cancelled.load())
                                     return;
                                   rasterizePreparedTriangleWithPolicies(
                                     triangle, clipRect, colorView, sampleOffset, stencil, depth,
                                     fragmentPolicy, alphaTest, diagnostics);
                                 }
                               });

  if (depthCapture &&
      core::util::bufferDimensionsMatch(depthCapture, tilePlan.width(), tilePlan.height())) {
    core::util::copyBuffer(*depthCapture, passBuffers.depth());
  }
}

void Rasterizer::Private::renderSingleSampleFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& tilePlan, const RasterTriangleEmitter& triangleEmitter,
  const ShadowMaps& shadowMaps, const Recti& renderClip, const std::atomic<bool>& cancelled,
  Buffer<Colord>& buffer, const Vector2d& sampleOffset) {
  if (tilePlan.isSingleTile()) {
    renderTriangleStreamPass(
      rasterizer, scene, triangleEmitter, tilePlan, shadowMaps, renderClip, cancelled, buffer,
      sampleOffset,
      rasterizer.postProcessAA() == Rasterizer::PostProcessAA::TAA ? currentDepth.get() : nullptr);
    return;
  }

  double binningSeconds = 0.0;
  const auto collectStart = RasterClock::now();
  const bool sortOpaqueFrontToBack = passSupportsConservativeDepthOcclusion(rasterizer);
  const RasterTriangleSet triangleSet =
    collectRasterTriangles(triangleEmitter, tilePlan, sortOpaqueFrontToBack, &binningSeconds);
  auto& metrics = const_cast<Rasterizer&>(rasterizer).m_lastMetrics;
  metrics.timings.tessellationTriangleEmissionSeconds +=
    std::max(0.0, render::detail::secondsBetween(collectStart, RasterClock::now()) - binningSeconds);
  metrics.timings.tileBinningSeconds += binningSeconds;
  recordTileMetrics(const_cast<Rasterizer&>(rasterizer), triangleSet, tilePlan);
  if (cancelled.load() || triangleSet.empty())
    return;

  renderTriangleSetPass(
    rasterizer, scene, triangleSet, tilePlan, shadowMaps, renderClip, cancelled, buffer,
    sampleOffset, true, nullptr,
    rasterizer.postProcessAA() == Rasterizer::PostProcessAA::TAA ? currentDepth.get() : nullptr);
}

void Rasterizer::Private::renderAutomaticSingleSampleFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& candidateTilePlan, const RasterTriangleEmitter& triangleEmitter,
  const ShadowMaps& shadowMaps, const Recti& renderClip, const std::atomic<bool>& cancelled,
  Buffer<Colord>& buffer, const Vector2d& sampleOffset) {
  double binningSeconds = 0.0;
  const auto collectStart = RasterClock::now();
  const bool sortOpaqueFrontToBack = passSupportsConservativeDepthOcclusion(rasterizer);
  const RasterTriangleSet candidateSet = collectRasterTriangles(
    triangleEmitter, candidateTilePlan, sortOpaqueFrontToBack, &binningSeconds);
  auto& metrics = const_cast<Rasterizer&>(rasterizer).m_lastMetrics;
  metrics.timings.tessellationTriangleEmissionSeconds +=
    std::max(0.0, render::detail::secondsBetween(collectStart, RasterClock::now()) - binningSeconds);
  metrics.timings.tileBinningSeconds += binningSeconds;
  if (cancelled.load() || candidateSet.empty()) {
    lastResolvedQueueSize = 1;
    recordTileMetrics(const_cast<Rasterizer&>(rasterizer), candidateSet, candidateTilePlan);
    recordSchedulingMetrics(const_cast<Rasterizer&>(rasterizer), true,
                            {static_cast<int>(candidateTilePlan.size())}, "single_tile",
                            "cancelled_or_empty");
    return;
  }

  std::vector<int> evaluatedQueueSizes;
  double adaptiveBinningSeconds = 0.0;
  RasterQueueChoice choice = chooseAutomaticQueue(
    candidateSet, candidateTilePlan, sortOpaqueFrontToBack, threadPool->maxThreadCount(),
    rasterizer.msaaSamples(), &evaluatedQueueSizes, &adaptiveBinningSeconds);
  metrics.timings.tileBinningSeconds += adaptiveBinningSeconds;
  lastResolvedQueueSize = choice.queueSize;
  recordTileMetrics(const_cast<Rasterizer&>(rasterizer), choice.triangleSet, choice.tilePlan);
  recordSchedulingMetrics(const_cast<Rasterizer&>(rasterizer), true, evaluatedQueueSizes,
                          choice.decision, choice.reason);
  if (choice.decision == "tiled") {
    renderTriangleSetPass(
      rasterizer, scene, choice.triangleSet, choice.tilePlan, shadowMaps, renderClip, cancelled,
      buffer, sampleOffset, true, nullptr,
      rasterizer.postProcessAA() == Rasterizer::PostProcessAA::TAA ? currentDepth.get() : nullptr);
    return;
  }

  renderTriangleListPass(
    rasterizer, scene, choice.triangleSet.triangles(), choice.tilePlan, shadowMaps, renderClip,
    cancelled, buffer, sampleOffset,
    rasterizer.postProcessAA() == Rasterizer::PostProcessAA::TAA ? currentDepth.get() : nullptr);
}

void Rasterizer::Private::renderMSAAFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& tilePlan, const MSAASamplePattern& pattern,
  const RasterTriangleEmitter& triangleEmitter, const ShadowMaps& shadowMaps,
  const Recti& renderClip, const std::atomic<bool>& cancelled, Buffer<Colord>& buffer) {
  double binningSeconds = 0.0;
  const auto collectStart = RasterClock::now();
  const bool sortOpaqueFrontToBack = passSupportsConservativeDepthOcclusion(rasterizer);
  const RasterTriangleSet triangleSet =
    collectRasterTriangles(triangleEmitter, tilePlan, sortOpaqueFrontToBack, &binningSeconds);
  auto& metrics = const_cast<Rasterizer&>(rasterizer).m_lastMetrics;
  metrics.timings.tessellationTriangleEmissionSeconds +=
    std::max(0.0, render::detail::secondsBetween(collectStart, RasterClock::now()) - binningSeconds);
  metrics.timings.tileBinningSeconds += binningSeconds;
  recordTileMetrics(const_cast<Rasterizer&>(rasterizer), triangleSet, tilePlan);
  if (cancelled.load() || triangleSet.empty())
    return;

  if (tilePlan.isSingleTile()) {
    renderMSAAFullFrame(rasterizer, scene, triangleSet, tilePlan, shadowMaps, renderClip, pattern,
                        cancelled, buffer);
    return;
  }

  prepareMSAATileScratch(rasterizer, tilePlan);
  engine::dispatchTileTasks(tilePlan, *threadPool, tasks,
                            [&](const Recti& rect, std::size_t tileIndex) {
                              renderMSAATile(rasterizer, scene, triangleSet, shadowMaps, renderClip,
                                             pattern, rect, tileIndex, cancelled, buffer);
                            });
}

void Rasterizer::Private::renderAutomaticMSAAFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& candidateTilePlan, const MSAASamplePattern& pattern,
  const RasterTriangleEmitter& triangleEmitter, const ShadowMaps& shadowMaps,
  const Recti& renderClip, const std::atomic<bool>& cancelled, Buffer<Colord>& buffer) {
  double binningSeconds = 0.0;
  const auto collectStart = RasterClock::now();
  const bool sortOpaqueFrontToBack = passSupportsConservativeDepthOcclusion(rasterizer);
  const RasterTriangleSet candidateSet = collectRasterTriangles(
    triangleEmitter, candidateTilePlan, sortOpaqueFrontToBack, &binningSeconds);
  auto& metrics = const_cast<Rasterizer&>(rasterizer).m_lastMetrics;
  metrics.timings.tessellationTriangleEmissionSeconds +=
    std::max(0.0, render::detail::secondsBetween(collectStart, RasterClock::now()) - binningSeconds);
  metrics.timings.tileBinningSeconds += binningSeconds;
  if (cancelled.load() || candidateSet.empty()) {
    lastResolvedQueueSize = 1;
    recordTileMetrics(const_cast<Rasterizer&>(rasterizer), candidateSet, candidateTilePlan);
    recordSchedulingMetrics(const_cast<Rasterizer&>(rasterizer), true,
                            {static_cast<int>(candidateTilePlan.size())}, "single_tile",
                            "cancelled_or_empty");
    return;
  }

  std::vector<int> evaluatedQueueSizes;
  double adaptiveBinningSeconds = 0.0;
  RasterQueueChoice choice = chooseAutomaticQueue(
    candidateSet, candidateTilePlan, sortOpaqueFrontToBack, threadPool->maxThreadCount(),
    rasterizer.msaaSamples(), &evaluatedQueueSizes, &adaptiveBinningSeconds);
  metrics.timings.tileBinningSeconds += adaptiveBinningSeconds;
  lastResolvedQueueSize = choice.queueSize;
  recordTileMetrics(const_cast<Rasterizer&>(rasterizer), choice.triangleSet, choice.tilePlan);
  recordSchedulingMetrics(const_cast<Rasterizer&>(rasterizer), true, evaluatedQueueSizes,
                          choice.decision, choice.reason);
  if (choice.decision == "tiled") {
    prepareMSAATileScratch(rasterizer, choice.tilePlan);
    engine::dispatchTileTasks(
      choice.tilePlan, *threadPool, tasks, [&](const Recti& rect, std::size_t tileIndex) {
        renderMSAATile(rasterizer, scene, choice.triangleSet, shadowMaps, renderClip, pattern, rect,
                       tileIndex, cancelled, buffer);
      });
    return;
  }

  if (choice.triangleSet.empty())
    return;

  renderMSAAFullFrame(rasterizer, scene, choice.triangleSet, choice.tilePlan, shadowMaps,
                      renderClip, pattern, cancelled, buffer);
}

void Rasterizer::Private::renderMSAAFullFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
  const ShadowMaps& shadowMaps, const Recti& renderClip, const MSAASamplePattern& pattern,
  const std::atomic<bool>& cancelled, Buffer<Colord>& buffer) {
  Buffer<Colord> loadedColor(tilePlan.width(), tilePlan.height());
  core::util::copyBuffer(loadedColor, buffer);
  buffer.clear(Colord::black());
  MSAAFragmentShadeCache shadeCache;
  MSAAFragmentShadeCache* shadeCachePtr =
    rasterizer.msaaShadingMode() == Rasterizer::MSAAShadingMode::PerFragment ? &shadeCache
                                                                             : nullptr;
  Buffer<Colord> sampleBuffer(tilePlan.width(), tilePlan.height());
  for (int sampleIndex = 0; sampleIndex != pattern.count; ++sampleIndex) {
    if (cancelled.load())
      return;

    core::util::copyBuffer(sampleBuffer, loadedColor);

    renderTriangleSetPass(rasterizer, scene, triangleSet, tilePlan, shadowMaps, renderClip,
                          cancelled, sampleBuffer, pattern.offsets[sampleIndex], false,
                          shadeCachePtr);

    if (cancelled.load())
      return;
    accumulateMSAASample(buffer, sampleBuffer);
  }

  const auto resolveStart = RasterClock::now();
  resolveMSAA(buffer, pattern.count);
  const_cast<Rasterizer&>(rasterizer).m_lastMetrics.timings.msaaResolveSeconds +=
    render::detail::secondsBetween(resolveStart, RasterClock::now());
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

  MSAATileScratch localScratch;
  MSAATileScratch* scratch = &localScratch;
  if (tileIndex < msaaTileScratch.size() && msaaTileScratch[tileIndex]) {
    scratch = msaaTileScratch[tileIndex].get();
  } else {
    scratch->prepare(rasterizer, rect);
  }
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
    scratch->clearSample(rasterizer, &buffer);

    RasterTileBufferView<std::uint8_t> stencilView;
    if (scratch->stencil()) {
      stencilView = tileBufferView(*scratch->stencil(), rect);
    }
    const RasterDiagnosticBufferViews diagnostics = diagnosticViews(
      rasterizer, buffer.width(), buffer.height(), metricCounterBuffers(), metricCounterAtomics());
    const AlphaTestState alphaTest{rasterizer.alphaTestEnabled(), rasterizer.alphaFunc(),
                                   rasterizer.alphaReference()};
    const bool conservativeDepthOcclusion = passSupportsConservativeDepthOcclusion(rasterizer);

    withPreparedTrianglePolicies(
      scene.get(), rasterizer, shadowMaps, tileBufferView(scratch->depth(), rect), stencilView,
      [&](auto stencil, auto depth, auto fragmentPolicy) {
        withMSAAFragmentShadingPolicy(
          rasterizer, shadeCachePtr, fragmentPolicy, [&](auto msaaFragmentPolicy) {
            rasterizeTileWithPolicies(
              triangleSet, rect, tileIndex,
              colorOutputPolicy(rasterizer, tileBufferView(scratch->sampleColor(), rect)),
              renderClip, pattern.offsets[sampleIndex], cancelled, stencil, depth,
              msaaFragmentPolicy, alphaTest, conservativeDepthOcclusion, diagnostics);
          });
      });

    if (cancelled.load())
      return;
    scratch->accumulateSample();
  }

  scratch->resolveTo(buffer, pattern.count);
}

void Rasterizer::Private::prepareTemporalResources(int width, int height) {
  if (!core::util::bufferDimensionsMatch(historyColor.get(), width, height)) {
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

  if (!core::util::bufferDimensionsMatch(nextHistoryColor.get(), width, height))
    nextHistoryColor = std::make_unique<Buffer<Colord>>(width, height);
  if (!core::util::bufferDimensionsMatch(historyDepth.get(), width, height))
    historyDepth = std::make_unique<Buffer<double>>(width, height);
  if (!core::util::bufferDimensionsMatch(currentDepth.get(), width, height))
    currentDepth = std::make_unique<Buffer<double>>(width, height);
  if (!core::util::bufferDimensionsMatch(motionVectors.get(), width, height))
    motionVectors = std::make_unique<Buffer<Vector2d>>(width, height);
}

void Rasterizer::Private::prepareMSAATileScratch(const Rasterizer& rasterizer,
                                                 const render::TilePlan& tilePlan) {
  if (msaaTileScratch.size() != tilePlan.size()) {
    msaaTileScratch.clear();
    msaaTileScratch.resize(tilePlan.size());
  }

  for (int row = 0; row != tilePlan.rows(); ++row) {
    for (int col = 0; col != tilePlan.cols(); ++col) {
      const Recti rect = tilePlan.rect(row, col);
      if (rect.width() <= 0 || rect.height() <= 0)
        continue;
      const std::size_t tileIndex = tilePlan.index(row, col);
      if (!msaaTileScratch[tileIndex]) {
        msaaTileScratch[tileIndex] = std::make_unique<MSAATileScratch>();
      }
      msaaTileScratch[tileIndex]->prepare(rasterizer, rect);
    }
  }
}

TemporalResetCondition Rasterizer::Private::temporalResetCondition(int width, int height) const {
  if (!core::util::bufferDimensionsMatch(historyColor.get(), width, height) ||
      !core::util::bufferDimensionsMatch(historyDepth.get(), width, height)) {
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
  core::util::copyBuffer(*historyColor, *nextHistoryColor);
  copyDepthHistory(*historyDepth, *currentDepth);
  previousJitter = currentJitter;
  temporalHistoryValid = true;
  temporalCamera = rasterizer.camera().get();
  temporalScene = rasterizer.scene().get();
  temporalInvalidated = false;
  pendingTemporalReset = TemporalResetCondition::None;
  ++temporalFrameIndex;
}

void Rasterizer::Private::renderFrame(Rasterizer& rasterizer,
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
  const int jitterFrame = resetCondition == TemporalResetCondition::None ? temporalFrameIndex : 0;
  const TemporalJitter currentJitter =
    useTemporalAA ? temporalJitterForFrame(jitterFrame) : TemporalJitter();
  const Vector2d sampleOffset(currentJitter.x, currentJitter.y);

  const render::TilePlan tilePlan = render::TilePlan::forBuffer(width, height, queueSize);
  rasterizer.m_lastMetrics.tiling.tileCount = tilePlan.size();
  rasterizer.m_lastMetrics.depthPrepass.requested =
    depthPrepassModeName(rasterizer.depthPrepassMode());
  rasterizer.m_lastMetrics.depthPrepass.decision = depthPrepassDecision(rasterizer, tilePlan);
  const MSAASamplePattern pattern(rasterizer.msaaSamples());
  const RasterTriangleEmitter triangleEmitter(
    scene.get(), camera, rasterizer.lod(), rasterizer, cancelled, rasterizer.cullMode(),
    rasterizer.hasCullModeOverride(), true, rasterizer.visibilitySet(), &rasterizer.m_lastMetrics);
  const ShadowMaps builtShadowMaps =
    rasterizer.m_externalShadowMaps
      ? ShadowMaps()
      : RasterShadowMapBuilder(rasterizer, scene, camera, *threadPool, cancelled).build();
  const ShadowMaps& shadowMaps =
    rasterizer.m_externalShadowMaps ? *rasterizer.m_externalShadowMaps : builtShadowMaps;
  const auto rasterStart = RasterClock::now();
  if (automaticQueueSize && pattern.count > 1) {
    renderAutomaticMSAAFrame(rasterizer, scene, tilePlan, pattern, triangleEmitter, shadowMaps,
                             renderClip, cancelled, buffer);
  } else if (pattern.count > 1) {
    lastResolvedQueueSize = static_cast<int>(tilePlan.size());
    recordSchedulingMetrics(rasterizer, false, {lastResolvedQueueSize}, "explicit_queue_size",
                            "caller_override");
    renderMSAAFrame(rasterizer, scene, tilePlan, pattern, triangleEmitter, shadowMaps, renderClip,
                    cancelled, buffer);
  } else if (automaticQueueSize) {
    renderAutomaticSingleSampleFrame(rasterizer, scene, tilePlan, triangleEmitter, shadowMaps,
                                     renderClip, cancelled, buffer, sampleOffset);
  } else {
    lastResolvedQueueSize = static_cast<int>(tilePlan.size());
    recordSchedulingMetrics(rasterizer, false, {lastResolvedQueueSize}, "explicit_queue_size",
                            "caller_override");
    renderSingleSampleFrame(rasterizer, scene, tilePlan, triangleEmitter, shadowMaps, renderClip,
                            cancelled, buffer, sampleOffset);
  }
  const double rasterElapsed = render::detail::secondsBetween(rasterStart, RasterClock::now());
  const double nonRasterElapsed =
    rasterizer.m_lastMetrics.timings.tessellationTriangleEmissionSeconds +
    rasterizer.m_lastMetrics.timings.tileBinningSeconds +
    rasterizer.m_lastMetrics.timings.msaaResolveSeconds;
  rasterizer.m_lastMetrics.timings.rasterLoopSeconds +=
    std::max(0.0, rasterElapsed - nonRasterElapsed);

  if (cancelled.load()) {
    return;
  }

  const auto postprocessStart = RasterClock::now();
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
  rasterizer.m_lastMetrics.timings.postprocessSeconds +=
    render::detail::secondsBetween(postprocessStart, RasterClock::now());
}
