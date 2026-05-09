#include "test/functional/support/RaytracerFeatureTest.h"

namespace MatteMaterialTest {
  using namespace ::testing;

  struct MatteMaterialTest : public RaytracerFeatureTest {};

  // Sanity: a red matte sphere in the default white-ambient scene produces
  // red pixels (ambient × texColor).
  TEST_F(MatteMaterialTest, ShouldBeVisible) {
    given("a centered sphere");
    when("i look at the origin");
    then("i should see something");
  }

  // Diffuse texture color carries through to the rendered output.
  TEST_F(MatteMaterialTest, ShouldApplyTextureColor) {
    given("a matte sphere with a blue texture");
    when("i look at the origin");
    then("i should see a blue sphere");
  }

  // Both coefficients zero + no lights → material contributes nothing; the
  // sphere is indistinguishable from empty space.
  TEST_F(MatteMaterialTest, ShouldBeInvisibleWithNoIllumination) {
    given("a matte sphere with ambient 0 and diffuse 0");
    when("i look at the origin");
    then("i should see nothing");
  }

  // Half ambient coefficient produces half-intensity pixels, confirming that
  // the coefficient scales the rendered color linearly.
  TEST_F(MatteMaterialTest, ShouldDimWithLowerAmbientCoefficient) {
    given("a matte sphere with ambient 0.5 and diffuse 1");
    when("i look at the origin");
    then("i should see a dim red sphere");
  }
}
