#include "gtest.h"
#include "raytracer/materials/ReflectiveMaterial.h"

namespace ReflectiveMaterialTest {
  using namespace raytracer;

  TEST(ReflectiveMaterial, ShouldInitialize) {
    ReflectiveMaterial material;
  }

  TEST(ReflectiveMaterial, ShouldInitializeWithDiffuseColor) {
    ReflectiveMaterial material(Colord(0, 1, 0));
    ASSERT_EQ(Colord::white(), material.specularColor());
    ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  }
  
  TEST(ReflectiveMaterial, ShouldSetDiffuseColor) {
    ReflectiveMaterial material;
    material.setDiffuseColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
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
