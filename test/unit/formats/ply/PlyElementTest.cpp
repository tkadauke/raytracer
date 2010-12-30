#include "gtest.h"
#include "formats/ply/PlyElement.h"

#include <sstream>

using namespace std;

namespace PlyElementTest {
  TEST(PlyElement, ShouldConstructWithValues) {
    PlyElement element("vertex", 5);
    ASSERT_EQ("vertex", element.name());
    ASSERT_EQ(5, element.count());
  }
  
  TEST(PlyElement, ShouldParseFromStream) {
    istringstream stream("vertex 5");
    PlyElement element(stream);
    ASSERT_EQ("vertex", element.name());
    ASSERT_EQ(5, element.count());
  }
  
  TEST(PlyElement, ShouldAddProperty) {
    PlyElement element("vertex", 5);
    element.addProperty(PlyProperty(PlyProperty::FLOAT32, "x"));
    ASSERT_EQ(1u, element.properties().size());
  }
}
