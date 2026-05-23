#include <gtest/gtest.h>

#include "world/objects/Material.h"
#include "world/objects/MatteMaterial.h"
#include "world/objects/PhongMaterial.h"
#include "world/objects/ReflectiveMaterial.h"
#include "world/objects/TransparentMaterial.h"
#include "world/objects/ConstantColorTexture.h"

#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"

#include <QString>

namespace MaterialTest {
  // ---------- Material (abstract base) --------------------------------------

  TEST(Material, ShouldReturnSameDefaultMaterialAcrossCalls) {
    EXPECT_EQ(Material::defaultMaterial(), Material::defaultMaterial());
  }

  TEST(Material, ShouldReturnMatteMaterialAsDefault) {
    EXPECT_NE(nullptr, dynamic_cast<MatteMaterial*>(Material::defaultMaterial()));
  }

  // ---------- MatteMaterial -------------------------------------------------

  TEST(MatteMaterial, ShouldDefaultToNoTexture) {
    MatteMaterial m;
    EXPECT_EQ(nullptr, m.diffuseTexture());
    EXPECT_EQ(nullptr, m.normalTexture());
  }

  TEST(MatteMaterial, ShouldDefaultToUnitCoefficients) {
    MatteMaterial m;
    EXPECT_DOUBLE_EQ(1.0, m.ambientCoefficient());
    EXPECT_DOUBLE_EQ(1.0, m.diffuseCoefficient());
  }

  TEST(MatteMaterial, ShouldDefaultToTwoSided) {
    MatteMaterial m;
    EXPECT_EQ(Material::Sidedness::TwoSided, m.sidedness());
    EXPECT_EQ(QString("TwoSided"), m.sidednessName());
  }

  TEST(MatteMaterial, ShouldConvertSidednessToRaytracerMaterial) {
    MatteMaterial m;
    m.setSidedness(Material::Sidedness::Front);
    Material* base = &m;

    auto rt = base->toRaytracerMaterial();

    EXPECT_EQ(render::Material::Sidedness::Front, rt->sidedness());
  }

  TEST(MatteMaterial, ShouldRoundTripSidednessName) {
    MatteMaterial m;
    m.setSidednessName("Back");
    EXPECT_EQ(Material::Sidedness::Back, m.sidedness());
    EXPECT_EQ(QString("Back"), m.sidednessName());

    m.setSidednessName("unknown");
    EXPECT_EQ(Material::Sidedness::TwoSided, m.sidedness());
  }

  TEST(MatteMaterial, ShouldSetAndGetCoefficients) {
    MatteMaterial m;
    m.setAmbientCoefficient(0.3);
    m.setDiffuseCoefficient(0.7);
    EXPECT_DOUBLE_EQ(0.3, m.ambientCoefficient());
    EXPECT_DOUBLE_EQ(0.7, m.diffuseCoefficient());
  }

  TEST(MatteMaterial, ShouldSetAndGetDiffuseTexture) {
    MatteMaterial m;
    ConstantColorTexture tex;
    m.setDiffuseTexture(&tex);
    EXPECT_EQ(&tex, m.diffuseTexture());
  }

  TEST(MatteMaterial, ShouldSetAndGetNormalTexture) {
    MatteMaterial m;
    ConstantColorTexture tex;
    m.setNormalTexture(&tex);
    EXPECT_EQ(&tex, m.normalTexture());
  }

  TEST(MatteMaterial, ShouldConvertNormalTextureToRaytracerMaterial) {
    MatteMaterial m;
    ConstantColorTexture tex;
    m.setNormalTexture(&tex);
    Material* base = &m;

    auto rt = std::dynamic_pointer_cast<render::MatteMaterial>(
      base->toRaytracerMaterial());

    ASSERT_NE(nullptr, rt);
    EXPECT_NE(nullptr, rt->normalTexture());
  }

  TEST(MatteMaterial, ShouldProduceRaytracerMatteMaterial) {
    MatteMaterial m;
    Material* base = &m;
    auto rt = std::dynamic_pointer_cast<render::MatteMaterial>(
      base->toRaytracerMaterial());
    EXPECT_NE(nullptr, rt);
  }

  // ---------- PhongMaterial -------------------------------------------------

  TEST(PhongMaterial, ShouldDefaultToWhiteSpecular) {
    PhongMaterial m;
    EXPECT_EQ(Colord::white(), m.specularColor());
  }

  TEST(PhongMaterial, ShouldDefaultToExponent16) {
    // Default exponent of 16 picks a moderately tight specular lobe; it's
    // the value the existing example scenes assume, so a change here will
    // shift the look of every Phong-shaded scene.
    PhongMaterial m;
    EXPECT_DOUBLE_EQ(16.0, m.exponent());
  }

  TEST(PhongMaterial, ShouldDefaultToFullSpecularCoefficient) {
    PhongMaterial m;
    EXPECT_DOUBLE_EQ(1.0, m.specularCoefficient());
  }

  TEST(PhongMaterial, ShouldClampSpecularCoefficientToZeroOne) {
    // setSpecularCoefficient passes through Ranged(0,1)::clamp — over-
    // and under-range values clamp to the endpoints rather than rejecting
    // the call. Pin both ends because there's no other indication in the
    // header that the setter is lossy.
    PhongMaterial m;
    m.setSpecularCoefficient(2.0);
    EXPECT_DOUBLE_EQ(1.0, m.specularCoefficient());
    m.setSpecularCoefficient(-0.5);
    EXPECT_DOUBLE_EQ(0.0, m.specularCoefficient());
  }

  TEST(PhongMaterial, ShouldSetAndGetSpecularColorAndExponent) {
    PhongMaterial m;
    m.setSpecularColor(Colord(1, 0, 0));
    m.setExponent(64);
    EXPECT_EQ(Colord(1, 0, 0), m.specularColor());
    EXPECT_DOUBLE_EQ(64.0, m.exponent());
  }

  TEST(PhongMaterial, ShouldProduceRaytracerPhongMaterial) {
    PhongMaterial m;
    Material* base = &m;
    auto rt = std::dynamic_pointer_cast<render::PhongMaterial>(
      base->toRaytracerMaterial());
    EXPECT_NE(nullptr, rt);
  }

  // ---------- ReflectiveMaterial --------------------------------------------

  TEST(ReflectiveMaterial, ShouldDefaultToWhiteReflectionColor) {
    ReflectiveMaterial m;
    EXPECT_EQ(Colord::white(), m.reflectionColor());
  }

  TEST(ReflectiveMaterial, ShouldDefaultToHalfReflectionCoefficient) {
    ReflectiveMaterial m;
    EXPECT_DOUBLE_EQ(0.5, m.reflectionCoefficient());
  }

  TEST(ReflectiveMaterial, ShouldSetAndGetReflectionPropertiesUnclamped) {
    // ReflectiveMaterial's reflectionCoefficient setter does NOT clamp
    // (unlike PhongMaterial::setSpecularCoefficient) — the value is stored
    // as-is. Pin that asymmetry so a future "make all coefficients
    // clamp" sweep doesn't silently change the contract.
    ReflectiveMaterial m;
    m.setReflectionColor(Colord(0.1, 0.2, 0.3));
    m.setReflectionCoefficient(2.5);
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), m.reflectionColor());
    EXPECT_DOUBLE_EQ(2.5, m.reflectionCoefficient());
  }

  TEST(ReflectiveMaterial, ShouldProduceRaytracerReflectiveMaterial) {
    ReflectiveMaterial m;
    Material* base = &m;
    auto rt = std::dynamic_pointer_cast<render::ReflectiveMaterial>(
      base->toRaytracerMaterial());
    EXPECT_NE(nullptr, rt);
  }

  // ---------- TransparentMaterial -------------------------------------------

  TEST(TransparentMaterial, ShouldDefaultToFullTransmission) {
    TransparentMaterial m;
    EXPECT_DOUBLE_EQ(1.0, m.transmissionCoefficient());
  }

  TEST(TransparentMaterial, ShouldDefaultToVacuumRefractionIndex) {
    // 1.0 = vacuum / air. Real materials override (water 1.33, glass 1.5,
    // diamond 2.4). Doc comment in the header tags 1.0 as the canned
    // default; pinning so a future change to "glass-like" defaults trips
    // an explicit failure.
    TransparentMaterial m;
    EXPECT_DOUBLE_EQ(1.0, m.refractionIndex());
  }

  TEST(TransparentMaterial, ShouldClampTransmissionCoefficientToZeroOne) {
    TransparentMaterial m;
    m.setTransmissionCoefficient(5.0);
    EXPECT_DOUBLE_EQ(1.0, m.transmissionCoefficient());
    m.setTransmissionCoefficient(-1.0);
    EXPECT_DOUBLE_EQ(0.0, m.transmissionCoefficient());
  }

  TEST(TransparentMaterial, ShouldClampReflectionCoefficientToZeroOne) {
    TransparentMaterial m;
    m.setReflectionCoefficient(2.0);
    EXPECT_DOUBLE_EQ(1.0, m.reflectionCoefficient());
    m.setReflectionCoefficient(-0.1);
    EXPECT_DOUBLE_EQ(0.0, m.reflectionCoefficient());
  }

  TEST(TransparentMaterial, ShouldStoreRefractionIndexUnclamped) {
    TransparentMaterial m;
    m.setRefractionIndex(2.4);
    EXPECT_DOUBLE_EQ(2.4, m.refractionIndex());
  }

  TEST(TransparentMaterial, ShouldProduceRaytracerTransparentMaterial) {
    TransparentMaterial m;
    Material* base = &m;
    auto rt = std::dynamic_pointer_cast<render::TransparentMaterial>(
      base->toRaytracerMaterial());
    EXPECT_NE(nullptr, rt);
  }
}
