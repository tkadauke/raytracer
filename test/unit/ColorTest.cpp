#include "gtest.h"
#include "Color.h"
#include "test/helpers/ColorTestHelper.h"

namespace ColorTest {
  template<class T>
  class ColorTest : public ::testing::Test {
  };

  typedef ::testing::Types<Color<float>, Color<double> > ColorTypes;
  TYPED_TEST_CASE(ColorTest, ColorTypes);
  
  TYPED_TEST(ColorTest, ShouldInitializeComponentsWithZeros) {
    TypeParam color;
    ASSERT_EQ(0, color[0]);
    ASSERT_EQ(0, color[1]);
    ASSERT_EQ(0, color[2]);
  }
  
  TYPED_TEST(ColorTest, ShouldGetAndSetComponent) {
    TypeParam color;
    color.setComponent(0, 1.0);
    ASSERT_EQ(1.0, color.component(0));
  }
  
  TYPED_TEST(ColorTest, ShouldGetAndSetComponentWithIndexOperator) {
    TypeParam color;
    color[1] = 1.0;
    ASSERT_EQ(1.0, color[1]);
  }
  
  TYPED_TEST(ColorTest, ShouldInitializeColorWithValues) {
    TypeParam color(0.5, 1, 1);
    ASSERT_EQ(0.5, color[0]);
    ASSERT_EQ(1,   color[1]);
    ASSERT_EQ(1,   color[2]);
  }
  
  TYPED_TEST(ColorTest, ShouldCopyColor) {
    TypeParam color(0.5, 1, 1);
  
    TypeParam copy = color;
    ASSERT_EQ(color[0], copy[0]);
    ASSERT_EQ(color[1], copy[1]);
    ASSERT_EQ(color[2], copy[2]);
  }
  
  TYPED_TEST(ColorTest, ShouldInitializeColorFromCArray) {
    typename TypeParam::Component elements[3] = { 0.5, 1, 1 };
    TypeParam color(elements);
    ASSERT_EQ(0.5, color[0]);
    ASSERT_EQ(1,   color[1]);
    ASSERT_EQ(1,   color[2]);
  }
  
  TYPED_TEST(ColorTest, ShouldAdd) {
    TypeParam color1(0.3, 0.1, 0.4), color2(0.1, 0.1, 0.2);
    TypeParam color3 = color1 + color2;
    TypeParam expected(0.4, 0.2, 0.6);
    
    ASSERT_COLOR_NEAR(expected, color3, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldSubtract) {
    TypeParam color1(0.3, 0.1, 0.4), color2(0.1, 0.1, 0.2);
    TypeParam color3 = color1 - color2;
    TypeParam expected(0.2, 0, 0.2);
    
    ASSERT_COLOR_NEAR(expected, color3, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldDivideColorByScalar) {
    TypeParam color(0.2, 0.2, 0.2);
    TypeParam divided = color / 2;
    TypeParam expected(0.1, 0.1, 0.1);
  
    ASSERT_COLOR_NEAR(expected, divided, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldNotAllowDivisionByZero) {
    TypeParam color;
    ASSERT_THROW(color / 0, DivisionByZeroException);
  }
  
  TYPED_TEST(ColorTest, ShouldMultiplyColorByScalar) {
    TypeParam color(0.2, 0.2, 0.2);
    TypeParam multiplied = color * 2;
    TypeParam expected(0.4, 0.4, 0.4);
  
    ASSERT_COLOR_NEAR(expected, multiplied, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldMultiplyColorByIntensityColor) {
    TypeParam color(1, 0.5, 1);
    TypeParam intensity(0.5, 0.5, 1);
    TypeParam expected(0.5, 0.25, 1);
    ASSERT_COLOR_NEAR(expected, color * intensity, 0.0001);
  }
  
  TYPED_TEST(ColorTest, ShouldReturnTrueWhenComparingTwoDefaultColors) {
    TypeParam first, second;
    ASSERT_TRUE(first == second);
  }
  
  TYPED_TEST(ColorTest, ShouldCompareColorsForEquality) {
    TypeParam first(1, 1, 0.5), second(1, 1, 0.5);
    ASSERT_TRUE(first == second);
  }
  
  TYPED_TEST(ColorTest, ShouldCompareColorsForInequality) {
    TypeParam first(0, 0, 1), second(0, 1, 1);
    ASSERT_TRUE(first != second);
  }
  
  TYPED_TEST(ColorTest, ShouldConvertToRgbValue) {
    TypeParam c1(0, 0, 0);
    ASSERT_EQ(0x00000000u, c1.rgb());
    TypeParam c2(1, 1, 1);
    ASSERT_EQ(0x00FFFFFFu, c2.rgb());
  }
  
  TYPED_TEST(ColorTest, ShouldDefineConstants) {
    TypeParam black(0, 0, 0);
    ASSERT_EQ(black, TypeParam::black());
  
    TypeParam white(1, 1, 1);
    ASSERT_EQ(white, TypeParam::white());
  }
}
