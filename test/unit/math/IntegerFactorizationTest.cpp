#include "gtest.h"
#include "math/IntegerFactorization.h"

namespace IntegerFactorizationTest {
  TEST(IntegerFactorization, ShouldFactorizeOne) {
    ASSERT_TRUE(IntegerFactorization(1).factors().empty());
  }

  TEST(IntegerFactorization, ShouldFactorizePrimes) {
    ASSERT_EQ(2, IntegerFactorization(2).factors().front());
    ASSERT_EQ(3, IntegerFactorization(3).factors().front());
    ASSERT_EQ(5, IntegerFactorization(5).factors().front());
    ASSERT_EQ(7, IntegerFactorization(7).factors().front());
    ASSERT_EQ(11, IntegerFactorization(11).factors().front());
    ASSERT_EQ(13, IntegerFactorization(13).factors().front());
    ASSERT_EQ(17, IntegerFactorization(17).factors().front());
  }
  
  TEST(IntegerFactorization, ShouldFactorizeNumbers) {
    ASSERT_EQ(4, IntegerFactorization(16).factors().size());
    ASSERT_EQ(4, IntegerFactorization(24).factors().size());
  }
}
