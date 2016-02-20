#include "gtest.h"
#include "raytracer/materials/TransparentMaterial.h"

namespace TransparentMaterialTest {
  using namespace raytracer;

  TEST(TransparentMaterial, ShouldInitialize) {
    TransparentMaterial material;
    ASSERT_EQ(Colord::white(), material.specularColor());
    ASSERT_EQ(1, material.refractionIndex());
  }

  TEST(TransparentMaterial, ShouldInitializeWithDiffuseColor) {
    TransparentMaterial material(Colord(0, 1, 0));
    ASSERT_EQ(Colord::white(), material.specularColor());
    ASSERT_EQ(1, material.refractionIndex());
    ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  }
  
  TEST(TransparentMaterial, ShouldSetDiffuseColor) {
    TransparentMaterial material;
    material.setDiffuseColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  }
  
  TEST(TransparentMaterial, ShouldSetHighlightColor) {
    TransparentMaterial material;
    material.setSpecularColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.specularColor());
  }
  
  TEST(TransparentMaterial, ShouldSetRefractionIndex) {
    TransparentMaterial material;
    material.setRefractionIndex(1.4);
    ASSERT_EQ(1.4, material.refractionIndex());
  }
}
