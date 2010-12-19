#include "gtest.h"
#include "Buffer.h"

namespace BufferTest {
  TEST(Buffer, ShouldInitializeBuffer) {
    Buffer buffer(50, 50);
    ASSERT_EQ(Colord::black, buffer[5][5]);
  }
  
  TEST(Buffer, ShouldSetValueAtPixel) {
    Buffer buffer(50, 50);
    buffer[10][5] = Colord::white;
    ASSERT_EQ(Colord::white, buffer[10][5]);
  }
  
  TEST(Buffer, ShouldReturnWidth) {
    Buffer buffer(50, 50);
    ASSERT_EQ(50, buffer.width());
  }
  
  TEST(Buffer, ShouldReturnHeight) {
    Buffer buffer(50, 50);
    ASSERT_EQ(50, buffer.height());
  }
  
  TEST(Buffer, ShouldReturnRect) {
    Buffer buffer(50, 50);
    Rect rect = buffer.rect();
    ASSERT_EQ(0, rect.x());
    ASSERT_EQ(0, rect.y());
    ASSERT_EQ(50, rect.width());
    ASSERT_EQ(50, rect.height());
  }
}
