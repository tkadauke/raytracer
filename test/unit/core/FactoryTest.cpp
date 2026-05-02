#include <gtest/gtest.h>
#include "core/Factory.h"
#include "test/helpers/ContainerTestHelper.h"

using namespace std;

namespace FactoryTest {
  using namespace ::testing;

  struct Shape {
    inline virtual ~Shape() {}
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

    auto shape = f.create("Circle");
    ASSERT_TRUE(dynamic_cast<Circle*>(shape.get()));
    ASSERT_FALSE(dynamic_cast<Rectangle*>(shape.get()));
  }

  TEST(Factory, ShouldReturnNullptrIfIdentifierIsUnknown) {
    Factory<Shape> f;
    f.registerClass<Rectangle>("Rectangle");

    auto shape = f.create("Foobar");
    ASSERT_EQ(nullptr, shape);
  }
  
  TEST(Factory, ShouldReturnIdentifiersSorted) {
    Factory<Shape> f;
    f.registerClass<Rectangle>("Rectangle");
    f.registerClass<Circle>("Circle");

    list<string> identifiers = f.identifiers();
    ASSERT_CONTAINERS_EQ(makeStdList<string>("Circle", "Rectangle"), identifiers);
  }

  TEST(Factory, ShouldReplaceCreatorWhenSameIdRegisteredTwice) {
    // Registering an id that already has an entry must replace the existing
    // Creator (not leak it). Run under ASan / leak-check builds and the
    // previous raw-pointer overwrite would have shown up as a leak.
    Factory<Shape> f;
    f.registerClass<Rectangle>("Shape");
    f.registerClass<Circle>("Shape");

    auto shape = f.create("Shape");
    ASSERT_TRUE(dynamic_cast<Circle*>(shape.get()));
    ASSERT_FALSE(dynamic_cast<Rectangle*>(shape.get()));
  }
}
