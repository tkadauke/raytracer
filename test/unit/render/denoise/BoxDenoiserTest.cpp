#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "render/denoise/BoxDenoiser.h"

#include "test/helpers/ColorTestHelper.h"

namespace BoxDenoiserTest {
  TEST(BoxDenoiser, ClampsNegativeRadiusToNoOp) {
    render::BoxDenoiser denoiser(-3);

    EXPECT_EQ(0, denoiser.radius());
  }

  TEST(BoxDenoiser, AveragesNeighborPixelsInsideRadius) {
    render::BoxDenoiser denoiser(1);
    Buffer<Colord> buffer(3, 1);
    buffer[0][0] = Colord::black();
    buffer[0][1] = Colord(3.0, 0.0, 0.0);
    buffer[0][2] = Colord::black();

    denoiser.denoise(buffer);

    ASSERT_COLOR_NEAR(Colord(1.5, 0.0, 0.0), buffer[0][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord(1.0, 0.0, 0.0), buffer[0][1], 1e-12);
    ASSERT_COLOR_NEAR(Colord(1.5, 0.0, 0.0), buffer[0][2], 1e-12);
  }

  TEST(BoxDenoiser, ClonesRadius) {
    render::BoxDenoiser denoiser(2);

    auto clone = denoiser.clone();

    ASSERT_NE(nullptr, clone);
    EXPECT_STREQ("box", clone->diagnosticName());
    auto* box = dynamic_cast<render::BoxDenoiser*>(clone.get());
    ASSERT_NE(nullptr, box);
    EXPECT_EQ(2, box->radius());
  }
}
