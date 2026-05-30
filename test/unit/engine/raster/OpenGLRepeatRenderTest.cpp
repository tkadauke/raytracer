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
}
