#include "gtest.h"
#include "materials/MatteMaterial.h"

namespace MatteMaterialTest {
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
