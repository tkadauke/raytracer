#include "gtest.h"
#include "core/Color.h"
#include "test/helpers/ColorTestHelper.h"

#include <sstream>

using namespace std;

namespace ColorTest {
  template<class T>
  class ColorTest : public ::testing::Test {
  public:
    struct {
      struct { T c, m, y, k; } cmyk;
      struct { T r, g, b; } rgb;
    } cmykCases[8] = {
     	{{0,0,0,1}, {0,0,0}},
     	{{0,0,0,0}, {1,1,1}},
     	{{0,1,1,0}, {1,0,0}},
     	{{1,0,1,0}, {0,1,0}},
     	{{1,1,0,0}, {0,0,1}},
     	{{0,0,1,0}, {1,1,0}},
     	{{1,0,0,0}, {0,1,1}},
     	{{0,1,0,0}, {1,0,1}}
    };
    
    struct {
      struct { unsigned int h; T s, v; } hsv;
      struct { T r, g, b; } rgb;
    } hsvCases[16] = {
     	{{0,0,0}, {0,0,0}},
     	{{0,0,1}, {1,1,1}},
     	{{0,1,1}, {1,0,0}},
     	{{120,1,1}, {0,1,0}},
     	{{240,1,1}, {0,0,1}},
     	{{60,1,1}, {1,1,0}},
     	{{180,1,1}, {0,1,1}},
     	{{300,1,1}, {1,0,1}},
     	{{0,0,0.75}, {0.75,0.75,0.75}},
     	{{0,0,0.5}, {0.5,0.5,0.5}},
     	{{0,1,0.5}, {0.5,0,0}},
     	{{60,1,0.5}, {0.5,0.5,0}},
     	{{120,1,0.5}, {0,0.5,0}},
     	{{300,1,0.5}, {0.5,0,0.5}},
     	{{180,1,0.5}, {0,0.5,0.5}},
     	{{240,1,0.5}, {0,0,0.5}},
    };
  };

  // long double is not by itself an interesting case, but it bypasses the SSE
  // optimizations, so it's used to test the unspecialized Color class
  typedef ::testing::Types<float, double, long double> ColorTypes;
  TYPED_TEST_CASE(ColorTest, ColorTypes);
  
  TYPED_TEST(ColorTest, ShouldInitializeComponentsWithZeros) {
    Color<TypeParam> color;
    ASSERT_EQ(0, color[0]);
    ASSERT_EQ(0, color[1]);
    ASSERT_EQ(0, color[2]);
  }
  
  TYPED_TEST(ColorTest, ShouldInitializeColorWithValues) {
    Color<TypeParam> color(0.5, 1, 1);
    ASSERT_EQ(0.5, color[0]);
    ASSERT_EQ(1,   color[1]);
    ASSERT_EQ(1,   color[2]);
  }
  
  TYPED_TEST(ColorTest, ShouldCopyColor) {
    Color<TypeParam> color(0.5, 1, 1);
  
    Color<TypeParam> copy = color;
    ASSERT_EQ(color[0], copy[0]);
    ASSERT_EQ(color[1], copy[1]);
    ASSERT_EQ(color[2], copy[2]);
  }
  
  TYPED_TEST(ColorTest, ShouldInitializeColorFromCArray) {
    Color<TypeParam> color({ 0.5, 1, 1 });
    ASSERT_EQ(0.5, color[0]);
    ASSERT_EQ(1,   color[1]);
    ASSERT_EQ(1,   color[2]);
  }
  
  TYPED_TEST(ColorTest, ShouldCreateColorFromRGBValues) {
    auto color = Color<TypeParam>::fromRGB(255, 255, 0);
    ASSERT_COLOR_NEAR(Color<TypeParam>(1, 1, 0), color, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldCreateColorFromCMYKValues) {
    for (auto testcase : this->cmykCases) {
      auto cmyk = testcase.cmyk;
      auto rgb = testcase.rgb;
      
      auto actual = Color<TypeParam>::fromCMYK(cmyk.c, cmyk.m, cmyk.y, cmyk.k);
      Color<TypeParam> expected(rgb.r, rgb.g, rgb.b);
      ASSERT_COLOR_NEAR(expected, actual, 0.0001);
    }
  }
  
  TYPED_TEST(ColorTest, ShouldCreateColorFromHSVValues) {
    for (auto testcase : this->hsvCases) {
      auto hsv = testcase.hsv;
      auto rgb = testcase.rgb;
      
      auto actual = Color<TypeParam>::fromHSV(hsv.h, hsv.s, hsv.v);
      Color<TypeParam> expected(rgb.r, rgb.g, rgb.b);
      ASSERT_COLOR_NEAR(expected, actual, 0.0001);
    }
  }
  
  TYPED_TEST(ColorTest, ShouldGetAndSetComponent) {
    Color<TypeParam> color;
    color.setComponent(0, 1.0);
    ASSERT_EQ(1.0, color.component(0));
  }

  TYPED_TEST(ColorTest, ShouldGetAndSetComponentWithIndexOperator) {
    Color<TypeParam> color;
    color[1] = 1.0;
    ASSERT_EQ(1.0, color[1]);
  }
  
  TYPED_TEST(ColorTest, ShouldGetIndividualComponents) {
    Color<TypeParam> color(0.5, 1, 1);
  
    ASSERT_EQ(0.5, color.r());
    ASSERT_EQ(1, color.g());
    ASSERT_EQ(1, color.b());
  }
  
  TYPED_TEST(ColorTest, ShouldGetCMYKComponents) {
    for (auto testcase : this->cmykCases) {
      auto cmyk = testcase.cmyk;
      auto rgb = testcase.rgb;
      
      Color<TypeParam> color(rgb.r, rgb.g, rgb.b);
      ASSERT_NEAR(cmyk.c, color.c(), 0.0001);
      ASSERT_NEAR(cmyk.m, color.m(), 0.0001);
      ASSERT_NEAR(cmyk.y, color.y(), 0.0001);
      ASSERT_NEAR(cmyk.k, color.k(), 0.0001);
    }
  }

  TYPED_TEST(ColorTest, ShouldGetHSVComponents) {
    for (auto testcase : this->hsvCases) {
      auto hsv = testcase.hsv;
      auto rgb = testcase.rgb;
      
      Color<TypeParam> color(rgb.r, rgb.g, rgb.b);
      ASSERT_NEAR(hsv.h, color.h(), 0.0001);
      ASSERT_NEAR(hsv.s, color.s(), 0.0001);
      ASSERT_NEAR(hsv.v, color.v(), 0.0001);
    }
  }
  
  TYPED_TEST(ColorTest, ShouldReturnMax) {
    Color<TypeParam> color(0.5, 1, 1);
    ASSERT_EQ(1, color.max());
  }

  TYPED_TEST(ColorTest, ShouldReturnMin) {
    Color<TypeParam> color(0.5, 1, 1);
    ASSERT_EQ(0.5, color.min());
  }
  
  TYPED_TEST(ColorTest, ShouldAdd) {
    Color<TypeParam> color1(0.3, 0.1, 0.4), color2(0.1, 0.1, 0.2);
    Color<TypeParam> color3 = color1 + color2;
    Color<TypeParam> expected(0.4, 0.2, 0.6);
    
    ASSERT_COLOR_NEAR(expected, color3, 0.0001);
  }

  TYPED_TEST(ColorTest, ShouldInplaceAdd) {
    Color<TypeParam> color1(0.3, 0.1, 0.4), color2(0.1, 0.1, 0.2);
    color1 += color2;
    Color<TypeParam> expected(0.4, 0.2, 0.6);
    
    ASSERT_COLOR_NEAR(expected, color1, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldSubtract) {
    Color<TypeParam> color1(0.3, 0.1, 0.4), color2(0.1, 0.1, 0.2);
    Color<TypeParam> color3 = color1 - color2;
    Color<TypeParam> expected(0.2, 0, 0.2);
    
    ASSERT_COLOR_NEAR(expected, color3, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldDivideColorByScalar) {
    Color<TypeParam> color(0.2, 0.2, 0.2);
    Color<TypeParam> divided = color / 2;
    Color<TypeParam> expected(0.1, 0.1, 0.1);
  
    ASSERT_COLOR_NEAR(expected, divided, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldNotAllowDivisionByZero) {
    Color<TypeParam> color;
    ASSERT_THROW(color / 0, DivisionByZeroException);
  }
  
  TYPED_TEST(ColorTest, ShouldMultiplyColorByScalar) {
    Color<TypeParam> color(0.2, 0.2, 0.2);
    Color<TypeParam> multiplied = color * 2;
    Color<TypeParam> expected(0.4, 0.4, 0.4);
  
    ASSERT_COLOR_NEAR(expected, multiplied, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldMultiplyColorByIntensityColor) {
    Color<TypeParam> color(1, 0.5, 1);
    Color<TypeParam> intensity(0.5, 0.5, 1);
    Color<TypeParam> expected(0.5, 0.25, 1);
    ASSERT_COLOR_NEAR(expected, color * intensity, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldReturnTrueWhenComparingTwoDefaultColors) {
    Color<TypeParam> first, second;
    ASSERT_TRUE(first == second);
  }
  
  TYPED_TEST(ColorTest, ShouldCompareColorsForEquality) {
    Color<TypeParam> first(1, 1, 0.5), second(1, 1, 0.5);
    ASSERT_TRUE(first == second);
  }
  
  TYPED_TEST(ColorTest, ShouldCompareColorsForInequality) {
    Color<TypeParam> first(0, 0, 1), second(0, 1, 1);
    ASSERT_TRUE(first != second);
  }
  
  TYPED_TEST(ColorTest, ShouldConvertToRgbValue) {
    Color<TypeParam> c1(0, 0, 0);
    ASSERT_EQ(0x00000000u, c1.rgb());
    Color<TypeParam> c2(1, 1, 1);
    ASSERT_EQ(0x00FFFFFFu, c2.rgb());
  }
  
  TYPED_TEST(ColorTest, ShouldDefineConstants) {
    Color<TypeParam> black(0, 0, 0);
    ASSERT_EQ(black, Color<TypeParam>::black());
  
    Color<TypeParam> white(1, 1, 1);
    ASSERT_EQ(white, Color<TypeParam>::white());

    Color<TypeParam> red(1, 0, 0);
    ASSERT_EQ(red, Color<TypeParam>::red());

    Color<TypeParam> green(0, 1, 0);
    ASSERT_EQ(green, Color<TypeParam>::green());

    Color<TypeParam> blue(0, 0, 1);
    ASSERT_EQ(blue, Color<TypeParam>::blue());
  }
  
  TYPED_TEST(ColorTest, ShouldStreamColorToString) {
    Color<TypeParam> color(1, 0.5, 1);
    
    ostringstream str;
    str << color;
    
    ASSERT_EQ("(1, 0.5, 1)", str.str());
  }
}
