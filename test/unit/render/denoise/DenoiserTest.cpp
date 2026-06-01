#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "render/denoise/Denoiser.h"

#include "test/helpers/ColorTestHelper.h"

namespace DenoiserTest {
  class RecordingDenoiser final : public render::Denoiser {
  public:
    std::unique_ptr<render::Denoiser> clone() const override {
      return std::make_unique<RecordingDenoiser>();
    }

    const char* diagnosticName() const override {
      return "recording";
    }

    void denoiseFrame(render::DenoiserFrame& frame) const override {
      sawAlbedo = frame.features.albedo != nullptr;
      sawNormal = frame.features.normal != nullptr;
      sawDepth = frame.features.depth != nullptr;
      frame.beauty.clear(Colord(0.25, 0.5, 0.75));
    }

    mutable bool sawAlbedo{false};
    mutable bool sawNormal{false};
    mutable bool sawDepth{false};
  };

  TEST(Denoiser, BufferConvenienceBuildsBeautyOnlyFrame) {
    RecordingDenoiser denoiser;
    Buffer<Colord> beauty(2, 1);

    denoiser.denoise(beauty);

    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 0.75), beauty[0][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 0.75), beauty[0][1], 1e-12);
    EXPECT_FALSE(denoiser.sawAlbedo);
    EXPECT_FALSE(denoiser.sawNormal);
    EXPECT_FALSE(denoiser.sawDepth);
  }

  TEST(DenoiserFrame, CarriesOptionalFeatureBuffers) {
    RecordingDenoiser denoiser;
    Buffer<Colord> beauty(1, 1);
    Buffer<Colord> albedo(1, 1);
    Buffer<Vector3d> normal(1, 1);
    Buffer<double> depth(1, 1);

    render::DenoiserFrame frame(beauty);
    frame.features.albedo = &albedo;
    frame.features.normal = &normal;
    frame.features.depth = &depth;
    denoiser.denoiseFrame(frame);

    EXPECT_TRUE(denoiser.sawAlbedo);
    EXPECT_TRUE(denoiser.sawNormal);
    EXPECT_TRUE(denoiser.sawDepth);
    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 0.75), beauty[0][0], 1e-12);
  }
}
