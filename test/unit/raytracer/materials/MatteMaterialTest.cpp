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
  
  TEST(MatteMaterial, ShouldSetAmbientCoefficient) {
    MatteMaterial material;
    material.setAmbientCoefficient(0.6);
    ASSERT_EQ(0.6, material.ambientCoefficient());
  }
  
  TEST(MatteMaterial, ShouldSetDiffuseCoefficient) {
    MatteMaterial material;
    material.setDiffuseCoefficient(0.6);
    ASSERT_EQ(0.6, material.diffuseCoefficient());
  }
}
