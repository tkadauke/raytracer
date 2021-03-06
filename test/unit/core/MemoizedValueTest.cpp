#include "gtest.h"
#include "core/MemoizedValue.h"

namespace MemoizedValueTest {
  TEST(MemoizedValue, ShouldBeUninitializedWhenDefaultConstructorIsCalled) {
    MemoizedValue<int> memo;
    ASSERT_FALSE(memo.isInitialized());
  }
  
  TEST(MemoizedValue, ShouldBeInitializedWhenCalledWithValue) {
    MemoizedValue<int> memo(10);
    ASSERT_TRUE(memo.isInitialized());
  }
  
  TEST(MemoizedValue, ShouldReturnValue) {
    MemoizedValue<int> memo(10);
    ASSERT_EQ(10, memo.value());
  }
  
  TEST(MemoizedValue, ShouldIndicateStatusWhenNotOperatorIsCalled) {
    MemoizedValue<int> uninitialized, initialized(10);
    ASSERT_TRUE(!uninitialized);
    ASSERT_FALSE(!initialized);
  }
  
  TEST(MemoizedValue, ShouldReturnValueWhenConvertedToValueType) {
    MemoizedValue<int> memo(10);
    ASSERT_EQ(10, static_cast<int>(memo));
  }
  
  TEST(MemoizedValue, ShouldAssignValue) {
    MemoizedValue<int> memo;
    memo = 10;
    ASSERT_EQ(10, memo.value());
  }
  
  TEST(MemoizedValue, ShouldReturnValueWhenAssigning) {
    MemoizedValue<int> memo;
    ASSERT_EQ(10, memo = 10);
  }
  
  TEST(MemoizedValue, ShouldBeInitializedAfterAssignment) {
    MemoizedValue<int> memo;
    memo = 10;
    ASSERT_TRUE(memo.isInitialized());
  }
  
  TEST(MemoizedValue, ShouldResetValue) {
    MemoizedValue<int> memo(10);
    ASSERT_TRUE(memo.isInitialized());
    memo.reset();
    ASSERT_FALSE(memo.isInitialized());
  }
}
