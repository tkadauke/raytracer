#include "gtest.h"
#include "core/Buffer.h"
#include "core/Color.h"

namespace BufferTest {
  template<class T>
  class BufferTest : public ::testing::Test {
  };

  typedef ::testing::Types<
    double, unsigned int, Color<float>, Color<double>
  > BufferTypes;
  
  TYPED_TEST_CASE(BufferTest, BufferTypes);
  
  TYPED_TEST(BufferTest, ShouldReturnWidth) {
    Buffer<TypeParam> buffer(50, 50);
    ASSERT_EQ(50, buffer.width());
  }
  
  TYPED_TEST(BufferTest, ShouldReturnHeight) {
    Buffer<TypeParam> buffer(50, 50);
    ASSERT_EQ(50, buffer.height());
  }
  
  TYPED_TEST(BufferTest, ShouldReturnRect) {
    Buffer<TypeParam> buffer(50, 50);
    Recti rect = buffer.rect();
    ASSERT_EQ(0, rect.x());
    ASSERT_EQ(0, rect.y());
    ASSERT_EQ(50, rect.width());
    ASSERT_EQ(50, rect.height());
  }
  
  TYPED_TEST(BufferTest, ShouldClear) {
    Buffer<TypeParam> buffer(50, 50);
    buffer.clear();
    ASSERT_TRUE(TypeParam() == buffer[10][10]);
  }

  template<class T>
  class ColorBufferTest : public ::testing::Test {
  };

  typedef ::testing::Types<
    Color<float>, Color<double>
  > ColorBufferTypes;
  
  TYPED_TEST_CASE(ColorBufferTest, ColorBufferTypes);

  TYPED_TEST(ColorBufferTest, ShouldInitializeBuffer) {
    Buffer<TypeParam> buffer(50, 50);
    ASSERT_EQ(Buffer<TypeParam>::ElementType::black(), buffer[5][5]);
  }
  
  TYPED_TEST(ColorBufferTest, ShouldSetValueAtPixel) {
    Buffer<TypeParam> buffer(50, 50);
    buffer[10][5] = Buffer<TypeParam>::ElementType::white();
    ASSERT_EQ(Buffer<TypeParam>::ElementType::white(), buffer[10][5]);
  }
  
  TYPED_TEST(ColorBufferTest, ShouldGetConstValueAtPixel) {
    Buffer<TypeParam> buffer(50, 50);
    buffer[10][5] = Buffer<TypeParam>::ElementType::white();
    const Buffer<TypeParam>& constBuffer = buffer;
    ASSERT_EQ(Buffer<TypeParam>::ElementType::white(), constBuffer[10][5]);
  }
}
