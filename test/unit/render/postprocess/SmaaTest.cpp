#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "render/postprocess/Smaa.h"
#include "test/helpers/BufferTestHelper.h"

namespace SmaaTest {
  using test::helpers::countIntermediatePixels;

  TEST(Smaa, ShouldLeaveFlatImageUnchanged) {
    Buffer<Colord> buffer(8, 8);
    buffer.clear(Colord(0.25, 0.5, 0.75));

    render::postprocess::applySmaa(buffer);

    for (int y = 0; y < buffer.height(); ++y) {
      for (int x = 0; x < buffer.width(); ++x) {
        EXPECT_EQ(Colord(0.25, 0.5, 0.75), buffer[y][x]);
      }
    }
  }

  TEST(Smaa, ShouldSoftenHighContrastVerticalEdge) {
    Buffer<Colord> buffer(8, 8);
    for (int y = 0; y < buffer.height(); ++y) {
      for (int x = 0; x < buffer.width(); ++x) {
        buffer[y][x] = x >= 4 ? Colord::white() : Colord::black();
      }
    }

    ASSERT_EQ(0, countIntermediatePixels(buffer));

    render::postprocess::applySmaa(buffer);

    EXPECT_GT(countIntermediatePixels(buffer), 0);
  }

  TEST(Smaa, ShouldSoftenHighContrastDiagonalEdge) {
    Buffer<Colord> buffer(8, 8);
    for (int y = 0; y < buffer.height(); ++y) {
      for (int x = 0; x < buffer.width(); ++x) {
        buffer[y][x] = x > y ? Colord::white() : Colord::black();
      }
    }

    ASSERT_EQ(0, countIntermediatePixels(buffer));

    render::postprocess::applySmaa(buffer);

    EXPECT_GT(countIntermediatePixels(buffer), 0);
  }

  TEST(Smaa, ShouldLeaveTinyBuffersUnchanged) {
    Buffer<Colord> buffer(2, 2);
    buffer.clear(Colord::black());
    buffer[0][1] = Colord::white();

    render::postprocess::applySmaa(buffer);

    EXPECT_EQ(Colord::black(), buffer[0][0]);
    EXPECT_EQ(Colord::white(), buffer[0][1]);
    EXPECT_EQ(Colord::black(), buffer[1][0]);
    EXPECT_EQ(Colord::black(), buffer[1][1]);
  }
}
