#include <gtest/gtest.h>

#include "render/GpuFloat4.h"

#include <array>

namespace GpuFloat4Test {
  using namespace render;

  TEST(GpuFloat4Test, TransformsPointFromPackedRows) {
    const GpuFloat4 row0{1.0f, 2.0f, 3.0f, 4.0f};
    const GpuFloat4 row1{5.0f, 6.0f, 7.0f, 8.0f};
    const GpuFloat4 row2{9.0f, 10.0f, 11.0f, 12.0f};
    const GpuFloat4 row3{13.0f, 14.0f, 15.0f, 16.0f};

    EXPECT_EQ((GpuFloat4{18.0f, 46.0f, 74.0f, 102.0f}),
              gpuTransformPoint(row0, row1, row2, row3, GpuFloat4{1.0f, 2.0f, 3.0f, 1.0f}));
  }

  TEST(GpuFloat4Test, TransformsPointFromPackedMatrix) {
    const std::array<float, 16> matrix{1.0f, 2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,
                                       9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};

    EXPECT_EQ((GpuFloat4{18.0f, 46.0f, 74.0f, 102.0f}),
              gpuTransformPoint(matrix, GpuFloat4{1.0f, 2.0f, 3.0f, 1.0f}));
  }

  TEST(GpuFloat4Test, TransformsDirectionFromPackedRows) {
    const GpuFloat4 row0{1.0f, 2.0f, 3.0f, 4.0f};
    const GpuFloat4 row1{5.0f, 6.0f, 7.0f, 8.0f};
    const GpuFloat4 row2{9.0f, 10.0f, 11.0f, 12.0f};

    EXPECT_EQ((GpuFloat4{14.0f, 38.0f, 62.0f, 0.0f}),
              gpuTransformDirection(row0, row1, row2, GpuFloat4{1.0f, 2.0f, 3.0f, 1.0f}));
  }

  TEST(GpuFloat4Test, TransformsDirectionFromPackedMatrix) {
    const std::array<float, 16> matrix{1.0f, 2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,
                                       9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f};

    EXPECT_EQ((GpuFloat4{14.0f, 38.0f, 62.0f, 0.0f}),
              gpuTransformDirection(matrix, GpuFloat4{1.0f, 2.0f, 3.0f, 1.0f}));
  }
}
