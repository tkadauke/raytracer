#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "render/denoise/BilateralDenoiser.h"

#include "test/helpers/ColorTestHelper.h"

namespace BilateralDenoiserTest {
  TEST(BilateralDenoiser, ClampsInvalidSettings) {
    render::BilateralDenoiser denoiser(-3, -1.0);

    EXPECT_EQ(0, denoiser.radius());
    EXPECT_GT(denoiser.colorSigma(), 0.0);
  }

  TEST(BilateralDenoiser, SmoothsSimilarColors) {
    render::BilateralDenoiser denoiser(1, 10.0);
    Buffer<Colord> buffer(3, 1);
    buffer[0][0] = Colord::black();
    buffer[0][1] = Colord(3.0, 0.0, 0.0);
    buffer[0][2] = Colord::black();

    denoiser.denoise(buffer);

    EXPECT_GT(buffer[0][0][0], 0.0);
    EXPECT_LT(buffer[0][1][0], 3.0);
    EXPECT_GT(buffer[0][2][0], 0.0);
  }

  TEST(BilateralDenoiser, PreservesHardColorEdges) {
    render::BilateralDenoiser denoiser(1, 0.05);
    Buffer<Colord> buffer(3, 1);
    buffer[0][0] = Colord::black();
    buffer[0][1] = Colord::black();
    buffer[0][2] = Colord::white();

    denoiser.denoise(buffer);

    ASSERT_COLOR_NEAR(Colord::black(), buffer[0][1], 1e-6);
    ASSERT_COLOR_NEAR(Colord::white(), buffer[0][2], 1e-6);
  }

  TEST(BilateralDenoiser, UsesFeatureBuffersToPreserveGeometryEdges) {
    render::BilateralDenoiser denoiser(1, 10.0);
    Buffer<Colord> beauty(3, 1);
    beauty[0][0] = Colord::black();
    beauty[0][1] = Colord(3.0, 0.0, 0.0);
    beauty[0][2] = Colord::black();

    Buffer<Colord> albedo(3, 1);
    albedo[0][0] = Colord::black();
    albedo[0][1] = Colord::white();
    albedo[0][2] = Colord::black();

    render::DenoiserFrame frame(beauty);
    frame.features.albedo = &albedo;

    denoiser.denoiseFrame(frame);

    EXPECT_LT(beauty[0][0][0], 0.01);
    EXPECT_GT(beauty[0][1][0], 2.99);
    EXPECT_LT(beauty[0][2][0], 0.01);
  }

  TEST(BilateralDenoiser, ClonesConfiguration) {
    render::BilateralDenoiser denoiser(3, 0.25);

    auto clone = denoiser.clone();

    ASSERT_NE(nullptr, clone);
    EXPECT_STREQ("bilateral", clone->diagnosticName());
    auto* bilateral = dynamic_cast<render::BilateralDenoiser*>(clone.get());
    ASSERT_NE(nullptr, bilateral);
    EXPECT_EQ(3, bilateral->radius());
    EXPECT_DOUBLE_EQ(0.25, bilateral->colorSigma());
  }

  TEST(BilateralDenoiser, ReportsDiagnostics) {
    render::BilateralDenoiser denoiser(4, 0.2);

    const render::DenoiserDiagnostics diagnostics = denoiser.diagnostics();

    EXPECT_EQ("bilateral", diagnostics.name);
    ASSERT_EQ(2u, diagnostics.numericParameters.size());
    EXPECT_EQ("radius", diagnostics.numericParameters[0].name);
    EXPECT_DOUBLE_EQ(4.0, diagnostics.numericParameters[0].value);
    EXPECT_EQ("color_sigma", diagnostics.numericParameters[1].name);
    EXPECT_DOUBLE_EQ(0.2, diagnostics.numericParameters[1].value);
  }
}
