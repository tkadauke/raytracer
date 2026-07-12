#include <gtest/gtest.h>
#include "render/brdf/GlossySpecular.h"

#include "core/math/Constants.h"
#include "core/math/HitPoint.h"
#include "test/helpers/PrimitiveTestHelper.h"

namespace GlossySpecularTest {
  using namespace render;
  using test::helpers::unitBox;

  static render::Box* box = unitBox();

  TEST(GlossySpecular, ShouldInitialize) {
    GlossySpecular glossy;
    ASSERT_EQ(1, glossy.specularCoefficient());
  }

  TEST(GlossySpecular, ShouldSetSpecularColor) {
    GlossySpecular glossy;
    glossy.setSpecularColor(Colord(1, 0, 0));
    ASSERT_EQ(Colord(1, 0, 0), glossy.specularColor());
  }

  TEST(GlossySpecular, ShouldSetSpecularCoefficient) {
    GlossySpecular glossy;
    glossy.setSpecularCoefficient(0.2);
    ASSERT_EQ(0.2, glossy.specularCoefficient());

    glossy.setSpecularCoefficient(-4);
    ASSERT_EQ(0, glossy.specularCoefficient());

    glossy.setSpecularCoefficient(12);
    ASSERT_EQ(1, glossy.specularCoefficient());
  }

  TEST(GlossySpecular, ShouldSetExponent) {
    GlossySpecular glossy;
    glossy.setExponent(128);
    ASSERT_EQ(128, glossy.exponent());
  }

  TEST(GlossySpecular, ShouldHaveBlackReflectance) {
    GlossySpecular glossy;

    ASSERT_EQ(Colord::black(), glossy.reflectance(HitPoint::undefined(), Vector3d::null));
  }

  TEST(GlossySpecular, ShouldBeBlackOutsideLobe) {
    GlossySpecular glossy;
    glossy.setSpecularColor(Colord(1, 0, 0));
    glossy.setExponent(128);

    HitPoint point(box, 1, Vector4d::null, Vector3d::up());

    ASSERT_EQ(Colord::black(), glossy(point, -Vector3d::up(), Vector3d::up()));
  }

  TEST(GlossySpecular, ShouldBeBrightInsideLobe) {
    GlossySpecular glossy;
    glossy.setSpecularColor(Colord(1, 0, 0));
    glossy.setExponent(128);

    HitPoint point(box, 1, Vector4d::null, Vector3d::up());

    ASSERT_EQ(Colord(1, 0, 0), glossy(point, Vector3d::forward(), -Vector3d::forward()));
  }

  TEST(GlossySpecular, ShouldExposeGlossyReflectionFlags) {
    GlossySpecular glossy;

    ASSERT_TRUE(glossy.isGlossy());
    ASSERT_TRUE(glossy.isReflection());
    ASSERT_FALSE(glossy.isDelta());
    ASSERT_FALSE(glossy.isTransmission());
  }

  TEST(GlossySpecular, ShouldSamplePhongLobeWithMatchingPdf) {
    GlossySpecular glossy;
    glossy.setSpecularColor(Colord(0.25, 0.5, 1.0));
    glossy.setSpecularCoefficient(0.6);
    glossy.setExponent(8.0);

    HitPoint point(box, 1, Vector4d::null, Vector3d::up());

    Vector3d sampledDirection;
    double sampledPdf = 0.0;
    const Colord value =
      glossy.sample(point, Vector3d::forward(), sampledDirection, sampledPdf, Vector2d(1.0, 0.0));

    ASSERT_TRUE(sampledDirection.approxEqual(-Vector3d::forward(), 1e-9));
    ASSERT_NEAR((glossy.exponent() + 1.0) * invTAU, sampledPdf, 1e-12);
    ASSERT_NEAR(sampledPdf, glossy.pdf(point, Vector3d::forward(), sampledDirection), 1e-12);
    ASSERT_NEAR(0.25 * 0.6, value.r(), 1e-12);
    ASSERT_NEAR(0.5 * 0.6, value.g(), 1e-12);
    ASSERT_NEAR(1.0 * 0.6, value.b(), 1e-12);
  }
}
