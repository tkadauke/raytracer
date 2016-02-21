#include "gtest.h"
#include "raytracer/materials/ReflectiveMaterial.h"
#include "raytracer/textures/ConstantColorTexture.h"

namespace ReflectiveMaterialTest {
  using namespace raytracer;

  TEST(ReflectiveMaterial, ShouldInitialize) {
    ReflectiveMaterial material;
  }

  TEST(ReflectiveMaterial, ShouldInitializeWithDiffuseTexture) {
    auto texture = new ConstantColorTexture(Colord(0, 1, 0));
    ReflectiveMaterial material(texture);
    ASSERT_EQ(texture, material.diffuseTexture());
    ASSERT_EQ(Colord::white(), material.specularColor());
  }
  
  TEST(ReflectiveMaterial, ShouldSetHighlightColor) {
    ReflectiveMaterial material;
    material.setSpecularColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.specularColor());
  }
  
  TEST(ReflectiveMaterial, ShouldSetSpecularColor) {
    ReflectiveMaterial material;
    material.setSpecularColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.specularColor());
  }
}
