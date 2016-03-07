#include "gtest.h"
#include "raytracer/brdf/GlossySpecular.h"
#include "raytracer/primitives/Box.h"

#include "core/math/HitPoint.h"

namespace GlossySpecularTest {
  using namespace raytracer;

  static Box* box = new Box(Vector3d::null(), Vector3d::one());
  
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
    
    ASSERT_EQ(Colord::black(), glossy.reflectance(HitPoint::undefined(), Vector3d::null()));
  }

  TEST(GlossySpecular, ShouldBeBlackOutsideLobe) {
    GlossySpecular glossy;
    glossy.setSpecularColor(Colord(1, 0, 0));
    glossy.setExponent(128);
    
    HitPoint point(box, 1, Vector4d::null(), Vector3d::up());
    
    ASSERT_EQ(Colord::black(), glossy(point, - Vector3d::up(), Vector3d::up()));
  }

  TEST(GlossySpecular, ShouldBeBrightInsideLobe) {
    GlossySpecular glossy;
    glossy.setSpecularColor(Colord(1, 0, 0));
    glossy.setExponent(128);
    
    HitPoint point(box, 1, Vector4d::null(), Vector3d::up());
    
    ASSERT_EQ(Colord(1, 0, 0), glossy(point, Vector3d::forward(), - Vector3d::forward()));
  }
}
