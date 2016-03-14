#include "gtest.h"
#include "core/ScopeExit.h"
#include "test/helpers/TypeTestHelper.h"

namespace ScopeExitTest {
  TEST(ScopeExit, ShouldCallFunctionOnExit) {
    int i = 0;
    {
      ScopeExit sx([&] { i = 42; });
      ASSERT_EQ(0, i);
    }
    
    ASSERT_EQ(42, i);
  }

  TEST(ScopeExit, ShouldCallFunctionsInReverseOrder) {
    int i = 2;
    {
      ScopeExit sx1([&] { i += 4; });
      ScopeExit sx2([&] { i /= 2; });
    }
    
    ASSERT_EQ((2 / 2) + 4, i);
  }
}
