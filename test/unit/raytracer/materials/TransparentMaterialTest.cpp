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
    auto texture = std::make_shared<ConstantColorTexture>(Colord(0, 1, 0));
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

  TEST(TransparentMaterial, ShouldSetTransmissionCoefficient) {
    TransparentMaterial material;
    material.setTransmissionCoefficient(0.5);
    ASSERT_EQ(0.5, material.transmissionCoefficient());
  }

  TEST(TransparentMaterial, ShouldReflectionColor) {
    TransparentMaterial material;
    material.setReflectionColor(Colord(0, 1, 0));
    ASSERT_EQ(Colord(0, 1, 0), material.reflectionColor());
  }

  TEST(TransparentMaterial, ShouldSetReflectionCoefficient) {
    TransparentMaterial material;
    material.setReflectionCoefficient(0.5);
    ASSERT_EQ(0.5, material.reflectionCoefficient());
  }
}
