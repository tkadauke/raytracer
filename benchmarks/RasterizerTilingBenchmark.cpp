#include <benchmark/benchmark.h>

#include "core/Buffer.h"
#include "engine/raster/Rasterizer.h"
#include "render/TilePlan.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Triangle.h"
#include "render/textures/ConstantColorTexture.h"

#include "engine/raster/detail/RasterPipelineTypes.h"
#include "engine/raster/detail/RasterTriangleEmitter.h"

#include <QThread>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <memory>

namespace {
  using engine::raster::Rasterizer;
  using engine::raster::detail::RasterTriangle;
  using engine::raster::detail::RasterTriangleEmitter;
  using engine::raster::detail::RasterTriangleSet;

  constexpr int kWidth = 640;
  constexpr int kHeight = 480;
  constexpr int kLod = 0;
  constexpr int kThreads = 4;
  constexpr int kTiledQueueSize = 16;

  enum class ProjectedTriangleSize {
    Small,
    Medium,
    Large,
  };

  struct RasterScene {
    std::shared_ptr<render::Scene> scene;
    std::shared_ptr<render::PinholeCamera> camera;
  };

  struct RasterTilingDiagnostics {
    std::size_t triangles{0};
    std::size_t tileReferences{0};
    double projectedBoundsPixels{0.0};
    double maxProjectedBoundsPixels{0.0};
  };

  std::shared_ptr<render::Material> material() {
    return std::make_shared<render::MatteMaterial>(
      std::make_shared<render::ConstantColorTexture>(Colord(0.85, 0.78, 0.62)));
  }

  std::shared_ptr<render::PinholeCamera> cameraForBenchmark() {
    auto camera = std::make_shared<render::PinholeCamera>(Vector3d(0.0, 0.0, -5.0), Vector3d::null);
    camera->setDistance(5.0);
    camera->setZoom(1.0);
    return camera;
  }

  void addTriangle(render::Scene& scene, const std::shared_ptr<render::Material>& mat,
                   const Vector3d& a, const Vector3d& b, const Vector3d& c) {
    auto triangle = std::make_shared<render::Triangle>(a, b, c);
    triangle->setMaterial(mat);
    scene.add(triangle);
  }

  void addGrid(render::Scene& scene, ProjectedTriangleSize size) {
    const auto mat = material();
    const int columns = size == ProjectedTriangleSize::Small ? 40 : 10;
    const int rows = size == ProjectedTriangleSize::Small ? 30 : 8;
    const double viewWidth = 8.0;
    const double viewHeight = 6.0;
    const double cellWidth = viewWidth / static_cast<double>(columns);
    const double cellHeight = viewHeight / static_cast<double>(rows);
    const double left = -viewWidth / 2.0;
    const double top = -viewHeight / 2.0;

    for (int y = 0; y != rows; ++y) {
      for (int x = 0; x != columns; ++x) {
        const double x0 = left + static_cast<double>(x) * cellWidth;
        const double x1 = x0 + cellWidth;
        const double y0 = top + static_cast<double>(y) * cellHeight;
        const double y1 = y0 + cellHeight;

        addTriangle(scene, mat, Vector3d(x0, y0, 0.0), Vector3d(x1, y0, 0.0),
                    Vector3d(x0, y1, 0.0));
        addTriangle(scene, mat, Vector3d(x1, y0, 0.0), Vector3d(x1, y1, 0.0),
                    Vector3d(x0, y1, 0.0));
      }
    }
  }

  void addLargeTriangles(render::Scene& scene) {
    const auto mat = material();
    addTriangle(scene, mat, Vector3d(-4.0, -3.0, 0.0), Vector3d(4.0, -3.0, 0.0),
                Vector3d(-4.0, 3.0, 0.0));
    addTriangle(scene, mat, Vector3d(4.0, -3.0, 0.0), Vector3d(4.0, 3.0, 0.0),
                Vector3d(-4.0, 3.0, 0.0));
  }

  RasterScene buildScene(ProjectedTriangleSize size) {
    RasterScene result{std::make_shared<render::Scene>(), cameraForBenchmark()};
    result.scene->setAmbient(Colord(0.25, 0.25, 0.25));
    result.scene->setBackground(Colord(0.02, 0.02, 0.025));
    result.scene->addLight(
      std::make_shared<render::DirectionalLight>(Vector3d(-0.3, -0.5, -1.0), Colord::white()));

    if (size == ProjectedTriangleSize::Large) {
      addLargeTriangles(*result.scene);
    } else {
      addGrid(*result.scene, size);
    }

    return result;
  }

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

  RasterTilingDiagnostics diagnose(const RasterScene& rasterScene, int queueSize) {
    Rasterizer rasterizer(rasterScene.camera, rasterScene.scene);
    rasterizer.setLod(kLod);
    rasterizer.setCullMode(Rasterizer::CullMode::Both);
    rasterizer.setMaximumThreads(kThreads);
    rasterizer.setQueueSize(queueSize);

    const Recti framebuffer(0, 0, kWidth, kHeight);
    rasterScene.camera->viewPlane()->setup(rasterScene.camera->matrix(), framebuffer);

    std::atomic<bool> cancelled{false};
    const render::TilePlan tilePlan = render::TilePlan::forBuffer(kWidth, kHeight, queueSize);
    RasterTriangleSet triangleSet(tilePlan);
    RasterTilingDiagnostics diagnostics;
    RasterTriangleEmitter emitter(rasterScene.scene.get(), rasterScene.camera, kLod, rasterizer,
                                  cancelled, Rasterizer::CullMode::Both, true, true);
    emitter.forEachTriangle([&](const RasterTriangle& triangle) {
      ++diagnostics.triangles;
      const double area = projectedBoundsArea(triangle);
      diagnostics.projectedBoundsPixels += area;
      diagnostics.maxProjectedBoundsPixels = std::max(diagnostics.maxProjectedBoundsPixels, area);
      triangleSet.add(triangle);
    });

    for (std::size_t tile = 0; tile != tilePlan.size(); ++tile) {
      diagnostics.tileReferences += triangleSet.tileGrid().triangleIndices(tile).size();
    }

    return diagnostics;
  }

  void setDiagnosticCounters(benchmark::State& state, const RasterTilingDiagnostics& diagnostics,
                             int queueSize, int msaaSamples) {
    const double triangles = static_cast<double>(diagnostics.triangles);
    state.counters["triangles"] = triangles;
    state.counters["avg_projected_bbox_px"] =
      triangles > 0.0 ? diagnostics.projectedBoundsPixels / triangles : 0.0;
    state.counters["max_projected_bbox_px"] = diagnostics.maxProjectedBoundsPixels;
    state.counters["tile_refs"] = static_cast<double>(diagnostics.tileReferences);
    state.counters["avg_tiles_per_triangle"] =
      triangles > 0.0 ? static_cast<double>(diagnostics.tileReferences) / triangles : 0.0;
    state.counters["frame_px"] = static_cast<double>(kWidth * kHeight);
    state.counters["threads"] = kThreads;
    state.counters["queue_size"] = queueSize;
    state.counters["msaa_samples"] = msaaSamples;
  }

  void bm_rasterizerTiling(benchmark::State& state, ProjectedTriangleSize size, int queueSize,
                           int msaaSamples) {
    const RasterScene rasterScene = buildScene(size);
    const RasterTilingDiagnostics diagnostics = diagnose(rasterScene, queueSize);

    Rasterizer rasterizer(rasterScene.camera, rasterScene.scene);
    rasterizer.setLod(kLod);
    rasterizer.setCullMode(Rasterizer::CullMode::Both);
    rasterizer.setMaximumThreads(kThreads);
    rasterizer.setQueueSize(queueSize);
    rasterizer.setMSAASamples(msaaSamples);

    Buffer<Colord> buffer(kWidth, kHeight);
    for (auto _ : state) {
      rasterizer.render(buffer);
      benchmark::DoNotOptimize(buffer[0][0]);
      benchmark::ClobberMemory();
    }

    setDiagnosticCounters(state, diagnostics, queueSize, msaaSamples);
    state.SetItemsProcessed(state.iterations() * diagnostics.triangles);
  }
}

BENCHMARK_CAPTURE(bm_rasterizerTiling, small_single_tile_1x, ProjectedTriangleSize::Small, 1, 1);
BENCHMARK_CAPTURE(bm_rasterizerTiling, small_tiled_1x, ProjectedTriangleSize::Small,
                  kTiledQueueSize, 1);
BENCHMARK_CAPTURE(bm_rasterizerTiling, medium_single_tile_1x, ProjectedTriangleSize::Medium, 1, 1);
BENCHMARK_CAPTURE(bm_rasterizerTiling, medium_tiled_1x, ProjectedTriangleSize::Medium,
                  kTiledQueueSize, 1);
BENCHMARK_CAPTURE(bm_rasterizerTiling, large_single_tile_1x, ProjectedTriangleSize::Large, 1, 1);
BENCHMARK_CAPTURE(bm_rasterizerTiling, large_tiled_1x, ProjectedTriangleSize::Large,
                  kTiledQueueSize, 1);
BENCHMARK_CAPTURE(bm_rasterizerTiling, medium_single_tile_4x, ProjectedTriangleSize::Medium, 1, 4);
BENCHMARK_CAPTURE(bm_rasterizerTiling, medium_tiled_4x, ProjectedTriangleSize::Medium,
                  kTiledQueueSize, 4);
