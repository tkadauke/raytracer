#include <gtest/gtest.h>

#include "render/GpuTracingScene.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/Light.h"
#include "render/lights/PointLight.h"
#include "render/lights/RectangularAreaLight.h"
#include "render/primitives/Scene.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/UVColorTexture.h"
#include "render/textures/mappings/PlanarMapping2D.h"

#include <memory>
#include <type_traits>
#include <vector>

namespace GpuTracingSceneTest {
  using namespace render;

  namespace {
    template<typename Record>
    void expectKernelRecordLayout() {
      EXPECT_TRUE(std::is_standard_layout_v<Record>);
      EXPECT_EQ(16u, alignof(Record));
      EXPECT_EQ(0u, sizeof(Record) % 16u);
    }

    class UnsupportedLight final : public Light {
    public:
      explicit UnsupportedLight(const char* type)
          : m_type(type) {
      }

      Vector3d direction(const Vector3d&) const override {
        return Vector3d(0.0, 1.0, 0.0);
      }

      Colord radiance() const override {
        return Colord::white();
      }

      const char* fingerprintType() const override {
        return m_type;
      }

    private:
      const char* m_type;
    };

    void expectFloat4(const std::array<float, 4>& actual, float x, float y, float z, float w) {
      EXPECT_FLOAT_EQ(x, actual[0]);
      EXPECT_FLOAT_EQ(y, actual[1]);
      EXPECT_FLOAT_EQ(z, actual[2]);
      EXPECT_FLOAT_EQ(w, actual[3]);
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

  TEST(GpuTracingScene, PointLightPacksPositionAndColor) {
    const PointLight light(Vector3d(1.0, 2.0, 3.0), Colord(0.25, 0.5, 0.75));

    const std::optional<GpuTracingLightRecord> record = makeGpuTracingLightRecord(light);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingLightKind::Point), record->kind);
    expectFloat4(record->positionOrDirection, 1.0f, 2.0f, 3.0f, 1.0f);
    expectFloat4(record->u, 0.0f, 0.0f, 0.0f, 0.0f);
    expectFloat4(record->v, 0.0f, 0.0f, 0.0f, 0.0f);
    expectFloat4(record->parameters, 0.25f, 0.5f, 0.75f, 1.0f);
  }

  TEST(GpuTracingScene, DirectionalLightPacksDirectionAndColor) {
    const DirectionalLight light(Vector3d(0.0, -2.0, 0.0), Colord(0.125, 0.25, 0.5));

    const std::optional<GpuTracingLightRecord> record = makeGpuTracingLightRecord(light);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingLightKind::Directional), record->kind);
    expectFloat4(record->positionOrDirection, 0.0f, -1.0f, 0.0f, 0.0f);
    expectFloat4(record->parameters, 0.125f, 0.25f, 0.5f, 1.0f);
  }

  TEST(GpuTracingScene, RectangularAreaLightPacksCenterEdgesAndRadiance) {
    const RectangularAreaLight light(Vector3d(1.0, 2.0, 3.0), Vector3d(4.0, 0.0, 0.0),
                                     Vector3d(0.0, 0.0, 5.0), Colord(0.2, 0.4, 0.8));

    const std::optional<GpuTracingLightRecord> record = makeGpuTracingLightRecord(light);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingLightKind::RectangularArea), record->kind);
    expectFloat4(record->positionOrDirection, 1.0f, 2.0f, 3.0f, 1.0f);
    expectFloat4(record->u, 4.0f, 0.0f, 0.0f, 0.0f);
    expectFloat4(record->v, 0.0f, 0.0f, 5.0f, 0.0f);
    expectFloat4(record->parameters, 0.2f, 0.4f, 0.8f, 1.0f);
  }

  TEST(GpuTracingScene, UnsupportedLightReportsStableReason) {
    const UnsupportedLight light("UnsupportedLight");
    std::string reason;

    const std::optional<GpuTracingLightRecord> record = makeGpuTracingLightRecord(light, &reason);

    EXPECT_FALSE(record.has_value());
    EXPECT_EQ("light type is not supported by GPU tracing scene compiler", reason);
  }

  TEST(GpuTracingScene, CompilesSceneLightsAndCountsUnsupportedReasons) {
    Scene scene;
    scene.addLight(std::make_shared<PointLight>(Vector3d(1.0, 2.0, 3.0), Colord::white()));
    scene.addLight(std::make_shared<UnsupportedLight>("UnsupportedA"));
    scene.addLight(
      std::make_shared<DirectionalLight>(Vector3d(0.0, -1.0, 0.0), Colord(0.25, 0.5, 0.75)));
    scene.addLight(std::make_shared<UnsupportedLight>("UnsupportedB"));

    const GpuTracingLightCompilation compilation = compileGpuTracingLights(scene);

    EXPECT_FALSE(compilation.supported());
    ASSERT_EQ(2u, compilation.records.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingLightKind::Point), compilation.records[0].kind);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingLightKind::Directional),
              compilation.records[1].kind);

    ASSERT_EQ(2u, compilation.unsupportedLights.size());
    EXPECT_EQ(1u, compilation.unsupportedLights[0].lightIndex);
    EXPECT_EQ("UnsupportedA", compilation.unsupportedLights[0].type);
    EXPECT_EQ(3u, compilation.unsupportedLights[1].lightIndex);
    EXPECT_EQ("UnsupportedB", compilation.unsupportedLights[1].type);

    const std::vector<GpuTracingUnsupportedReasonCount> reasonCounts =
      compilation.unsupportedReasonCounts();
    ASSERT_EQ(1u, reasonCounts.size());
    EXPECT_EQ("light type is not supported by GPU tracing scene compiler", reasonCounts[0].reason);
    EXPECT_EQ(2u, reasonCounts[0].count);
  }

  TEST(GpuTracingScene, ConstantColorTexturePacksColorWithOpaqueAlpha) {
    const GpuTracingTextureRecord texture =
      makeGpuTracingConstantColorTexture(Colord(0.125, 0.5, 0.875));

    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor), texture.kind);
    EXPECT_EQ(0u, texture.payloadOffset);
    EXPECT_EQ(0u, texture.payloadCount);
    EXPECT_EQ(0u, texture.flags);
    EXPECT_FLOAT_EQ(0.125f, texture.parameters[0]);
    EXPECT_FLOAT_EQ(0.5f, texture.parameters[1]);
    EXPECT_FLOAT_EQ(0.875f, texture.parameters[2]);
    EXPECT_FLOAT_EQ(1.0f, texture.parameters[3]);
  }

  TEST(GpuTracingTextureCompiler, CompilesConstantColorTextureRecords) {
    const std::vector<std::shared_ptr<Texturec>> textures{
      std::make_shared<ConstantColorTexture>(Colord(0.1, 0.2, 0.3)),
      std::make_shared<ConstantColorTexture>(Colord(0.7, 0.8, 0.9)),
    };

    const GpuTracingTextureCompilation compilation = GpuTracingTextureCompiler().compile(textures);

    ASSERT_TRUE(compilation.fullySupported());
    ASSERT_EQ(2u, compilation.records().size());
    EXPECT_TRUE(compilation.unsupportedTextures().empty());
    EXPECT_TRUE(compilation.unsupportedReasonCounts().empty());

    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor),
              compilation.records()[0].kind);
    EXPECT_FLOAT_EQ(0.1f, compilation.records()[0].parameters[0]);
    EXPECT_FLOAT_EQ(0.2f, compilation.records()[0].parameters[1]);
    EXPECT_FLOAT_EQ(0.3f, compilation.records()[0].parameters[2]);

    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor),
              compilation.records()[1].kind);
    EXPECT_FLOAT_EQ(0.7f, compilation.records()[1].parameters[0]);
    EXPECT_FLOAT_EQ(0.8f, compilation.records()[1].parameters[1]);
    EXPECT_FLOAT_EQ(0.9f, compilation.records()[1].parameters[2]);
  }

  TEST(GpuTracingTextureCompiler, SurfacesUnsupportedTextureReasons) {
    auto mapping = std::make_unique<PlanarMapping2D>();
    auto checker = std::make_shared<CheckerBoardTexture>(mapping.get());
    checker->setName("checker albedo");

    auto uvColor = std::make_shared<UVColorTexture>();
    uvColor->setName("uv debug");

    const std::vector<std::shared_ptr<Texturec>> textures{
      std::make_shared<ConstantColorTexture>(Colord(0.1, 0.2, 0.3)),
      checker,
      std::shared_ptr<Texturec>(),
      uvColor,
    };

    const GpuTracingTextureCompilation compilation = GpuTracingTextureCompiler().compile(textures);

    EXPECT_FALSE(compilation.fullySupported());
    ASSERT_EQ(4u, compilation.records().size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor),
              compilation.records()[0].kind);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::Unsupported),
              compilation.records()[1].kind);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::Unsupported),
              compilation.records()[2].kind);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::Unsupported),
              compilation.records()[3].kind);

    ASSERT_EQ(3u, compilation.unsupportedTextures().size());
    EXPECT_EQ(1u, compilation.unsupportedTextures()[0].texture);
    EXPECT_EQ("checker albedo", compilation.unsupportedTextures()[0].textureName);
    EXPECT_EQ(unsupportedGpuTracingTextureTypeReason, compilation.unsupportedTextures()[0].reason);
    EXPECT_EQ(2u, compilation.unsupportedTextures()[1].texture);
    EXPECT_EQ(unsupportedGpuTracingNullTextureReason, compilation.unsupportedTextures()[1].reason);
    EXPECT_EQ(3u, compilation.unsupportedTextures()[2].texture);
    EXPECT_EQ("uv debug", compilation.unsupportedTextures()[2].textureName);
    EXPECT_EQ(unsupportedGpuTracingTextureTypeReason, compilation.unsupportedTextures()[2].reason);

    const std::vector<UnsupportedGpuTracingReasonCount> reasonCounts =
      compilation.unsupportedReasonCounts();
    ASSERT_EQ(2u, reasonCounts.size());
    EXPECT_EQ(unsupportedGpuTracingTextureTypeReason, reasonCounts[0].reason);
    EXPECT_EQ(2u, reasonCounts[0].count);
    EXPECT_EQ(unsupportedGpuTracingNullTextureReason, reasonCounts[1].reason);
    EXPECT_EQ(1u, reasonCounts[1].count);
  }
}
