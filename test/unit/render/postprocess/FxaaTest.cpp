#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "render/postprocess/Fxaa.h"

namespace FxaaTest {
  static int countIntermediatePixels(const Buffer<Colord>& buffer) {
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y) {
      for (int x = 0; x < buffer.width(); ++x) {
        const double r = buffer[y][x].r();
        if (r > 0.0 && r < 1.0)
          ++count;
      }
    }
    return count;
  }

  TEST(Fxaa, ShouldLeaveFlatImageUnchanged) {
    Buffer<Colord> buffer(8, 8);
    buffer.clear(Colord(0.25, 0.5, 0.75));

    render::postprocess::applyFxaa(buffer);

    for (int y = 0; y < buffer.height(); ++y) {
      for (int x = 0; x < buffer.width(); ++x) {
        EXPECT_EQ(Colord(0.25, 0.5, 0.75), buffer[y][x]);
      }
    }
  }

  TEST(Fxaa, ShouldSoftenHighContrastDiagonalEdge) {
    Buffer<Colord> buffer(8, 8);
    for (int y = 0; y < buffer.height(); ++y) {
      for (int x = 0; x < buffer.width(); ++x) {
        buffer[y][x] = x > y ? Colord::white() : Colord::black();
      }
    }

    ASSERT_EQ(0, countIntermediatePixels(buffer));

    render::postprocess::applyFxaa(buffer);

    EXPECT_GT(countIntermediatePixels(buffer), 0);
  }
}
