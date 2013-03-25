#include "test/functional/support/RaytracerFeatureTest.h"

namespace MeshTest {
  using namespace ::testing;
  
  class MeshTest : public RaytracerFeatureTest {};
  
  TEST_F(MeshTest, ShouldBeVisibileInFrontOfTheCamera) {
    given("a centered cube mesh");
    when("i look at the origin");
    then("i should see the mesh");
  }
  
  TEST_F(MeshTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced cube mesh");
    when("i look at the origin");
    then("i should not see the mesh");
  }

  TEST_F(MeshTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered cube mesh");
    when("i look away from the origin");
    then("i should not see the mesh");
  }
}
