#include "gtest.h"
#include "Buffer.h"
#include "Color.h"

namespace BufferTest {
  template<class T>
  class BufferTest : public ::testing::Test {
  };

  typedef ::testing::Types<Buffer<Color<float> >, Buffer<Color<double> > > BufferTypes;
  TYPED_TEST_CASE(BufferTest, BufferTypes);
  
  TYPED_TEST(BufferTest, ShouldInitializeBuffer) {
    TypeParam buffer(50, 50);
    ASSERT_EQ(TypeParam::ElementType::black(), buffer[5][5]);
  }
  
  TYPED_TEST(BufferTest, ShouldSetValueAtPixel) {
    TypeParam buffer(50, 50);
    buffer[10][5] = TypeParam::ElementType::white();
    ASSERT_EQ(TypeParam::ElementType::white(), buffer[10][5]);
  }
  
  TYPED_TEST(BufferTest, ShouldGetConstValueAtPixel) {
    TypeParam buffer(50, 50);
    buffer[10][5] = TypeParam::ElementType::white();
    const TypeParam& constBuffer = buffer;
    ASSERT_EQ(TypeParam::ElementType::white(), constBuffer[10][5]);
  }
  
  TYPED_TEST(BufferTest, ShouldReturnWidth) {
    TypeParam buffer(50, 50);
    ASSERT_EQ(50, buffer.width());
  }
  
  TYPED_TEST(BufferTest, ShouldReturnHeight) {
    TypeParam buffer(50, 50);
    ASSERT_EQ(50, buffer.height());
  }
  
  TYPED_TEST(BufferTest, ShouldReturnRect) {
    TypeParam buffer(50, 50);
    Rect rect = buffer.rect();
    ASSERT_EQ(0, rect.x());
    ASSERT_EQ(0, rect.y());
    ASSERT_EQ(50, rect.width());
    ASSERT_EQ(50, rect.height());
  }
}
