#include "gtest.h"
#include "core/Factory.h"
#include "test/helpers/ContainerTestHelper.h"

using namespace std;

namespace FactoryTest {
  using namespace ::testing;

  struct Shape {
    virtual ~Shape() {}
  };
  struct Rectangle : public Shape {};
  struct Circle : public Shape {};
  
  TEST(Factory, ShouldInitialize) {
    Factory<Shape> f;
  }
  
  TEST(Factory, ShouldCreateObjectBasedOnIdentifier) {
    Factory<Shape> f;
    f.registerClass<Rectangle>("Rectangle");
    f.registerClass<Circle>("Circle");
    
    Shape* shape = f.create("Circle");
    ASSERT_TRUE(dynamic_cast<Circle*>(shape));
    ASSERT_FALSE(dynamic_cast<Rectangle*>(shape));
  }
  
  TEST(Factory, ShouldReturnZeroIfIdentifierIsUnknown) {
    Factory<Shape> f;
    f.registerClass<Rectangle>("Rectangle");
    
    Shape* shape = f.create("Foobar");
    ASSERT_EQ(0, shape);
  }
  
  TEST(Factory, ShouldReturnIdentifiersSorted) {
    Factory<Shape> f;
    f.registerClass<Rectangle>("Rectangle");
    f.registerClass<Circle>("Circle");
    
    list<string> identifiers = f.identifiers();
    ASSERT_CONTAINERS_EQ(makeStdList<string>("Circle", "Rectangle"), identifiers);
  }
}
