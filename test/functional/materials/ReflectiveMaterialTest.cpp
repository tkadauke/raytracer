#include "test/functional/support/RaytracerFeatureTest.h"

namespace ReflectiveMaterialTest {
  using namespace ::testing;
  
  struct ReflectiveMaterialTest : public RaytracerFeatureTest {};
  
  TEST_F(ReflectiveMaterialTest, ShouldNotBeVisibleIfPerfectlyReflective) {
    given("a perfectly reflective box");
    when("i look at the origin");
    then("i should see nothing");
  }
  
  TEST_F(ReflectiveMaterialTest, ShouldReflectByDefault) {
    given("a perfectly reflective box");
    given("a sphere behind the camera");
    when("i look at the origin");
    then("i should see the sphere");
  }

  TEST_F(ReflectiveMaterialTest, ShouldNotBeTransparent) {
    given("a perfectly reflective box");
    given("a sphere behind the box");
    when("i look at the origin");
    then("i should see nothing");
  }

  TEST_F(ReflectiveMaterialTest, ShouldFilterColor) {
    given("a reflective box which filters the colors");
    when("i look at the origin");
    then("i should see the color filtered view through the box");
  }
}
