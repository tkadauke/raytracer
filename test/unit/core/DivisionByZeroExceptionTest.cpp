#include "gtest.h"
#include "core/DivisionByZeroException.h"

namespace DivisionByZeroExceptionTest {
  TEST(DivisionByZeroException, ShouldHaveMeaningfulMessage) {
    DivisionByZeroException exception(__FILE__, __LINE__);
  
    ASSERT_EQ("Division by zero", exception.message());
  }
}
