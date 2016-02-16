#include "gtest.h"
#include "raytracer/materials/PhongMaterial.h"

namespace PhongMaterialTest {
  using namespace raytracer;

  TEST(PhongMaterial, ShouldInitialize) {
    PhongMaterial material;
    ASSERT_EQ(Colord::white(), material.specularColor());
  }

  TEST(PhongMaterial, ShouldInitializeWithDiffuseColor) {
    PhongMaterial material(Colord(0, 1, 0));
    ASSERT_EQ(Colord::white(), material.specularColor());
    ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  }
  
  TEST(PhongMaterial, ShouldSetDiffuseColor) {
    PhongMaterial material;
    material.setDiffuseColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  }
  
  TEST(PhongMaterial, ShouldSetHighlightColor) {
    PhongMaterial material;
    material.setSpecularColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.specularColor());
  }
}
