#ifndef GPU_TEST_HELPER_H
#define GPU_TEST_HELPER_H

#include <gtest/gtest.h>

#include <array>
#include <type_traits>

namespace test {
  namespace helpers {
    template<typename Record>
    void expectKernelRecordLayout() {
      EXPECT_TRUE(std::is_standard_layout_v<Record>);
      EXPECT_EQ(16u, alignof(Record));
      EXPECT_EQ(0u, sizeof(Record) % 16u);
    }

    inline void expectFloat4(const std::array<float, 4>& actual,
                              float x, float y, float z, float w) {
      EXPECT_FLOAT_EQ(x, actual[0]);
      EXPECT_FLOAT_EQ(y, actual[1]);
      EXPECT_FLOAT_EQ(z, actual[2]);
      EXPECT_FLOAT_EQ(w, actual[3]);
    }
  }
}

#endif
