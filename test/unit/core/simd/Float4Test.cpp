#include "gtest/gtest.h"

#include "core/simd/Float4.h"

#include <array>

namespace Float4Test {
  template<class Backend>
  std::array<float, 4> toArray(core::simd::Float4T<Backend> value) {
    alignas(16) float lanes[4];
    core::simd::store4<Backend>(lanes, value);
    return {lanes[0], lanes[1], lanes[2], lanes[3]};
  }

  template<class Backend>
  void expectArithmeticWorks() {
    alignas(16) const float a[4] = {1.0f, -2.0f, 9.0f, 16.0f};
    alignas(16) const float b[4] = {4.0f, 0.5f, 3.0f, -2.0f};
    alignas(16) const float squares[4] = {4.0f, 9.0f, 16.0f, 25.0f};

    const auto lhs = core::simd::load4<Backend>(a);
    const auto rhs = core::simd::load4<Backend>(b);

    EXPECT_EQ((std::array<float, 4>{5.0f, -1.5f, 12.0f, 14.0f}), toArray<Backend>(lhs + rhs));
    EXPECT_EQ((std::array<float, 4>{-3.0f, -2.5f, 6.0f, 18.0f}), toArray<Backend>(lhs - rhs));
    EXPECT_EQ((std::array<float, 4>{4.0f, -1.0f, 27.0f, -32.0f}), toArray<Backend>(lhs * rhs));
    EXPECT_EQ((std::array<float, 4>{0.25f, -4.0f, 3.0f, -8.0f}), toArray<Backend>(lhs / rhs));
    EXPECT_EQ((std::array<float, 4>{1.0f, -2.0f, 3.0f, -2.0f}),
              toArray<Backend>(core::simd::min(lhs, rhs)));
    EXPECT_EQ((std::array<float, 4>{4.0f, 0.5f, 9.0f, 16.0f}),
              toArray<Backend>(core::simd::max(lhs, rhs)));
    EXPECT_EQ((std::array<float, 4>{2.0f, 3.0f, 4.0f, 5.0f}),
              toArray<Backend>(core::simd::sqrt(core::simd::load4<Backend>(squares))));
  }

  template<class Backend>
  void expectComparisonSelectAndMovemaskWork() {
    alignas(16) const float a[4] = {1.0f, 5.0f, 3.0f, 8.0f};
    alignas(16) const float b[4] = {2.0f, 5.0f, 1.0f, 9.0f};
    alignas(16) const float trueValues[4] = {10.0f, 20.0f, 30.0f, 40.0f};
    alignas(16) const float falseValues[4] = {-10.0f, -20.0f, -30.0f, -40.0f};

    const auto lhs = core::simd::load4<Backend>(a);
    const auto rhs = core::simd::load4<Backend>(b);

    EXPECT_EQ(0b0010, core::simd::movemask(core::simd::cmpEq(lhs, rhs)));
    EXPECT_EQ(0b1101, core::simd::movemask(core::simd::cmpNe(lhs, rhs)));
    EXPECT_EQ(0b1001, core::simd::movemask(core::simd::cmpLt(lhs, rhs)));
    EXPECT_EQ(0b1011, core::simd::movemask(core::simd::cmpLe(lhs, rhs)));
    EXPECT_EQ(0b0100, core::simd::movemask(core::simd::cmpGt(lhs, rhs)));
    EXPECT_EQ(0b0110, core::simd::movemask(core::simd::cmpGe(lhs, rhs)));

    const auto selected =
      core::simd::select(core::simd::cmpGe(lhs, rhs), core::simd::load4<Backend>(trueValues),
                         core::simd::load4<Backend>(falseValues));
    EXPECT_EQ((std::array<float, 4>{-10.0f, 20.0f, 30.0f, -40.0f}), toArray<Backend>(selected));
  }

  template<class Backend>
  void expectMaskLogicWorks() {
    alignas(16) const float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    alignas(16) const float b[4] = {1.0f, 0.0f, 4.0f, 0.0f};

    const auto lhs = core::simd::load4<Backend>(a);
    const auto rhs = core::simd::load4<Backend>(b);
    const auto eq = core::simd::cmpEq(lhs, rhs);
    const auto gt = core::simd::cmpGt(lhs, rhs);

    EXPECT_EQ(0b0000, core::simd::movemask(core::simd::maskAnd(eq, gt)));
    EXPECT_EQ(0b1011, core::simd::movemask(core::simd::maskOr(eq, gt)));
    EXPECT_EQ(0b1011, core::simd::movemask(core::simd::maskXor(eq, gt)));
    EXPECT_EQ(0b1010, core::simd::movemask(core::simd::maskAndNot(eq, gt)));
    EXPECT_EQ(0b1110, core::simd::movemask(core::simd::maskNot(eq)));
  }

  TEST(Float4, NativeBackendArithmeticWorks) {
    expectArithmeticWorks<core::simd::NativeBackend>();
  }

  TEST(Float4, NativeBackendComparisonSelectAndMovemaskWork) {
    expectComparisonSelectAndMovemaskWork<core::simd::NativeBackend>();
  }

  TEST(Float4, NativeBackendMaskLogicWorks) {
    expectMaskLogicWorks<core::simd::NativeBackend>();
  }

  TEST(Float4, ScalarFallbackArithmeticWorks) {
    expectArithmeticWorks<core::simd::ScalarBackend>();
  }

  TEST(Float4, ScalarFallbackComparisonSelectAndMovemaskWork) {
    expectComparisonSelectAndMovemaskWork<core::simd::ScalarBackend>();
  }

  TEST(Float4, ScalarFallbackMaskLogicWorks) {
    expectMaskLogicWorks<core::simd::ScalarBackend>();
  }
}
