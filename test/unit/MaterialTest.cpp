#include "gtest.h"
#include "Material.h"

namespace MaterialTest {
  // TEST(Material, ShouldInitialize) {
  //   Material material;
  //   ASSERT_EQ(Colord::white, material.highlightColor());
  //   ASSERT_EQ(1, material.refractionIndex());
  // }
  // 
  // TEST(Material, ShouldInitializeWithDiffuseColor) {
  //   Material material(Colord(0, 1, 0));
  //   ASSERT_EQ(Colord::white, material.highlightColor());
  //   ASSERT_EQ(1, material.refractionIndex());
  //   ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  // }
  // 
  // TEST(Material, ShouldSetDiffuseColor) {
  //   Material material;
  //   material.setDiffuseColor(Colord(0, 1, 0));
  //   ASSERT_EQ(Colord(0, 1, 0), material.diffuseColor());
  // }
  // 
  // TEST(Material, ShouldSetHighlightColor) {
  //   Material material;
  //   material.setHighlightColor(Colord(0, 1, 0));
  //   ASSERT_EQ(Colord(0, 1, 0), material.highlightColor());
  // }
  // 
  // TEST(Material, ShouldSetSpecularColor) {
  //   Material material;
  //   material.setSpecularColor(Colord(0, 1, 0));
  //   ASSERT_EQ(Colord(0, 1, 0), material.specularColor());
  // }
  // 
  // TEST(Material, ShouldReturnTrueIfMaterialIsReflective) {
  //   Material material;
  //   material.setSpecularColor(Colord::white);
  //   ASSERT_TRUE(material.isReflective());
  // }
  // 
  // TEST(Material, ShouldReturnFalseIfMaterialIsNotReflective) {
  //   Material material;
  //   material.setSpecularColor(Colord::black);
  //   ASSERT_FALSE(material.isReflective());
  // }
  // 
  // TEST(Material, ShouldNotBeReflectiveByDefault) {
  //   Material material;
  //   ASSERT_FALSE(material.isReflective());
  // }
  // 
  // TEST(Material, ShouldSetAbsorbanceColor) {
  //   Material material;
  //   material.setAbsorbanceColor(Colord(0, 1, 0));
  //   ASSERT_EQ(Colord(0, 1, 0), material.absorbanceColor());
  // }
  // 
  // TEST(Material, ShouldSetRefractionIndex) {
  //   Material material;
  //   material.setRefractionIndex(1.4);
  //   ASSERT_EQ(1.4, material.refractionIndex());
  // }
  // 
  // TEST(Material, ShouldReturnTrueIfMaterialIsRefractive) {
  //   Material material;
  //   material.setAbsorbanceColor(Colord(0, 1, 0));
  //   ASSERT_TRUE(material.isRefractive());
  // }
  // 
  // TEST(Material, ShouldReturnFalseIfMaterialIsNotRefractive) {
  //   Material material;
  //   material.setAbsorbanceColor(Colord::black);
  //   ASSERT_FALSE(material.isRefractive());
  // }
  // 
  // TEST(Material, ShouldNotBeRefractiveByDefault) {
  //   Material material;
  //   ASSERT_FALSE(material.isRefractive());
  // }
  // 
  // TEST(Material, ShouldReturnTrueIfMaterialIsSpecular) {
  //   Material material;
  //   material.setAbsorbanceColor(Colord(0, 1, 0));
  //   ASSERT_TRUE(material.isSpecular());
  //   
  //   material = Material();
  //   material.setSpecularColor(Colord(0, 1, 0));
  //   ASSERT_TRUE(material.isSpecular());
  // }
  // 
  // TEST(Material, ShouldReturnFalseIfMaterialIsNotSpecular) {
  //   Material material;
  //   material.setAbsorbanceColor(Colord::black);
  //   material.setSpecularColor(Colord::black);
  //   ASSERT_FALSE(material.isSpecular());
  // }
  // 
  // TEST(Material, ShouldNotBeSpecularByDefault) {
  //   Material material;
  //   ASSERT_FALSE(material.isSpecular());
  // }
}
