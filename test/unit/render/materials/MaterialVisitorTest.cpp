#include <gtest/gtest.h>

#include "core/math/HitPoint.h"
#include "core/math/Matrix.h"
#include "render/State.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/PortalMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Scene.h"

namespace MaterialVisitorTest {
  using namespace render;

  namespace {
    class CustomMaterial final : public Material {
    public:
      Colord shade(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                   State&) const override {
        return Colord::black();
      }
    };

    class MaterialFamilyCounter final : public MaterialVisitor {
    public:
      void visit(const Material&) override {
        ++baseMaterials;
      }

      void visit(const MatteMaterial&) override {
        ++matteMaterials;
      }

      void visit(const EmissiveMaterial&) override {
        ++emissiveMaterials;
      }

      int baseMaterials{0};
      int matteMaterials{0};
      int emissiveMaterials{0};
    };
  }

  TEST(MaterialVisitor, TypeNamesAreStableDiagnosticLabels) {
    const CustomMaterial custom;
    const MatteMaterial matte;
    const PhongMaterial phong;
    const ReflectiveMaterial reflective;
    const TransparentMaterial transparent;
    const EmissiveMaterial emissive(Colord::white());
    const PortalMaterial portal(Matrix4d(), Colord::white());

    EXPECT_STREQ("Material", custom.typeName());
    EXPECT_STREQ("MatteMaterial", matte.typeName());
    EXPECT_STREQ("PhongMaterial", phong.typeName());
    EXPECT_STREQ("ReflectiveMaterial", reflective.typeName());
    EXPECT_STREQ("TransparentMaterial", transparent.typeName());
    EXPECT_STREQ("EmissiveMaterial", emissive.typeName());
    EXPECT_STREQ("PortalMaterial", portal.typeName());
  }

  TEST(MaterialVisitor, DefaultOverloadsFollowMaterialInheritanceTree) {
    const CustomMaterial custom;
    const MatteMaterial matte;
    const PhongMaterial phong;
    const ReflectiveMaterial reflective;
    const TransparentMaterial transparent;
    const EmissiveMaterial emissive(Colord::white());
    const PortalMaterial portal(Matrix4d(), Colord::white());

    MaterialFamilyCounter counter;
    custom.accept(counter);
    matte.accept(counter);
    phong.accept(counter);
    reflective.accept(counter);
    transparent.accept(counter);
    emissive.accept(counter);
    portal.accept(counter);

    EXPECT_EQ(2, counter.baseMaterials);
    EXPECT_EQ(4, counter.matteMaterials);
    EXPECT_EQ(1, counter.emissiveMaterials);
  }
}
