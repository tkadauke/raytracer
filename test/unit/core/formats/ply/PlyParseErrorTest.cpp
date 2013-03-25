#include "gtest.h"
#include "core/formats/ply/PlyParseError.h"

namespace PlyParseErrorTest {
  TEST(PlyParseError, ShouldHaveMeaningfulMessage) {
    PlyParseError exception(__FILE__, __LINE__);
  
    ASSERT_EQ("Parse error in PLY file", exception.message());
  }
}
