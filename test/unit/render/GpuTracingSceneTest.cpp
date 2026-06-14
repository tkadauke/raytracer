#include <gtest/gtest.h>

#include "render/IntersectionSceneCompiler.h"
#include "render/GpuTracingScene.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"

#include <type_traits>

namespace GpuTracingSceneTest {
  using namespace render;

  namespace {
    template<typename Record>
    void expectKernelRecordLayout() {
      EXPECT_TRUE(std::is_standard_layout_v<Record>);
      EXPECT_EQ(16u, alignof(Record));
      EXPECT_EQ(0u, sizeof(Record) % 16u);
    }
  }

  TEST(GpuTracingScene, PackedRecordsHaveStableKernelFriendlyLayout) {
    expectKernelRecordLayout<GpuTracingMaterialRecord>();
    expectKernelRecordLayout<GpuTracingTextureRecord>();
    expectKernelRecordLayout<GpuTracingLightRecord>();
    expectKernelRecordLayout<GpuTracingEnvironmentRecord>();
    expectKernelRecordLayout<GpuTracingDebugIdRecord>();
    expectKernelRecordLayout<GpuTracingShadingRecord>();
  }

  TEST(GpuTracingScene, LayoutVersionScopesAllCompiledSceneSections) {
    const GpuTracingSceneSections sections;

    const auto layouts = sections.sectionLayouts();

    ASSERT_EQ(6u, layouts.size());
    EXPECT_EQ(GpuTracingSceneSectionKind::Geometry, layouts[0].kind);
    EXPECT_EQ(GpuTracingSceneSectionKind::Materials, layouts[1].kind);
    EXPECT_EQ(GpuTracingSceneSectionKind::Textures, layouts[2].kind);
    EXPECT_EQ(GpuTracingSceneSectionKind::Lights, layouts[3].kind);
    EXPECT_EQ(GpuTracingSceneSectionKind::Environment, layouts[4].kind);
    EXPECT_EQ(GpuTracingSceneSectionKind::DebugIds, layouts[5].kind);

    for (const GpuTracingSceneSectionLayout& layout : layouts) {
      EXPECT_EQ(gpuTracingSceneLayoutVersion, layout.layoutVersion);
    }
  }

  TEST(GpuTracingScene, SectionLayoutsReportCountsOffsetsAndUploadBytes) {
    GpuTracingSceneSections sections;
    sections.geometry.primitives.push_back(GpuIntersectionPrimitiveRecord{});
    sections.geometry.triangles.push_back(GpuIntersectionTrianglePayload{});
    sections.materials.push_back(GpuTracingMaterialRecord{});
    sections.materials.push_back(GpuTracingMaterialRecord{});
    sections.textures.push_back(GpuTracingTextureRecord{});
    sections.lights.push_back(GpuTracingLightRecord{});
    sections.environment.push_back(makeGpuTracingConstantEnvironment(Colord(0.25, 0.5, 0.75)));
    sections.debugIds.push_back(GpuTracingDebugIdRecord{});

    const auto layouts = sections.sectionLayouts();

    const std::uint32_t geometryBytes =
      static_cast<std::uint32_t>(sections.geometry.uploadByteCount());
    EXPECT_EQ(1u, layouts[0].recordCount);
    EXPECT_EQ(0u, layouts[0].recordSize);
    EXPECT_EQ(16u, layouts[0].recordAlignment);
    EXPECT_EQ(0u, layouts[0].byteOffset);
    EXPECT_EQ(geometryBytes, layouts[0].byteCount);

    EXPECT_EQ(2u, layouts[1].recordCount);
    EXPECT_EQ(sizeof(GpuTracingMaterialRecord), layouts[1].recordSize);
    EXPECT_EQ(alignof(GpuTracingMaterialRecord), layouts[1].recordAlignment);
    EXPECT_EQ(geometryBytes, layouts[1].byteOffset);

    EXPECT_EQ(1u, layouts[2].recordCount);
    EXPECT_EQ(layouts[1].byteOffset + layouts[1].byteCount, layouts[2].byteOffset);
    EXPECT_EQ(1u, layouts[3].recordCount);
    EXPECT_EQ(layouts[2].byteOffset + layouts[2].byteCount, layouts[3].byteOffset);
    EXPECT_EQ(1u, layouts[4].recordCount);
    EXPECT_EQ(layouts[3].byteOffset + layouts[3].byteCount, layouts[4].byteOffset);
    EXPECT_EQ(1u, layouts[5].recordCount);
    EXPECT_EQ(layouts[4].byteOffset + layouts[4].byteCount, layouts[5].byteOffset);

    EXPECT_EQ(layouts[5].byteOffset + layouts[5].byteCount, sections.uploadByteCount());
  }

  TEST(GpuTracingScene, ShadingRecordsAreSeparateFromIntersectionHitRecords) {
    constexpr bool sameRecordType =
      std::is_same_v<GpuTracingShadingRecord, GpuIntersectionHitRecord>;
    EXPECT_FALSE(sameRecordType);
    EXPECT_NE(sizeof(GpuIntersectionHitRecord), sizeof(GpuTracingShadingRecord));

    GpuIntersectionHitRecord hit;
    hit.hit = 1;
    hit.rayIndex = 42;

    GpuTracingShadingRecord shading;
    shading.pathIndex = 42;
    shading.material = 7;

    EXPECT_EQ(1u, hit.hit);
    EXPECT_EQ(42u, hit.rayIndex);
    EXPECT_EQ(42u, shading.pathIndex);
    EXPECT_EQ(7u, shading.material);
  }

  TEST(GpuTracingScene, ConstantEnvironmentPacksColorWithOpaqueAlpha) {
    const GpuTracingEnvironmentRecord environment =
      makeGpuTracingConstantEnvironment(Colord(0.25, 0.5, 0.75));

    EXPECT_FLOAT_EQ(0.25f, environment.color[0]);
    EXPECT_FLOAT_EQ(0.5f, environment.color[1]);
    EXPECT_FLOAT_EQ(0.75f, environment.color[2]);
    EXPECT_FLOAT_EQ(1.0f, environment.color[3]);
  }

  TEST(GpuTracingScene, MaterialCompilerPacksMatteAndEmissiveRecords) {
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::red()));
    matte->setAmbientCoefficient(0.25);
    matte->setDiffuseCoefficient(0.75);
    auto emissive = std::make_shared<EmissiveMaterial>(Colord(2.0, 3.0, 4.0));
    const std::vector<std::shared_ptr<Material>> materials{nullptr, matte, emissive};

    const GpuTracingMaterialCompilation compiled = GpuTracingMaterialCompiler().compile(materials);

    ASSERT_EQ(3u, compiled.records.size());
    EXPECT_TRUE(compiled.fullySupported());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Unsupported),
              compiled.records[0].kind);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Matte), compiled.records[1].kind);
    EXPECT_FLOAT_EQ(0.25f, compiled.records[1].parameters[0]);
    EXPECT_FLOAT_EQ(0.75f, compiled.records[1].parameters[1]);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Emissive),
              compiled.records[2].kind);
    EXPECT_FLOAT_EQ(2.0f, compiled.records[2].parameters[0]);
    EXPECT_FLOAT_EQ(3.0f, compiled.records[2].parameters[1]);
    EXPECT_FLOAT_EQ(4.0f, compiled.records[2].parameters[2]);
    EXPECT_FLOAT_EQ(1.0f, compiled.records[2].parameters[3]);
  }

  TEST(GpuTracingScene, MaterialIdsRoundTripFromRuntimeSceneToCompiledRecords) {
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto emissive = std::make_shared<EmissiveMaterial>(Colord(1.0, 2.0, 3.0));
    auto matteSphere = std::make_shared<Sphere>(Vector3d(-2, 0, 0), 1.0);
    auto emissiveSphere = std::make_shared<Sphere>(Vector3d(2, 0, 0), 1.0);
    matteSphere->setMaterial(matte);
    emissiveSphere->setMaterial(emissive);
    Scene scene;
    scene.add(matteSphere);
    scene.add(emissiveSphere);

    const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
    const GpuTracingMaterialCompilation materials =
      GpuTracingMaterialCompiler().compile(intersection);

    ASSERT_EQ(intersection.materials().size(), materials.records.size());
    ASSERT_EQ(2u, intersection.primitives().size());
    for (const IntersectionPrimitiveRecord& primitive : intersection.primitives()) {
      ASSERT_LT(primitive.material, materials.records.size());
      ASSERT_EQ(intersection.materials()[primitive.material].get() == matte.get()
                  ? static_cast<std::uint32_t>(GpuTracingMaterialKind::Matte)
                  : static_cast<std::uint32_t>(GpuTracingMaterialKind::Emissive),
                materials.records[primitive.material].kind);
    }
  }

  TEST(GpuTracingScene, MaterialCompilerCountsUnsupportedMaterialsByReason) {
    auto matte = std::make_shared<MatteMaterial>();
    auto phong = std::make_shared<PhongMaterial>();
    auto reflective = std::make_shared<ReflectiveMaterial>();
    const std::vector<std::shared_ptr<Material>> materials{nullptr, matte, phong, reflective};

    const GpuTracingMaterialCompilation compiled = GpuTracingMaterialCompiler().compile(materials);

    ASSERT_EQ(4u, compiled.records.size());
    EXPECT_FALSE(compiled.fullySupported());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Matte), compiled.records[1].kind);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Unsupported),
              compiled.records[2].kind);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Unsupported),
              compiled.records[3].kind);

    ASSERT_EQ(2u, compiled.unsupportedMaterials.size());
    EXPECT_EQ(2u, compiled.unsupportedMaterials[0].material);
    EXPECT_EQ(3u, compiled.unsupportedMaterials[1].material);

    const std::vector<GpuTracingUnsupportedReasonCount> counts = compiled.unsupportedReasonCounts();
    ASSERT_EQ(1u, counts.size());
    EXPECT_EQ("material is outside the GPU tracing Matte/Emissive subset", counts[0].reason);
    EXPECT_EQ(2u, counts[0].count);
  }
}
