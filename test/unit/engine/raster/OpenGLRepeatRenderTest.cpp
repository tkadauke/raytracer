#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "engine/raster/OpenGLOffscreenContext.h"
#include "engine/raster/OpenGLRasterizer.h"
#include "engine/raster/Rasterizer.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"
#include "test/helpers/GuiTestHelper.h"

#include <QThread>

#include <chrono>
#include <iostream>
#include <memory>

namespace OpenGLRepeatRenderTest {
  using namespace engine::raster;
  using namespace render;

  class OpenGLRepeatRender : public ::testing::GuiTest {};

  namespace {
    std::shared_ptr<Scene> sphereScene() {
      auto scene = std::make_shared<Scene>(Colord(0.1, 0.1, 0.1));
      auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);
      sphere->setMaterial(
        std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::red())));
      scene->add(sphere);
      scene->addLight(std::make_shared<DirectionalLight>(Vector3d(0, 0, -1), Colord::white()));
      return scene;
    }

    std::shared_ptr<PinholeCamera> camera() {
      return std::make_shared<PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
    }

    template<typename Engine>
    double timeRender(Engine& engine, Buffer<Colord>& buffer) {
      const auto t0 = std::chrono::steady_clock::now();
      engine.render(buffer);
      const auto t1 = std::chrono::steady_clock::now();
      return std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
  }

  TEST_F(OpenGLRepeatRender, CpuVsGpuTimingSweep) {
    if (!OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "no offscreen GL";
    }
    for (int size : {512, 1024, 1920}) {
      Buffer<Colord> bufCpu(size, size);
      Buffer<Colord> bufGpu(size, size);
      Rasterizer cpu(camera(), sphereScene());
      OpenGLRasterizer gpu(camera(), sphereScene());

      cpu.render(bufCpu);
      gpu.render(bufGpu);

      double cpuTotal = 0.0, gpuTotal = 0.0;
      const int iterations = 5;
      for (int i = 0; i < iterations; ++i) {
        cpuTotal += timeRender(cpu, bufCpu);
      }
      for (int i = 0; i < iterations; ++i) {
        gpuTotal += timeRender(gpu, bufGpu);
      }
      std::cerr << "size=" << size << "x" << size << " cpu_mean=" << (cpuTotal / iterations)
                << " ms"
                << " gpu_mean=" << (gpuTotal / iterations) << " ms"
                << " ratio=" << (gpuTotal / cpuTotal) << "x\n";
    }
  }

  TEST_F(OpenGLRepeatRender, GpuFreshInstanceReusesSharedCache) {
    if (!OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "no offscreen GL";
    }
    // Mirrors the Modeler/graph-engine pattern: every frame constructs a
    // fresh OpenGLRasterizer via RasterBackend::createEngine, but the
    // sharedResources() cache should make frame 2+ as fast as steady-state
    // on a single retained instance.
    Buffer<Colord> buffer(1024, 1024);

    double firstFrame = 0.0;
    double subsequentTotal = 0.0;
    const int iterations = 5;
    for (int i = 0; i < iterations + 1; ++i) {
      OpenGLRasterizer fresh(camera(), sphereScene());
      const double ms = timeRender(fresh, buffer);
      if (i == 0) {
        firstFrame = ms;
      } else {
        subsequentTotal += ms;
      }
    }
    const double subsequentMean = subsequentTotal / iterations;
    std::cerr << "fresh-instance pattern at 1024x1024: first=" << firstFrame
              << " ms, subsequent_mean=" << subsequentMean << " ms ("
              << (firstFrame / subsequentMean) << "x speedup vs first)\n";

    // The whole point of OpenGLRasterizer::sharedResources(): the
    // Modeler/graph-engine pattern constructs a brand-new rasterizer per
    // pass per frame. If the cache regresses to per-instance every
    // fresh-instance render pays the ~70ms cold shader compile + texture
    // upload cost at 1024x1024. Pin the absolute speed at <=25ms so the
    // assertion catches the regression even when an earlier test has
    // already warmed the shared cache.
    EXPECT_LT(subsequentMean, 25.0)
      << "fresh-instance subsequent renders should reuse the shared cache; mean was "
      << subsequentMean << " ms (cold would be ~70 ms)";
  }

  namespace {
    class OneShotRenderThread : public QThread {
    public:
      OneShotRenderThread(int width, int height, std::shared_ptr<Scene> scene_,
                          std::shared_ptr<PinholeCamera> cam_)
          : m_buffer(width, height),
            m_scene(std::move(scene_)),
            m_cam(std::move(cam_)) {
      }

      void run() override {
        OpenGLRasterizer fresh(m_cam, m_scene);
        const auto t0 = std::chrono::steady_clock::now();
        fresh.render(m_buffer);
        const auto t1 = std::chrono::steady_clock::now();
        m_elapsedMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
      }

      double m_elapsedMs{0.0};
      Buffer<Colord> m_buffer;
      std::shared_ptr<Scene> m_scene;
      std::shared_ptr<PinholeCamera> m_cam;
    };
  }

  TEST_F(OpenGLRepeatRender, GpuSharedCacheSurvivesPerFrameWorkerThreads) {
    if (!OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "no offscreen GL";
    }
    // Mirrors the Modeler render-job pattern: each render is dispatched
    // to a fresh QThread that exits immediately after the render finishes.
    // The shared OpenGLRasterResourceCache lives across threads through
    // OpenGLOffscreenContext::detachFromCurrentThread() /
    // migrateToCurrentThread() so the second-and-later renders reuse the
    // linked program, image textures, and VBOs instead of paying the
    // ~70 ms cold cost every frame.
    auto scene = sphereScene();
    auto cam = camera();

    double firstFrame = 0.0;
    double subsequentTotal = 0.0;
    const int iterations = 5;
    for (int i = 0; i < iterations + 1; ++i) {
      OneShotRenderThread t(1024, 1024, scene, cam);
      t.start();
      t.wait();
      if (i == 0) {
        firstFrame = t.m_elapsedMs;
      } else {
        subsequentTotal += t.m_elapsedMs;
      }
    }
    const double subsequentMean = subsequentTotal / iterations;
    std::cerr << "per-frame-worker-thread pattern at 1024x1024: first=" << firstFrame
              << " ms, subsequent_mean=" << subsequentMean << " ms ("
              << (firstFrame / subsequentMean) << "x speedup vs first)\n";

    EXPECT_LT(subsequentMean, 25.0)
      << "shared cache should survive across one-shot render threads; mean was " << subsequentMean
      << " ms (cold per-frame would be ~70 ms)";
  }
}
