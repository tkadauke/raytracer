#include "gtest.h"
#include "materials/PhongMaterial.h"

namespace PhongMaterialTest {
  TEST(PhongMaterial, ShouldInitialize) {
    PhongMaterial material;
    ASSERT_EQ(Colord::white, material.highlightColor());
  }

  TEST(PhongMaterial, ShouldInitializeWithDiffuseColor) {
    PhongMaterial material(Colord(0, 1, 0));
    ASSERT_EQ(Colord::white, material.highlightColor());
    ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  }
  
  TEST(PhongMaterial, ShouldSetDiffuseColor) {
    PhongMaterial material;
    material.setDiffuseColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  }
  
  TEST(PhongMaterial, ShouldSetHighlightColor) {
    PhongMaterial material;
    material.setHighlightColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.highlightColor());
  }
}
