#include "gtest.h"
#include "core/math/IntegerDecomposition.h"

namespace IntegerDecompositionTest {
  TEST(IntegerDecomposition, ShouldDecomposeOne) {
    IntegerDecomposition d(1);
    ASSERT_EQ(1, d.first());
    ASSERT_EQ(1, d.second());
  }

  TEST(IntegerDecomposition, ShouldDecomposeTwo) {
    IntegerDecomposition d(2);
    ASSERT_EQ(1, d.first());
    ASSERT_EQ(2, d.second());
  }

  TEST(IntegerDecomposition, ShouldDecomposePrime) {
    IntegerDecomposition d(11);
    ASSERT_EQ(1, d.first());
    ASSERT_EQ(11, d.second());
  }
  
  TEST(IntegerDecomposition, ShouldDecomposeSquare) {
    IntegerDecomposition d(16);
    ASSERT_EQ(4, d.first());
    ASSERT_EQ(4, d.second());
  }
  
  TEST(IntegerDecomposition, ShouldDecomposeNumber) {
    IntegerDecomposition d(24);
    ASSERT_EQ(4, d.first());
    ASSERT_EQ(6, d.second());
  }
}
