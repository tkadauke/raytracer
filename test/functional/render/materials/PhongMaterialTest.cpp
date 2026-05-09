#include "test/functional/support/RaytracerFeatureTest.h"

namespace PhongMaterialTest {
  using namespace ::testing;

  struct PhongMaterialTest : public RaytracerFeatureTest {};

  // A Phong sphere is visible in the default white-ambient scene just like a
  // matte sphere.
  TEST_F(PhongMaterialTest, ShouldBeVisible) {
    given("a phong sphere");
    when("i look at the origin");
    then("i should see something");
  }

  // A directional light aligned with the camera puts the specular lobe peak
  // at the sphere's center (R·V=1). With specularCoeff=1 and white specular
  // color, the center cluster gains non-red light on top of the red diffuse
  // texture. Black ambient + black background isolates the specular term.
  TEST_F(PhongMaterialTest, ShouldProduceSpecularHighlight) {
    given("a dark scene");
    given("a phong sphere with white specular");
    given("a directional light from (0, 0, -1)");
    when("i look at the origin");
    then("i should see a specular highlight");
  }

  // Contrast: a matte sphere (no specular term) under the same dark scene +
  // head-on light must NOT produce the non-red center highlight. Pins the
  // behavioral difference between PhongMaterial and MatteMaterial.
  TEST_F(PhongMaterialTest, MatteSphereDoesNotProduceSpecularHighlight) {
    given("a dark scene");
    given("a centered sphere");
    given("a directional light from (0, 0, -1)");
    when("i look at the origin");
    then("i should not see a specular highlight");
  }
}
