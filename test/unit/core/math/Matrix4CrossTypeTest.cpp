#include <gtest/gtest.h>
#include "core/math/Matrix.h"

namespace {
  TEST(Matrix4CrossTypeConstruction, ConvertsDoubleToFloat) {
    Matrix4d m;
    m[0][0] = 1.5;
    m[0][1] = 2.5;
    m[0][2] = 3.5;
    m[0][3] = 4.5;
    m[1][0] = 5.5;
    m[1][1] = 6.5;
    m[1][2] = 7.5;
    m[1][3] = 8.5;

    Matrix4f f(m);
    EXPECT_FLOAT_EQ(1.5f, f[0][0]);
    EXPECT_FLOAT_EQ(7.5f, f[1][2]);

    EXPECT_EQ(f.data()[0], f[0][0]);
    EXPECT_EQ(f.data()[5], f[1][1]);
    EXPECT_EQ(f.data()[15], f[3][3]);
  }
}
