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
#include "test/helpers/CameraTestHelper.h"
#include "test/helpers/GuiTestHelper.h"
#include "test/helpers/OpenGLTestHelper.h"

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

  using test::helpers::standardCamera;

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

    std::shared_ptr<Scene> backFacingRectangleScene() {
      auto scene = std::make_shared<Scene>(Colord(0.05, 0.05, 0.1));
      auto rect = std::make_shared<Rectangle>(Vector3d(-1.0, -1.0, 0.0), Vector3d(2.0, 0.0, 0.0),
                                              Vector3d(0.0, 2.0, 0.0));
      rect->setMaterial(std::make_shared<MatteMaterial>(
        std::make_shared<ConstantColorTexture>(Colord(0.8, 0.4, 0.2))));
      scene->add(rect);
      scene->addLight(
        std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white()));
      return scene;
    }

    struct ChannelStats {
      double meanAbs = 0.0;
      double noisyFraction = 0.0;
    };

    constexpr double kNoisyPixelChannelThreshold = 0.10;

    bool channelDifferenceExceeds(const Colord& a, const Colord& b, double threshold) {
      return std::abs(a.r() - b.r()) > threshold || std::abs(a.g() - b.g()) > threshold ||
             std::abs(a.b() - b.b()) > threshold;
    }

    ChannelStats compareBuffers(const Buffer<Colord>& cpu, const Buffer<Colord>& gpu) {
      ChannelStats stats;
      const int width = cpu.width();
      const int height = cpu.height();
      const std::size_t channels = static_cast<std::size_t>(width) * height * 3;
      const std::size_t pixels = static_cast<std::size_t>(width) * height;
      double sumAbs = 0.0;
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          const Colord& a = cpu[y][x];
          const Colord& b = gpu[y][x];
          sumAbs += std::abs(a.r() - b.r()) + std::abs(a.g() - b.g()) + std::abs(a.b() - b.b());
        }
      }
      const int noisy = cpu.countDifferences(gpu, [](const Colord& a, const Colord& b) {
        return channelDifferenceExceeds(a, b, kNoisyPixelChannelThreshold);
      });
      stats.meanAbs = channels == 0 ? 0.0 : sumAbs / static_cast<double>(channels);
      stats.noisyFraction = pixels == 0 ? 0.0 : static_cast<double>(noisy) / pixels;
      return stats;
    }
  }

  class OpenGLRasterizerParity : public ::testing::GuiTest {};

  TEST_F(OpenGLRasterizerParity, LitSphereMatchesCpuWithinTolerance) {
    SKIP_IF_NO_OPENGL();

    auto scene = litSphereScene();
    auto cam = standardCamera();

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
    SKIP_IF_NO_OPENGL();

    auto scene = litRectangleScene();
    auto cam = standardCamera();

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

  namespace {
    // Big bright colored sphere placed off-axis. The sphere covers a
    // recognizable region of the image; tests check pixels at known
    // positions to catch Y-flips / X-flips / aspect-mapping bugs that
    // the loose CPU↔GPU mean-diff parity tests miss (we already saw a
    // vertical flip slip past those).
    std::shared_ptr<Scene> orientedSphereScene(const Vector3d& center, const Colord& color) {
      auto scene = std::make_shared<Scene>(Colord::white());
      // Radius 1.0 — large enough that at the project's default 8×6
      // view plane and distance=5 camera the sphere covers ~10 pixels
      // diameter in a 64×64 buffer, well clear of the half-edge rows
      // the tests sample.
      auto sphere = std::make_shared<Sphere>(center, 1.0);
      sphere->setMaterial(
        std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(color)));
      scene->add(sphere);
      scene->addLight(
        std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white()));
      return scene;
    }

    bool isColored(const Colord& color, double threshold = 0.5) {
      // "Has any color" means at least one channel is below ~white-1.
      // White background pixels are (1,1,1) (or close to it); a colored
      // sphere pixel has at least one channel much lower.
      const double minChannel = std::min({color.r(), color.g(), color.b()});
      return minChannel < threshold;
    }
  }

  namespace {
    // Find the first column index where the center row is colored.
    int firstColoredColAtCenter(const Buffer<Colord>& buffer) {
      const int centerRow = buffer.height() / 2;
      for (int x = 0; x < buffer.width(); ++x) {
        if (isColored(buffer[centerRow][x])) {
          return x;
        }
      }
      return -1;
    }

    // Find the first row index where the center column is colored.
    int firstColoredRowAtCenter(const Buffer<Colord>& buffer) {
      const int centerCol = buffer.width() / 2;
      for (int y = 0; y < buffer.height(); ++y) {
        if (isColored(buffer[y][centerCol])) {
          return y;
        }
      }
      return -1;
    }
  }

  // Per project convention, world Y+ maps to high screen rows (lower
  // half of the buffer). The dice scene's floor at world Y+1.1 renders
  // at the bottom of the rendered PNG; the test asserts the same.
  // Using `firstColored...` keeps the test robust to small projection /
  // sphere-tessellation rounding differences between CPU and GPU.
  TEST_F(OpenGLRasterizerParity, RendersSphereInLowerHalfWhenCenteredAtPositiveY) {
    SKIP_IF_NO_OPENGL();

    auto scene = orientedSphereScene(Vector3d(0, 1.5, 0), Colord(0.8, 0.2, 0.2));
    auto cam = standardCamera();

    OpenGLRasterizer gpuEngine(cam, scene);
    Buffer<Colord> gpuBuffer(kBufferSize, kBufferSize);
    gpuEngine.render(gpuBuffer);

    const int row = firstColoredRowAtCenter(gpuBuffer);
    ASSERT_GE(row, 0) << "scaffold: sphere not rendering at all in the center column";
    EXPECT_GT(row, kBufferSize / 2)
      << "world Y+ sphere must render below the middle row (got first colored at " << row
      << "); a Y-flip would put it in the upper half";
  }

  TEST_F(OpenGLRasterizerParity, RendersSphereInRightHalfWhenCenteredAtPositiveX) {
    SKIP_IF_NO_OPENGL();

    auto scene = orientedSphereScene(Vector3d(1.5, 0, 0), Colord(0.2, 0.8, 0.2));
    auto cam = standardCamera();

    OpenGLRasterizer gpuEngine(cam, scene);
    Buffer<Colord> gpuBuffer(kBufferSize, kBufferSize);
    gpuEngine.render(gpuBuffer);

    const int col = firstColoredColAtCenter(gpuBuffer);
    ASSERT_GE(col, 0) << "scaffold: sphere not rendering at all in the center row";
    EXPECT_GT(col, kBufferSize / 2)
      << "world X+ sphere must render right of the middle column (got first colored at " << col
      << "); an X-flip would put it in the left half";
  }

  TEST_F(OpenGLRasterizerParity, CpuReferenceRendersSphereInLowerHalfWhenCenteredAtPositiveY) {
    auto scene = orientedSphereScene(Vector3d(0, 1.5, 0), Colord(0.8, 0.2, 0.2));
    auto cam = standardCamera();

    Rasterizer cpuEngine(cam, scene);
    Buffer<Colord> cpuBuffer(kBufferSize, kBufferSize);
    cpuEngine.render(cpuBuffer);

    const int row = firstColoredRowAtCenter(cpuBuffer);
    ASSERT_GE(row, 0) << "scaffold: the sphere isn't rendering at all in the center column";
    EXPECT_GT(row, kBufferSize / 2)
      << "CPU reference: world Y+ must render below the middle row (got first "
         "colored pixel at row "
      << row << "); a Y-flip would put it in the upper half";
  }

  TEST_F(OpenGLRasterizerParity, BackCullModeMatchesCpuOnBackFacingGeometry) {
    SKIP_IF_NO_OPENGL();

    auto scene = backFacingRectangleScene();
    auto cam = standardCamera();

    Rasterizer cpuEngine(cam, scene);
    cpuEngine.setCullMode(Rasterizer::CullMode::Back);
    Buffer<Colord> cpuBuffer(kBufferSize, kBufferSize);
    cpuEngine.render(cpuBuffer);

    OpenGLRasterizer gpuEngine(cam, scene);
    gpuEngine.setCullMode(Rasterizer::CullMode::Back);
    Buffer<Colord> gpuBuffer(kBufferSize, kBufferSize);
    gpuEngine.render(gpuBuffer);

    const ChannelStats stats = compareBuffers(cpuBuffer, gpuBuffer);
    EXPECT_LE(stats.meanAbs, 0.005)
      << "mean channel divergence too large between CPU and OpenGL paths";
    EXPECT_LE(stats.noisyFraction, 0.05)
      << "fraction of pixels with large channel divergence too large";
  }
}
