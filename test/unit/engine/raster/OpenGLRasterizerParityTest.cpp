#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/math/Vector.h"
#include "engine/raster/OpenGLOffscreenContext.h"
#include "engine/raster/OpenGLRasterizer.h"
#include "engine/raster/Rasterizer.h"
#include "render/cameras/PinholeCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"
#include "test/helpers/GuiTestHelper.h"

#include <cmath>
#include <cstddef>
#include <memory>

namespace OpenGLRasterizerParityTest {
  using engine::raster::OpenGLOffscreenContext;
  using engine::raster::OpenGLRasterizer;
  using engine::raster::Rasterizer;
  using render::ConstantColorTexture;
  using render::DirectionalLight;
  using render::MatteMaterial;
  using render::PinholeCamera;
  using render::Rectangle;
  using render::Scene;
  using render::Sphere;

  namespace {
    constexpr int kBufferSize = 64;

    std::shared_ptr<Scene> litSphereScene() {
      auto scene = std::make_shared<Scene>(Colord(0.05, 0.05, 0.1));
      auto sphere = std::make_shared<Sphere>(Vector3d::null, 1.0);
      sphere->setMaterial(std::make_shared<MatteMaterial>(
        std::make_shared<ConstantColorTexture>(Colord(0.8, 0.4, 0.2))));
      scene->add(sphere);
      scene->addLight(
        std::make_shared<DirectionalLight>(Vector3d(-0.5, 0.2, -1.0), Colord::white()));
      return scene;
    }

    std::shared_ptr<Scene> litRectangleScene() {
      auto scene = std::make_shared<Scene>(Colord(0.05, 0.05, 0.1));
      auto rect = std::make_shared<Rectangle>(Vector3d(-1.0, -1.0, 0.0), Vector3d(0.0, 2.0, 0.0),
                                              Vector3d(2.0, 0.0, 0.0));
      rect->setMaterial(std::make_shared<MatteMaterial>(
        std::make_shared<ConstantColorTexture>(Colord(0.8, 0.4, 0.2))));
      scene->add(rect);
      scene->addLight(
        std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white()));
      return scene;
    }

    std::shared_ptr<PinholeCamera> camera() {
      return std::make_shared<PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
    }

    struct ChannelStats {
      double meanAbs = 0.0;
      double noisyFraction = 0.0;
    };

    constexpr double kNoisyPixelChannelThreshold = 0.10;

    ChannelStats compareBuffers(const Buffer<Colord>& cpu, const Buffer<Colord>& gpu) {
      ChannelStats stats;
      const int width = cpu.width();
      const int height = cpu.height();
      const std::size_t channels = static_cast<std::size_t>(width) * height * 3;
      const std::size_t pixels = static_cast<std::size_t>(width) * height;
      double sumAbs = 0.0;
      std::size_t noisy = 0;
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const Colord& a = cpu[y][x];
          const Colord& b = gpu[y][x];
          const double dr = std::abs(a.r() - b.r());
          const double dg = std::abs(a.g() - b.g());
          const double db = std::abs(a.b() - b.b());
          sumAbs += dr + dg + db;
          if (dr > kNoisyPixelChannelThreshold || dg > kNoisyPixelChannelThreshold ||
              db > kNoisyPixelChannelThreshold) {
            ++noisy;
          }
        }
      }
      stats.meanAbs = channels == 0 ? 0.0 : sumAbs / static_cast<double>(channels);
      stats.noisyFraction = pixels == 0 ? 0.0 : static_cast<double>(noisy) / pixels;
      return stats;
    }
  }

  class OpenGLRasterizerParity : public ::testing::GuiTest {};

  TEST_F(OpenGLRasterizerParity, LitSphereMatchesCpuWithinTolerance) {
    if (!OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }

    auto scene = litSphereScene();
    auto cam = camera();

    Rasterizer cpuEngine(cam, scene);
    Buffer<Colord> cpuBuffer(kBufferSize, kBufferSize);
    cpuEngine.render(cpuBuffer);

    OpenGLRasterizer gpuEngine(cam, scene);
    Buffer<Colord> gpuBuffer(kBufferSize, kBufferSize);
    gpuEngine.render(gpuBuffer);

    const ChannelStats stats = compareBuffers(cpuBuffer, gpuBuffer);
    EXPECT_LE(stats.meanAbs, 0.01)
      << "mean channel divergence too large between CPU and OpenGL paths";
    EXPECT_LE(stats.noisyFraction, 0.05)
      << "fraction of pixels with large channel divergence too large";
  }

  TEST_F(OpenGLRasterizerParity, FlatRectangleMatchesCpuWithinTolerance) {
    if (!OpenGLOffscreenContext::probe().available()) {
      GTEST_SKIP() << "OpenGL offscreen context unavailable on this host";
    }

    auto scene = litRectangleScene();
    auto cam = camera();

    Rasterizer cpuEngine(cam, scene);
    Buffer<Colord> cpuBuffer(kBufferSize, kBufferSize);
    cpuEngine.render(cpuBuffer);

    OpenGLRasterizer gpuEngine(cam, scene);
    Buffer<Colord> gpuBuffer(kBufferSize, kBufferSize);
    gpuEngine.render(gpuBuffer);

    const ChannelStats stats = compareBuffers(cpuBuffer, gpuBuffer);
    EXPECT_LE(stats.meanAbs, 0.005)
      << "mean channel divergence too large between CPU and OpenGL paths";
    EXPECT_LE(stats.noisyFraction, 0.05)
      << "fraction of pixels with large channel divergence too large";
  }
}
