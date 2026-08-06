#include <gtest/gtest.h>

#include "core/util/Bit.h"

namespace core {
  TEST(BitTest, ShouldCountSetBitsInMask) {
    static_assert(countSetBits(0) == 0);
    static_assert(countSetBits(0xFFFFu) == 16);

    EXPECT_EQ(0, countSetBits(0));
    EXPECT_EQ(1, countSetBits(0b0000'0000'0000'1000));
    EXPECT_EQ(5, countSetBits(0b1010'0001'0000'0011));
    EXPECT_EQ(16, countSetBits(0xFFFFu));
  }
}
