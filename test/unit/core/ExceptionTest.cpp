#include "gtest.h"
#include "core/Exception.h"

namespace ExceptionTest {
  TEST(Exception, ShouldInitialize) {
    Exception exception("some message", __FILE__, __LINE__);
  
    ASSERT_EQ("some message", exception.message());
  }
}
