#include <gtest/gtest.h>

#include "render/tonemap/LinearTonemap.h"
#include "render/tonemap/ReinhardTonemap.h"
#include "render/tonemap/AcesTonemap.h"
#include "render/tonemap/TonemapFactory.h"

#include "test/helpers/ColorTestHelper.h"

namespace TonemapTest {
  using namespace render;

  // ---- LinearTonemap ----

  TEST(LinearTonemap, IsIdentityOnLDRColors) {
    LinearTonemap t;
    ASSERT_COLOR_NEAR(Colord(0.5, 0.5, 0.5), t.apply(Colord(0.5, 0.5, 0.5)), 1e-12);
  }

  TEST(LinearTonemap, PassesHDRColorsThroughUnchanged) {
    // LinearTonemap is a pass-through — the only clamping happens
    // downstream in `.rgb()`. This test pins that contract.
    LinearTonemap t;
    ASSERT_COLOR_NEAR(Colord(2.5, 4.0, 100.0), t.apply(Colord(2.5, 4.0, 100.0)), 1e-12);
  }

  // ---- ReinhardTonemap ----

  TEST(ReinhardTonemap, MapsZeroToZero) {
    ReinhardTonemap t;
    ASSERT_COLOR_NEAR(Colord::black(), t.apply(Colord::black()), 1e-12);
  }

  TEST(ReinhardTonemap, MapsLuminanceOneToHalf) {
    // Closed-form: c / (1 + c) at c = 1.0 → 0.5. Pin so anyone
    // changing the formula gets a loud failure.
    ReinhardTonemap t;
    ASSERT_COLOR_NEAR(Colord(0.5, 0.5, 0.5), t.apply(Colord(1.0, 1.0, 1.0)), 1e-12);
  }

  TEST(ReinhardTonemap, BoundsArbitrarilyBrightHDRInputsBelowOne) {
    // The whole point of the operator: any non-negative input maps
    // to [0, 1). At HDR=100 the output is 100/101 ≈ 0.99 — under 1.
    ReinhardTonemap t;
    auto out = t.apply(Colord(100, 100, 100));
    EXPECT_LT(out.r(), 1.0);
    EXPECT_LT(out.g(), 1.0);
    EXPECT_LT(out.b(), 1.0);
    EXPECT_GT(out.r(), 0.99); // clearly close to but below the limit.
  }

  TEST(ReinhardTonemap, AppliesPerChannel) {
    // A red HDR pixel keeps red dominant; saturation is preserved
    // (mostly) for moderate values. Pin per-channel application
    // explicitly so a future refactor to a luminance-based variant
    // doesn't silently change the look.
    ReinhardTonemap t;
    auto out = t.apply(Colord(2.0, 0.0, 0.0));
    EXPECT_GT(out.r(), 0.0);
    EXPECT_DOUBLE_EQ(0.0, out.g());
    EXPECT_DOUBLE_EQ(0.0, out.b());
  }

  // ---- AcesTonemap ----

  TEST(AcesTonemap, MapsZeroToZero) {
    AcesTonemap t;
    ASSERT_COLOR_NEAR(Colord::black(), t.apply(Colord::black()), 1e-12);
  }

  TEST(AcesTonemap, ClampsToUnitRange) {
    // Narkowicz fit can overshoot above 1.0 around the inflection
    // before the asymptote pulls it back. The implementation
    // clamps; pin that so the LDR contract is honoured.
    AcesTonemap t;
    auto out = t.apply(Colord(50, 50, 50));
    EXPECT_LE(out.r(), 1.0);
    EXPECT_LE(out.g(), 1.0);
    EXPECT_LE(out.b(), 1.0);
    EXPECT_GE(out.r(), 0.0);
  }

  TEST(AcesTonemap, ProducesPunchierMidtonesThanReinhard) {
    // ACES is the "filmic" curve: midtones are *brighter* than the
    // gentler Reinhard rational, contrast is higher, and the
    // shoulder hits later — that's the load-bearing perceptual
    // difference between the two. Reinhard at luminance 1 hits
    // exactly 0.5 (the closed-form midpoint); ACES at luminance 1
    // sits well above that. Pin the relationship so a future tweak
    // to either operator's constants makes the regression loud.
    ReinhardTonemap r;
    AcesTonemap a;
    auto rOut = r.apply(Colord(1, 1, 1));
    auto aOut = a.apply(Colord(1, 1, 1));
    EXPECT_NEAR(0.5, rOut.r(), 1e-9);
    EXPECT_GT(aOut.r(), 0.7);
    EXPECT_GT(aOut.r(), rOut.r());
  }

  // ---- Factory ----

  TEST(TonemapFactory, RegistersAllThreeOperators) {
    EXPECT_NE(nullptr, TonemapFactory::self().create("Linear").get());
    EXPECT_NE(nullptr, TonemapFactory::self().create("Reinhard").get());
    EXPECT_NE(nullptr, TonemapFactory::self().create("ACES").get());
  }

  TEST(TonemapFactory, ReturnsCorrectConcreteType) {
    auto linear = TonemapFactory::self().create("Linear");
    auto reinhard = TonemapFactory::self().create("Reinhard");
    auto aces = TonemapFactory::self().create("ACES");
    EXPECT_NE(nullptr, dynamic_cast<LinearTonemap*>(linear.get()));
    EXPECT_NE(nullptr, dynamic_cast<ReinhardTonemap*>(reinhard.get()));
    EXPECT_NE(nullptr, dynamic_cast<AcesTonemap*>(aces.get()));
  }
}
