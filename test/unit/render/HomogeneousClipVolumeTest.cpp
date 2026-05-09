#include <gtest/gtest.h>

#include "render/HomogeneousClipVolume.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace HomogeneousClipVolumeTest {
  using namespace render;

  struct TestVertex {
    Vector4d clip;
    double attribute;
  };

  TestVertex interpolate(const TestVertex& from, const TestVertex& to, double t) {
    return {from.clip + (to.clip - from.clip) * t,
            from.attribute + (to.attribute - from.attribute) * t};
  }

  const Vector4d& clipOf(const TestVertex& vertex) {
    return vertex.clip;
  }

  TEST(HomogeneousClipVolume, ShouldReturnNoOutCodeForInsideClipPoint) {
    HomogeneousClipVolume volume(0.1);

    ASSERT_EQ(0, volume.outCode(Vector4d(0.0, 0.0, 1.0, 1.0)));
  }

  TEST(HomogeneousClipVolume, ShouldReturnAllOutCodeBitsForUndefinedClipPoint) {
    HomogeneousClipVolume volume(0.1);

    ASSERT_EQ(HomogeneousClipVolume::allBits(), volume.outCode(Vector4d::undefined()));
  }

  TEST(HomogeneousClipVolume, ShouldKeepFullyInsideTriangle) {
    HomogeneousClipVolume volume(0.1);
    const std::array<TestVertex, 3> input = {{
      {Vector4d(0.0, 0.0, 1.0, 1.0), 1.0},
      {Vector4d(0.5, 0.0, 1.0, 1.0), 2.0},
      {Vector4d(0.0, 0.5, 1.0, 1.0), 3.0},
    }};
    std::array<TestVertex, 8> clipped;

    const std::size_t count = volume.clipTriangle(input, clipped, clipOf, interpolate);

    ASSERT_EQ(3u, count);
    EXPECT_DOUBLE_EQ(1.0, clipped[0].attribute);
    EXPECT_DOUBLE_EQ(2.0, clipped[1].attribute);
    EXPECT_DOUBLE_EQ(3.0, clipped[2].attribute);
  }

  TEST(HomogeneousClipVolume, ShouldClipTriangleAndInterpolateAttributes) {
    HomogeneousClipVolume volume(0.1);
    const std::array<TestVertex, 3> input = {{
      {Vector4d(-2.0, 0.0, 1.0, 1.0), 0.0},
      {Vector4d(0.0, 0.0, 1.0, 1.0), 2.0},
      {Vector4d(0.0, 0.5, 1.0, 1.0), 4.0},
    }};
    std::array<TestVertex, 8> clipped;

    const std::size_t count = volume.clipTriangle(input, clipped, clipOf, interpolate);

    ASSERT_EQ(4u, count);
    std::vector<double> boundaryAttributes;
    for (std::size_t i = 0; i != count; ++i) {
      EXPECT_GE(clipped[i].clip.x(), -clipped[i].clip.w());
      if (std::abs(clipped[i].clip.x() + clipped[i].clip.w()) < 1e-12) {
        boundaryAttributes.push_back(clipped[i].attribute);
      }
    }
    std::sort(boundaryAttributes.begin(), boundaryAttributes.end());
    ASSERT_EQ(2u, boundaryAttributes.size());
    EXPECT_DOUBLE_EQ(1.0, boundaryAttributes[0]);
    EXPECT_DOUBLE_EQ(2.0, boundaryAttributes[1]);
  }
}
