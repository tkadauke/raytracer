#include "gtest.h"
#include "Color.h"
#include "test/helpers/ColorTestHelper.h"

namespace ColorTest {
  TEST(Color, ShouldInitializeComponentsWithZeros) {
    Color<float> color;
    ASSERT_EQ(0, color[0]);
    ASSERT_EQ(0, color[1]);
    ASSERT_EQ(0, color[2]);
  }

  TEST(Color, ShouldGetAndSetComponent) {
    Color<float> color;
    color.setComponent(0, 1.0);
    ASSERT_EQ(1.0, color.component(0));
  }

  TEST(Color, ShouldGetAndSetComponentWithIndexOperator) {
    Color<float> color;
    color[1] = 1.0;
    ASSERT_EQ(1.0, color[1]);
  }
  
  TEST(Color, ShouldInitializeColorWithValues) {
    Color<float> color(0.5, 1, 1);
    ASSERT_EQ(0.5, color[0]);
    ASSERT_EQ(1,   color[1]);
    ASSERT_EQ(1,   color[2]);
  }

  TEST(Color, ShouldCopyColor) {
    Color<float> color(0.5, 1, 1);

    Color<float> copy = color;
    ASSERT_EQ(color[0], copy[0]);
    ASSERT_EQ(color[1], copy[1]);
    ASSERT_EQ(color[2], copy[2]);
  }

  TEST(Color, ShouldInitializeColorFromCArray) {
    float elements[3] = { 0.5, 1, 1 };
    Color<float> color(elements);
    ASSERT_EQ(0.5, color[0]);
    ASSERT_EQ(1,   color[1]);
    ASSERT_EQ(1,   color[2]);
  }

  TEST(Color, ShouldAdd) {
    Color<float> color1(0.3, 0.1, 0.4), color2(0.1, 0.1, 0.2);
    Color<float> color3 = color1 + color2;
    Color<float> expected(0.4, 0.2, 0.6);
    
    ASSERT_COLOR_NEAR(expected, color3, 0.0001);
  }
  
  TEST(Color, ShouldSubtract) {
    Color<float> color1(0.3, 0.1, 0.4), color2(0.1, 0.1, 0.2);
    Color<float> color3 = color1 - color2;
    Color<float> expected(0.2, 0, 0.2);
    
    ASSERT_COLOR_NEAR(expected, color3, 0.0001);
  }
  
  TEST(Color, ShouldDivideColorByScalar) {
    Color<float> color(0.2, 0.2, 0.2);
    Color<float> divided = color / 2;
    Color<float> expected(0.1, 0.1, 0.1);
  
    ASSERT_COLOR_NEAR(expected, divided, 0.0001);
  }
  
  TEST(Color, ShouldNotAllowDivisionByZero) {
    Color<float> color;
    ASSERT_THROW(color / 0, DivisionByZeroException);
  }
  
  TEST(Color, ShouldMultiplyColorByScalar) {
    Color<float> color(0.2, 0.2, 0.2);
    Color<float> multiplied = color * 2;
    Color<float> expected(0.4, 0.4, 0.4);
  
    ASSERT_COLOR_NEAR(expected, multiplied, 0.0001);
  }
  
  TEST(Color, ShouldMultiplyColorByIntensityColor) {
    Color<float> color(1, 0.5, 1);
    Color<float> intensity(0.5, 0.5, 1);
    Color<float> expected(0.5, 0.25, 1);
    ASSERT_COLOR_NEAR(expected, color * intensity, 0.0001);
  }

  TEST(Color, ShouldReturnTrueWhenComparingTwoDefaultColors) {
    Color<float> first, second;
    ASSERT_TRUE(first == second);
  }
  
  TEST(Color, ShouldCompareColorsForEquality) {
    Color<float> first(1, 1, 0.5), second(1, 1, 0.5);
    ASSERT_TRUE(first == second);
  }
  
  TEST(Color, ShouldCompareColorsForInequality) {
    Color<float> first(0, 0, 1), second(0, 1, 1);
    ASSERT_TRUE(first != second);
  }
  
  TEST(Color, ShouldConvertToRgbValue) {
    Color<float> c1(0, 0, 0);
    ASSERT_EQ(0x00000000, c1.rgb());
    Color<float> c2(1, 1, 1);
    ASSERT_EQ(0x00FFFFFF, c2.rgb());
  }
  
  TEST(Color, ShouldDefineConstants) {
    Color<float> black(0, 0, 0);
    ASSERT_EQ(black, Color<float>::black);

    Color<float> white(1, 1, 1);
    ASSERT_EQ(white, Color<float>::white);
  }
}
