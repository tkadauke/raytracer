#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/util/BufferUtils.h"

#include <memory>

namespace BufferUtilsTest {

  TEST(BufferUtils, MatchesRawBufferDimensions) {
    Buffer<int> buffer(3, 2);

    EXPECT_TRUE(core::util::bufferDimensionsMatch(&buffer, 3, 2));
    EXPECT_FALSE(core::util::bufferDimensionsMatch(&buffer, 2, 3));
    EXPECT_FALSE(core::util::bufferDimensionsMatch(static_cast<Buffer<int>*>(nullptr), 3, 2));
  }

  TEST(BufferUtils, MatchesUniquePtrBufferDimensions) {
    auto buffer = std::make_unique<Buffer<int>>(4, 5);
    std::unique_ptr<Buffer<int>> empty;

    EXPECT_TRUE(core::util::bufferDimensionsMatch(buffer, 4, 5));
    EXPECT_FALSE(core::util::bufferDimensionsMatch(buffer, 5, 4));
    EXPECT_FALSE(core::util::bufferDimensionsMatch(empty, 4, 5));
  }

  TEST(BufferUtils, ComparesBufferDimensionsAcrossElementTypes) {
    Buffer<int> integers(4, 5);
    Buffer<double> matching(4, 5);
    Buffer<double> different(5, 4);

    EXPECT_TRUE(core::util::bufferDimensionsEqual(integers, matching));
    EXPECT_FALSE(core::util::bufferDimensionsEqual(integers, different));
  }

  TEST(BufferUtils, CopiesBufferCells) {
    Buffer<int> source(2, 2);
    source[0][0] = 1;
    source[0][1] = 2;
    source[1][0] = 3;
    source[1][1] = 4;

    Buffer<int> target(2, 2);
    target.clear(0);

    core::util::copyBuffer(target, source);

    EXPECT_EQ(1, target[0][0]);
    EXPECT_EQ(2, target[0][1]);
    EXPECT_EQ(3, target[1][0]);
    EXPECT_EQ(4, target[1][1]);
  }

}
