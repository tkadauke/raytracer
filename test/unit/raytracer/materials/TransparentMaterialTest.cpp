#include "gtest.h"
#include "raytracer/materials/TransparentMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"

namespace TransparentMaterialTest {
  using namespace raytracer;

  TEST(TransparentMaterial, ShouldInitialize) {
    TransparentMaterial material;
    ASSERT_EQ(Colord::white(), material.specularColor());
    ASSERT_EQ(1, material.refractionIndex());
  }

  TEST(TransparentMaterial, ShouldInitializeWithDiffuseTexture) {
    auto texture = new ConstantColorTexture(Colord(0, 1, 0));
    TransparentMaterial material(texture);
    ASSERT_EQ(texture, material.diffuseTexture());
    ASSERT_EQ(Colord::white(), material.specularColor());
    ASSERT_EQ(1, material.refractionIndex());
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
