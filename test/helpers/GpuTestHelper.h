#ifndef GPU_TEST_HELPER_H
#define GPU_TEST_HELPER_H

#include <gtest/gtest.h>

#include "core/math/Vector.h"

#include <array>
#include <cstddef>
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

    inline void expectFloat4Near(const std::array<float, 4>& actual,
                                  const std::array<float, 4>& expected,
                                  float tolerance = 1e-5f) {
      for (std::size_t i = 0; i != actual.size(); ++i)
        EXPECT_NEAR(expected[i], actual[i], tolerance);
    }

    inline void expectVectorNear(const std::array<float, 4>& actual, const Vector4d& expected,
                                  float tolerance = 1e-5f) {
      EXPECT_NEAR(static_cast<float>(expected.x()), actual[0], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.y()), actual[1], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.z()), actual[2], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.w()), actual[3], tolerance);
    }

    inline void expectVectorNear(const std::array<float, 4>& actual, const Vector3d& expected,
                                  float tolerance = 1e-5f) {
      EXPECT_NEAR(static_cast<float>(expected.x()), actual[0], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.y()), actual[1], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.z()), actual[2], tolerance);
      EXPECT_FLOAT_EQ(0.0f, actual[3]);
    }

    inline void expectVectorNear(const std::array<float, 4>& actual, const Vector2d& expected,
                                  float tolerance = 1e-5f) {
      EXPECT_NEAR(static_cast<float>(expected.x()), actual[0], tolerance);
      EXPECT_NEAR(static_cast<float>(expected.y()), actual[1], tolerance);
      EXPECT_FLOAT_EQ(0.0f, actual[2]);
      EXPECT_FLOAT_EQ(0.0f, actual[3]);
    }
  }
}

#endif
