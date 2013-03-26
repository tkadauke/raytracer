#include "gtest.h"
#include "raytracer/materials/MatteMaterial.h"

namespace MatteMaterialTest {
  using namespace raytracer;

  TEST(MatteMaterial, ShouldInitialize) {
    MatteMaterial material;
    ASSERT_EQ(Colord::black(), material.diffuseColor());
  }

  TEST(MatteMaterial, ShouldInitializeWithDiffuseColor) {
    MatteMaterial material(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  }
  
  TEST(MatteMaterial, ShouldSetDiffuseColor) {
    MatteMaterial material;
    material.setDiffuseColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  }
}
