#include "test/functional/support/RaytracerFeatureTest.h"

namespace BoxTest {
  using namespace ::testing;
  
  class BoxTest : public RaytracerFeatureTest {};
  
  TEST_F(BoxTest, ShouldBeEmptyByDefault) {
    given("an empty scene");
    when("i look at the origin");
    then("i should see nothing");
  }
  
  TEST_F(BoxTest, ShouldHaveBackgroundColor) {
    given("an empty scene with blue background");
    when("i look at the origin");
    then("i should see only blue");
  }
}
