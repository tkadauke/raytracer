#include "gtest.h"
#include "core/formats/ply/PlyProperty.h"

#include <sstream>

using namespace std;

namespace PlyPropertyTest {
  TEST(PlyProperty, ShouldConstructWithSimpleType) {
    PlyProperty property(PlyProperty::INT32, "x");
    ASSERT_EQ("x", property.name());
    ASSERT_FALSE(property.isList());
    ASSERT_EQ(PlyProperty::INT32, property.elementType());
  }
  
  TEST(PlyProperty, ShouldConstructWithListType) {
    PlyProperty property(PlyProperty::UINT8, PlyProperty::INT32, "vertex_index");
    ASSERT_EQ("vertex_index", property.name());
    ASSERT_TRUE(property.isList());
    ASSERT_EQ(PlyProperty::UINT8, property.countType());
    ASSERT_EQ(PlyProperty::INT32, property.elementType());
  }
  
  TEST(PlyProperty, ShouldParseSimpleType) {
    istringstream stream("int32 x");
    PlyProperty property(stream);
    ASSERT_EQ("x", property.name());
    ASSERT_FALSE(property.isList());
    ASSERT_EQ(PlyProperty::INT32, property.elementType());
  }
  
  TEST(PlyProperty, ShouldParseListType) {
    istringstream stream("list uint8 int32 vertex_index");
    PlyProperty property(stream);
    ASSERT_EQ("vertex_index", property.name());
    ASSERT_TRUE(property.isList());
    ASSERT_EQ(PlyProperty::UINT8, property.countType());
    ASSERT_EQ(PlyProperty::INT32, property.elementType());
  }
  
  TEST(PlyProperty, ShouldParseInt8Type) {
    istringstream stream("int8 x");
    PlyProperty property(stream);
    ASSERT_EQ(PlyProperty::INT8, property.elementType());
  }

  TEST(PlyProperty, ShouldParseInt16Type) {
    istringstream stream("int16 x");
    PlyProperty property(stream);
    ASSERT_EQ(PlyProperty::INT16, property.elementType());
  }

  TEST(PlyProperty, ShouldParseInt32Type) {
    istringstream stream("int32 x");
    PlyProperty property(stream);
    ASSERT_EQ(PlyProperty::INT32, property.elementType());
  }
  
  TEST(PlyProperty, ShouldParseUint8Type) {
    istringstream stream("uint8 x");
    PlyProperty property(stream);
    ASSERT_EQ(PlyProperty::UINT8, property.elementType());
  }

  TEST(PlyProperty, ShouldParseUint16Type) {
    istringstream stream("uint16 x");
    PlyProperty property(stream);
    ASSERT_EQ(PlyProperty::UINT16, property.elementType());
  }

  TEST(PlyProperty, ShouldParseUint32Type) {
    istringstream stream("uint32 x");
    PlyProperty property(stream);
    ASSERT_EQ(PlyProperty::UINT32, property.elementType());
  }

  TEST(PlyProperty, ShouldParseFloat32Type) {
    istringstream stream("float32 x");
    PlyProperty property(stream);
    ASSERT_EQ(PlyProperty::FLOAT32, property.elementType());
  }

  TEST(PlyProperty, ShouldParseFloat64Type) {
    istringstream stream("float64 x");
    PlyProperty property(stream);
    ASSERT_EQ(PlyProperty::FLOAT64, property.elementType());
  }
}
