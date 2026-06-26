#include <gtest/gtest.h>

#include "render/GpuFloat4.h"
#include "render/GpuTracingScene.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/Light.h"
#include "render/lights/PointLight.h"
#include "render/lights/RectangularAreaLight.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/ImageTexture.h"
#include "render/textures/Texture.h"
#include "render/textures/TintedTexture.h"
#include "render/textures/mappings/UVMapping2D.h"

#include <cstddef>
#include <cstring>
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

    template<typename Record>
    Record recordAt(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
      Record record;
      std::memcpy(&record, bytes.data() + offset, sizeof(Record));
      return record;
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

    class UnsupportedTexture final : public Texturec {
    public:
      Colord evaluate(const Rayd&, const HitPoint&) const override {
        return Colord::white();
      }
    };

    class UnsupportedMaterial final : public Material {
    public:
      Colord shade(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                   State&) const override {
        return Colord::black();
      }
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
    expectKernelRecordLayout<GpuDiffusePathStateRecord>();
    expectKernelRecordLayout<GpuDiffusePathStepRecord>();
    EXPECT_TRUE(std::is_trivially_copyable_v<GpuDiffusePathStateRecord>);
    EXPECT_TRUE(std::is_trivially_copyable_v<GpuDiffusePathStepRecord>);
  }

  TEST(GpuTracingScene, Float4PackingUsesCoreConstructorsAndMethods) {
    expectFloat4(Vector3d(1.25, -2.5, 3.75).toFloat4(1.0f), 1.25f, -2.5f, 3.75f, 1.0f);
    expectFloat4(Colord(0.125, 0.25, 0.5).toFloat4(), 0.125f, 0.25f, 0.5f, 1.0f);
    expectFloat4(Colord(0.125, 0.25, 0.5).toFloat4(0.0f), 0.125f, 0.25f, 0.5f, 0.0f);

    const GpuFloat4 packed{-1.0f, 2.0f, 4.0f, 8.0f};
    EXPECT_EQ(Vector3d(-1.0, 2.0, 4.0), Vector3d(packed));
    EXPECT_EQ(Vector4d(-1.0, 2.0, 4.0, 8.0), Vector4d(packed));
    EXPECT_EQ(Colord(-1.0, 2.0, 4.0), Colord(packed));
    EXPECT_EQ(4.0, gpuFloat4MaxColor(packed));
    EXPECT_TRUE(gpuFloat4HasValue(packed));
    EXPECT_TRUE(gpuFloat4HasValue(GpuFloat4{0.0f, 0.0f, -2.0e-8f, 0.0f}));
    EXPECT_FALSE(gpuFloat4HasValue(GpuFloat4{0.0f, 1.0e-8f, 0.0f, -1.0e-8f}));
    expectFloat4(GpuFloat4{}, 0.0f, 0.0f, 0.0f, 0.0f);
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

  TEST(GpuTracingScene, UploadBytesFollowSectionLayoutOrder) {
    GpuTracingSceneSections sections;
    GpuIntersectionPrimitiveRecord primitive;
    primitive.kind = static_cast<std::uint32_t>(GpuIntersectionPrimitiveKind::Sphere);
    primitive.material = 7u;
    sections.geometry.primitives.push_back(primitive);
    GpuIntersectionSpherePayload sphere;
    sphere.centerRadius = {1.0f, 2.0f, 3.0f, 4.0f};
    sections.geometry.spheres.push_back(sphere);
    GpuTracingMaterialRecord material;
    material.kind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Matte);
    material.albedoTexture = 3u;
    sections.materials.push_back(material);
    GpuTracingTextureRecord texture;
    texture.kind = static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor);
    texture.parameters = {0.25f, 0.5f, 0.75f, 0.0f};
    sections.textures.push_back(texture);
    GpuTracingLightRecord light;
    light.kind = static_cast<std::uint32_t>(GpuTracingLightKind::Point);
    light.parameters = {2.0f, 3.0f, 4.0f, 0.0f};
    sections.lights.push_back(light);
    sections.environment.push_back(makeGpuTracingConstantEnvironment(Colord(0.125, 0.25, 0.5)));
    GpuTracingDebugIdRecord debugId;
    debugId.object = 11u;
    sections.debugIds.push_back(debugId);

    const std::vector<std::uint8_t> bytes = sections.uploadBytes();
    const auto layouts = sections.sectionLayouts();

    ASSERT_EQ(sections.uploadByteCount(), bytes.size());
    const auto materialRecord = recordAt<GpuTracingMaterialRecord>(bytes, layouts[1].byteOffset);
    const auto textureRecord = recordAt<GpuTracingTextureRecord>(bytes, layouts[2].byteOffset);
    const auto lightRecord = recordAt<GpuTracingLightRecord>(bytes, layouts[3].byteOffset);
    const auto environmentRecord =
      recordAt<GpuTracingEnvironmentRecord>(bytes, layouts[4].byteOffset);
    const auto debugRecord = recordAt<GpuTracingDebugIdRecord>(bytes, layouts[5].byteOffset);

    EXPECT_EQ(material.kind, materialRecord.kind);
    EXPECT_EQ(material.albedoTexture, materialRecord.albedoTexture);
    EXPECT_EQ(texture.kind, textureRecord.kind);
    EXPECT_EQ(texture.parameters, textureRecord.parameters);
    EXPECT_EQ(light.kind, lightRecord.kind);
    EXPECT_EQ(light.parameters, lightRecord.parameters);
    EXPECT_EQ(Colord(0.125, 0.25, 0.5).toFloat4(), environmentRecord.color);
    EXPECT_EQ(debugId.object, debugRecord.object);
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

  TEST(GpuTracingScene, DiffusePathStateLayoutPinsGpuFieldOrder) {
    EXPECT_EQ(0u, offsetof(GpuDiffusePathStateRecord, ray));
    EXPECT_EQ(sizeof(GpuIntersectionRay), offsetof(GpuDiffusePathStateRecord, throughput));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, throughput) + sizeof(std::array<float, 4>),
              offsetof(GpuDiffusePathStateRecord, accumulatedRadiance));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, accumulatedRadiance) +
                sizeof(std::array<float, 4>),
              offsetof(GpuDiffusePathStateRecord, pixelIndex));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, pixelIndex) + sizeof(std::uint32_t),
              offsetof(GpuDiffusePathStateRecord, primarySampleIndex));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, primarySampleIndex) + sizeof(std::uint32_t),
              offsetof(GpuDiffusePathStateRecord, depth));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, depth) + sizeof(std::uint32_t),
              offsetof(GpuDiffusePathStateRecord, sampleSeed));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, sampleSeed) + sizeof(std::uint32_t),
              offsetof(GpuDiffusePathStateRecord, sampleDimensionBase));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, sampleDimensionBase) + sizeof(std::uint32_t),
              offsetof(GpuDiffusePathStateRecord, sampleDimensionStride));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, sampleDimensionStride) + sizeof(std::uint32_t),
              offsetof(GpuDiffusePathStateRecord, flags));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, reserved0) + sizeof(std::uint32_t),
              offsetof(GpuDiffusePathStateRecord, previousBsdfPdf));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, previousBsdfPdf) + sizeof(float),
              offsetof(GpuDiffusePathStateRecord, previousLightPdf));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, previousLightPdf) + sizeof(float),
              offsetof(GpuDiffusePathStateRecord, previousMaterial));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, previousMaterial) + sizeof(std::uint32_t),
              offsetof(GpuDiffusePathStateRecord, previousEventFlags));
    EXPECT_EQ(offsetof(GpuDiffusePathStateRecord, previousEventFlags) + sizeof(std::uint32_t),
              offsetof(GpuDiffusePathStateRecord, reserved));
  }

  TEST(GpuTracingScene, DiffusePathStateRepresentsActivePaths) {
    GpuDiffusePathStateRecord pathState = makeActiveGpuDiffusePathState();
    pathState.ray.rayIndex = 11;
    pathState.pixelIndex = 41;
    pathState.primarySampleIndex = 7;
    pathState.depth = 2;
    pathState.sampleSeed = 1234;
    pathState.sampleDimensionBase = 12;
    pathState.sampleDimensionStride = 4;
    pathState.previousBsdfPdf = 0.25f;
    pathState.previousLightPdf = 0.5f;
    pathState.previousMaterial = 3;
    pathState.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag;

    EXPECT_TRUE(gpuDiffusePathStateIsActive(pathState));
    EXPECT_FALSE(gpuDiffusePathStateIsTerminated(pathState));
    expectFloat4(pathState.throughput, 1.0f, 1.0f, 1.0f, 0.0f);
    EXPECT_EQ(11u, pathState.ray.rayIndex);
    EXPECT_EQ(41u, pathState.pixelIndex);
    EXPECT_EQ(7u, pathState.primarySampleIndex);
    EXPECT_EQ(2u, pathState.depth);
    EXPECT_EQ(1234u, pathState.sampleSeed);
    EXPECT_EQ(12u, pathState.sampleDimensionBase);
    EXPECT_EQ(4u, pathState.sampleDimensionStride);
    EXPECT_FLOAT_EQ(0.25f, pathState.previousBsdfPdf);
    EXPECT_FLOAT_EQ(0.5f, pathState.previousLightPdf);
    EXPECT_EQ(3u, pathState.previousMaterial);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag, pathState.previousEventFlags);
  }

  TEST(GpuTracingScene, DiffusePathStateRepresentsTerminatedPaths) {
    const GpuDiffusePathStateRecord pathState = makeTerminatedGpuDiffusePathState();

    EXPECT_FALSE(gpuDiffusePathStateIsActive(pathState));
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(pathState));
    EXPECT_EQ(gpuDiffusePathStateTerminatedFlag, pathState.flags);
  }

  TEST(GpuTracingScene, ConstantEnvironmentPacksColorWithOpaqueAlpha) {
    const GpuTracingEnvironmentRecord environment =
      makeGpuTracingConstantEnvironment(Colord(0.25, 0.5, 0.75));

    EXPECT_FLOAT_EQ(0.25f, environment.color[0]);
    EXPECT_FLOAT_EQ(0.5f, environment.color[1]);
    EXPECT_FLOAT_EQ(0.75f, environment.color[2]);
    EXPECT_FLOAT_EQ(1.0f, environment.color[3]);
  }

  TEST(GpuTracingScene, ConstantColorTexturePacksColorWithOpaqueAlpha) {
    const ConstantColorTexture texture(Colord(0.25, 0.5, 0.75));

    const std::optional<GpuTracingTextureRecord> record = makeGpuTracingTextureRecord(texture);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor), record->kind);
    expectFloat4(record->parameters, 0.25f, 0.5f, 0.75f, 1.0f);
  }

  TEST(GpuTracingScene, CompilesCheckerBoardTextureRecordsWithChildTextureIds) {
    auto brightTexture = std::make_shared<ConstantColorTexture>(Colord(0.8, 0.1, 0.2));
    auto darkTexture = std::make_shared<ConstantColorTexture>(Colord(0.1, 0.2, 0.8));
    auto checkerTexture =
      std::make_shared<CheckerBoardTexture>(new UVMapping2D(4.0, 8.0), brightTexture, darkTexture);
    auto matte = std::make_shared<MatteMaterial>(checkerTexture);
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 0.5);
    sphere->setMaterial(matte);

    Scene scene;
    scene.add(sphere);

    const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
    const GpuTracingMaterialCompilation compilation = compileGpuTracingMaterials(intersection);

    EXPECT_TRUE(compilation.supported());
    ASSERT_EQ(2u, compilation.records.size());
    ASSERT_EQ(4u, compilation.textures.records.size());
    EXPECT_EQ(1u, compilation.records[1].albedoTexture);
    const GpuTracingTextureRecord& checker = compilation.textures.records[1];
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::CheckerBoard), checker.kind);
    EXPECT_EQ(2u, checker.payloadOffset);
    EXPECT_EQ(3u, checker.payloadCount);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureMappingKind::UV), checker.flags);
    expectFloat4(checker.parameters, 4.0f, 8.0f, 0.0f, 0.0f);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor),
              compilation.textures.records[2].kind);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor),
              compilation.textures.records[3].kind);
    expectFloat4(compilation.textures.records[2].parameters, 0.8f, 0.1f, 0.2f, 1.0f);
    expectFloat4(compilation.textures.records[3].parameters, 0.1f, 0.2f, 0.8f, 1.0f);
  }

  TEST(GpuTracingScene, CompilesNearestImageTextureRecordsWithTexelPayloadIds) {
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto imageTexture =
      std::make_shared<ImageTexture>(new UVMapping2D(2.0, 3.0), 2, 2, pixels,
                                     ImageTextureFilter::Nearest, ImageTextureWrap::Clamp);
    auto matte = std::make_shared<MatteMaterial>(imageTexture);
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 0.5);
    sphere->setMaterial(matte);

    Scene scene;
    scene.add(sphere);

    const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
    const GpuTracingMaterialCompilation compilation = compileGpuTracingMaterials(intersection);

    EXPECT_TRUE(compilation.supported());
    ASSERT_EQ(2u, compilation.records.size());
    ASSERT_EQ(6u, compilation.textures.records.size());
    EXPECT_EQ(1u, compilation.records[1].albedoTexture);
    const GpuTracingTextureRecord& image = compilation.textures.records[1];
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::Image), image.kind);
    EXPECT_EQ(2u, image.payloadOffset);
    EXPECT_EQ(4u, image.payloadCount);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureMappingKind::UV) |
                gpuTracingTextureWrapClampFlag,
              image.flags);
    expectFloat4(image.parameters, 2.0f, 3.0f, 2.0f, 2.0f);
    for (std::uint32_t textureId = 2; textureId != 6; ++textureId) {
      EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor),
                compilation.textures.records[textureId].kind);
    }
    expectFloat4(compilation.textures.records[2].parameters, 1.0f, 0.0f, 0.0f, 1.0f);
    expectFloat4(compilation.textures.records[3].parameters, 0.0f, 1.0f, 0.0f, 1.0f);
    expectFloat4(compilation.textures.records[4].parameters, 0.0f, 0.0f, 1.0f, 1.0f);
    expectFloat4(compilation.textures.records[5].parameters, 1.0f, 1.0f, 1.0f, 1.0f);
  }

  TEST(GpuTracingScene, CompilesTintedTextureRecordsWithChildTextureId) {
    auto childTexture = std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75));
    auto tintedTexture = std::make_shared<TintedTexture>(childTexture, Colord(0.5, 0.25, 0.125));
    auto matte = std::make_shared<MatteMaterial>(tintedTexture);
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 0.5);
    sphere->setMaterial(matte);

    Scene scene;
    scene.add(sphere);

    const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
    const GpuTracingMaterialCompilation compilation = compileGpuTracingMaterials(intersection);

    EXPECT_TRUE(compilation.supported());
    ASSERT_EQ(2u, compilation.records.size());
    ASSERT_EQ(3u, compilation.textures.records.size());
    EXPECT_EQ(1u, compilation.records[1].albedoTexture);
    const GpuTracingTextureRecord& tinted = compilation.textures.records[1];
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::Tinted), tinted.kind);
    EXPECT_EQ(2u, tinted.payloadOffset);
    expectFloat4(tinted.parameters, 0.5f, 0.25f, 0.125f, 1.0f);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor),
              compilation.textures.records[2].kind);
    expectFloat4(compilation.textures.records[2].parameters, 0.25f, 0.5f, 0.75f, 1.0f);
  }

  TEST(GpuTracingScene, CompilesTintedImageTextureRecordsWithImageChildTextureId) {
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto imageTexture =
      std::make_shared<ImageTexture>(new UVMapping2D(2.0, 3.0), 2, 2, pixels,
                                     ImageTextureFilter::Nearest, ImageTextureWrap::Clamp);
    auto tintedTexture = std::make_shared<TintedTexture>(imageTexture, Colord(0.5, 0.25, 0.125));
    auto matte = std::make_shared<MatteMaterial>(tintedTexture);
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 0.5);
    sphere->setMaterial(matte);

    Scene scene;
    scene.add(sphere);

    const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
    const GpuTracingMaterialCompilation compilation = compileGpuTracingMaterials(intersection);

    EXPECT_TRUE(compilation.supported());
    ASSERT_EQ(2u, compilation.records.size());
    ASSERT_EQ(7u, compilation.textures.records.size());
    EXPECT_EQ(1u, compilation.records[1].albedoTexture);
    const GpuTracingTextureRecord& tinted = compilation.textures.records[1];
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::Tinted), tinted.kind);
    EXPECT_EQ(2u, tinted.payloadOffset);
    expectFloat4(tinted.parameters, 0.5f, 0.25f, 0.125f, 1.0f);
    const GpuTracingTextureRecord& image = compilation.textures.records[2];
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::Image), image.kind);
    EXPECT_EQ(3u, image.payloadOffset);
    EXPECT_EQ(4u, image.payloadCount);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureMappingKind::UV) |
                gpuTracingTextureWrapClampFlag,
              image.flags);
  }

  TEST(GpuTracingScene, MatteMaterialPacksTextureIdsAndCoefficients) {
    MatteMaterial material;
    material.setAmbientCoefficient(0.25);
    material.setDiffuseCoefficient(0.75);

    const std::optional<GpuTracingMaterialRecord> record =
      makeGpuTracingMaterialRecord(material, 4, 0);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Matte), record->kind);
    EXPECT_EQ(4u, record->albedoTexture);
    EXPECT_EQ(0u, record->emissionTexture);
    EXPECT_FLOAT_EQ(0.25f, record->parameters[0]);
    EXPECT_FLOAT_EQ(0.75f, record->parameters[1]);
  }

  TEST(GpuTracingScene, EmissiveMaterialPacksEmissionTextureId) {
    const EmissiveMaterial material(Colord(0.8, 0.6, 0.4));

    const std::optional<GpuTracingMaterialRecord> record =
      makeGpuTracingMaterialRecord(material, 0, 7);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Emissive), record->kind);
    EXPECT_EQ(0u, record->albedoTexture);
    EXPECT_EQ(7u, record->emissionTexture);
  }

  TEST(GpuTracingScene, PhongMaterialPacksLocalLightingCoefficients) {
    PhongMaterial material;
    material.setAmbientCoefficient(0.125);
    material.setDiffuseCoefficient(0.25);
    material.setSpecularColor(Colord(0.75, 0.5, 0.25));
    material.setSpecularCoefficient(0.5);
    material.setExponent(32.0);

    const std::optional<GpuTracingMaterialRecord> record =
      makeGpuTracingMaterialRecord(material, 4, 0);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Phong), record->kind);
    EXPECT_EQ(4u, record->albedoTexture);
    expectFloat4(record->parameters, 0.125f, 0.25f, 0.5f, 32.0f);
    expectFloat4(record->specularParameters, 0.75f, 0.5f, 0.25f, 0.0f);
    expectFloat4(record->continuationParameters, 0.0f, 0.0f, 0.0f, 0.0f);
  }

  TEST(GpuTracingScene, ReflectiveMaterialPacksMirrorContinuationParameters) {
    ReflectiveMaterial material;
    material.setAmbientCoefficient(0.125);
    material.setDiffuseCoefficient(0.25);
    material.setSpecularColor(Colord(0.25, 0.5, 0.75));
    material.setSpecularCoefficient(0.5);
    material.setExponent(32.0);
    material.setReflectionColor(Colord(0.75, 0.5, 0.25));
    material.setReflectionCoefficient(0.375);

    const std::optional<GpuTracingMaterialRecord> record =
      makeGpuTracingMaterialRecord(material, 4, 0);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Reflective), record->kind);
    EXPECT_EQ(4u, record->albedoTexture);
    expectFloat4(record->parameters, 0.125f, 0.25f, 0.5f, 32.0f);
    expectFloat4(record->specularParameters, 0.25f, 0.5f, 0.75f, 0.0f);
    expectFloat4(record->continuationParameters, 0.75f, 0.5f, 0.25f, 0.375f);
  }

  TEST(GpuTracingScene, TransparentMaterialPacksLocalReflectionAndTransmissionParameters) {
    TransparentMaterial material;
    material.setAmbientCoefficient(0.125);
    material.setDiffuseCoefficient(0.25);
    material.setSpecularColor(Colord(0.2, 0.4, 0.8));
    material.setSpecularCoefficient(0.5);
    material.setExponent(32.0);
    material.setReflectionColor(Colord(0.75, 0.5, 0.25));
    material.setReflectionCoefficient(0.375);
    material.setTransmissionCoefficient(0.625);
    material.setRefractionIndex(1.5);

    const std::optional<GpuTracingMaterialRecord> record =
      makeGpuTracingMaterialRecord(material, 4, 0);

    ASSERT_TRUE(record.has_value());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Transparent), record->kind);
    EXPECT_EQ(4u, record->albedoTexture);
    expectFloat4(record->parameters, 0.125f, 0.25f, 0.5f, 32.0f);
    expectFloat4(record->specularParameters, 0.2f, 0.4f, 0.8f, 0.0f);
    expectFloat4(record->continuationParameters, 0.75f, 0.5f, 0.25f, 0.375f);
    expectFloat4(record->transmissionParameters, 0.625f, 1.5f, 0.0f, 0.0f);
  }

  TEST(GpuTracingScene, CompilesMaterialAndTextureRecordsAtRuntimeIds) {
    auto redTexture = std::make_shared<ConstantColorTexture>(Colord(0.8, 0.1, 0.2));
    auto matte = std::make_shared<MatteMaterial>(redTexture);
    matte->setAmbientCoefficient(0.25);
    matte->setDiffuseCoefficient(0.75);
    auto emissive = std::make_shared<EmissiveMaterial>(Colord(1.0, 0.5, 0.25));

    auto firstSphere = std::make_shared<Sphere>(Vector3d(-1.0, 0.0, 0.0), 0.5);
    firstSphere->setMaterial(matte);
    auto secondSphere = std::make_shared<Sphere>(Vector3d(1.0, 0.0, 0.0), 0.5);
    secondSphere->setMaterial(emissive);
    auto thirdSphere = std::make_shared<Sphere>(Vector3d(3.0, 0.0, 0.0), 0.5);
    thirdSphere->setMaterial(matte);

    Scene scene;
    scene.add(firstSphere);
    scene.add(secondSphere);
    scene.add(thirdSphere);

    const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
    const GpuTracingMaterialCompilation compilation = compileGpuTracingMaterials(intersection);

    ASSERT_EQ(3u, intersection.materials().size());
    ASSERT_EQ(3u, intersection.primitives().size());
    EXPECT_EQ(1u, intersection.primitives()[0].material);
    EXPECT_EQ(2u, intersection.primitives()[1].material);
    EXPECT_EQ(1u, intersection.primitives()[2].material);

    EXPECT_TRUE(compilation.supported());
    ASSERT_EQ(intersection.materials().size(), compilation.records.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Unsupported),
              compilation.records[0].kind);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Matte),
              compilation.records[1].kind);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Emissive),
              compilation.records[2].kind);

    ASSERT_EQ(3u, compilation.textures.records.size());
    EXPECT_EQ(1u, compilation.records[1].albedoTexture);
    EXPECT_EQ(2u, compilation.records[2].emissionTexture);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor),
              compilation.textures.records[1].kind);
    expectFloat4(compilation.textures.records[1].parameters, 0.8f, 0.1f, 0.2f, 1.0f);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor),
              compilation.textures.records[2].kind);
    expectFloat4(compilation.textures.records[2].parameters, 1.0f, 0.5f, 0.25f, 1.0f);
  }

  TEST(GpuTracingScene, RecordsFirstUnsupportedMaterialReasonAndGroupedCounts) {
    auto firstUnsupported = std::make_shared<Sphere>(Vector3d(-1.0, 0.0, 0.0), 0.5);
    firstUnsupported->setMaterial(std::make_shared<UnsupportedMaterial>());
    auto supported = std::make_shared<Sphere>(Vector3d(1.0, 0.0, 0.0), 0.5);
    supported->setMaterial(
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::red())));
    auto secondUnsupported = std::make_shared<Sphere>(Vector3d(3.0, 0.0, 0.0), 0.5);
    secondUnsupported->setMaterial(std::make_shared<UnsupportedMaterial>());

    Scene scene;
    scene.add(firstUnsupported);
    scene.add(supported);
    scene.add(secondUnsupported);

    const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
    const GpuTracingMaterialCompilation compilation = compileGpuTracingMaterials(intersection);

    EXPECT_FALSE(compilation.supported());
    ASSERT_EQ(2u, compilation.unsupportedMaterials.size());
    EXPECT_EQ(1u, compilation.unsupportedMaterials[0].materialId);
    EXPECT_EQ("Material", compilation.unsupportedMaterials[0].type);
    EXPECT_EQ("material type is not supported by GPU tracing scene compiler",
              compilation.unsupportedMaterials[0].reason);
    EXPECT_EQ(3u, compilation.unsupportedMaterials[1].materialId);

    const std::vector<GpuTracingUnsupportedReasonCount> reasonCounts =
      compilation.unsupportedReasonCounts();
    ASSERT_EQ(1u, reasonCounts.size());
    EXPECT_EQ("material type is not supported by GPU tracing scene compiler",
              reasonCounts[0].reason);
    EXPECT_EQ(2u, reasonCounts[0].count);
  }

  TEST(GpuTracingScene, RecordsFirstUnsupportedTextureReasonAndGroupedCounts) {
    auto firstUnsupportedTexture = std::make_shared<UnsupportedTexture>();
    auto secondUnsupportedTexture = std::make_shared<UnsupportedTexture>();
    auto firstMaterial = std::make_shared<MatteMaterial>(firstUnsupportedTexture);
    auto secondMaterial = std::make_shared<MatteMaterial>(secondUnsupportedTexture);

    auto firstSphere = std::make_shared<Sphere>(Vector3d(-1.0, 0.0, 0.0), 0.5);
    firstSphere->setMaterial(firstMaterial);
    auto secondSphere = std::make_shared<Sphere>(Vector3d(1.0, 0.0, 0.0), 0.5);
    secondSphere->setMaterial(secondMaterial);

    Scene scene;
    scene.add(firstSphere);
    scene.add(secondSphere);

    const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
    const GpuTracingMaterialCompilation compilation = compileGpuTracingMaterials(intersection);

    EXPECT_FALSE(compilation.supported());
    ASSERT_EQ(2u, compilation.textures.unsupportedTextures.size());
    EXPECT_EQ(1u, compilation.textures.unsupportedTextures[0].textureId);
    EXPECT_EQ("Texture", compilation.textures.unsupportedTextures[0].type);
    EXPECT_EQ("texture type is not supported by GPU tracing scene compiler",
              compilation.textures.unsupportedTextures[0].reason);
    EXPECT_EQ(2u, compilation.textures.unsupportedTextures[1].textureId);
    EXPECT_EQ("Texture", compilation.textures.unsupportedTextures[1].type);

    const std::vector<GpuTracingUnsupportedReasonCount> reasonCounts =
      compilation.textures.unsupportedReasonCounts();
    ASSERT_EQ(1u, reasonCounts.size());
    EXPECT_EQ("texture type is not supported by GPU tracing scene compiler",
              reasonCounts[0].reason);
    EXPECT_EQ(2u, reasonCounts[0].count);
  }

  TEST(GpuTracingScene, DiagnosticsExposeCompiledSectionAndUnsupportedCounts) {
    auto unsupportedMaterialSphere = std::make_shared<Sphere>(Vector3d(-1.0, 0.0, 0.0), 0.5);
    unsupportedMaterialSphere->setMaterial(std::make_shared<UnsupportedMaterial>());
    auto unsupportedTextureSphere = std::make_shared<Sphere>(Vector3d(1.0, 0.0, 0.0), 0.5);
    unsupportedTextureSphere->setMaterial(
      std::make_shared<MatteMaterial>(std::make_shared<UnsupportedTexture>()));

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    scene.add(unsupportedMaterialSphere);
    scene.add(unsupportedTextureSphere);
    scene.addLight(std::make_shared<PointLight>(Vector3d(1.0, 2.0, 3.0), Colord::white()));
    scene.addLight(std::make_shared<UnsupportedLight>("UnsupportedLight"));

    const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
    const GpuTracingSceneDiagnostics diagnostics =
      compileGpuTracingSceneDiagnostics(intersection, scene);

    EXPECT_TRUE(diagnostics.compiled);
    EXPECT_EQ(3u, diagnostics.materials);
    EXPECT_EQ(2u, diagnostics.textures);
    EXPECT_EQ(1u, diagnostics.lights);
    EXPECT_EQ(3u, diagnostics.environment);
    EXPECT_EQ(0u, diagnostics.debugIds);
    EXPECT_EQ(1u, diagnostics.unsupportedMaterials);
    EXPECT_EQ(1u, diagnostics.unsupportedTextures);
    EXPECT_EQ(1u, diagnostics.unsupportedLights);
    EXPECT_EQ(0u, diagnostics.unsupportedPrimitives);
    EXPECT_TRUE(diagnostics.unsupportedPrimitiveReasons.empty());
    EXPECT_EQ(1u, diagnostics.unsupportedMaterialReasons.at(
                    "material type is not supported by GPU tracing scene compiler"));
    EXPECT_EQ(1u, diagnostics.unsupportedTextureReasons.at(
                    "texture type is not supported by GPU tracing scene compiler"));
    EXPECT_EQ(1u, diagnostics.unsupportedLightReasons.at(
                    "light type is not supported by GPU tracing scene compiler"));
    EXPECT_GT(diagnostics.uploadBytes, 0u);
  }

  TEST(GpuTracingScene, CompilesSectionsAndDiagnosticsTogether) {
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    sphere->setMaterial(matte);
    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    scene.add(sphere);
    scene.addLight(std::make_shared<PointLight>(Vector3d(1.0, 2.0, 3.0), Colord::white()));

    const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
    const GpuTracingSceneCompilation compilation = compileGpuTracingScene(intersection, scene);

    EXPECT_TRUE(compilation.supported());
    EXPECT_TRUE(compilation.diagnostics.compiled);
    EXPECT_EQ(intersection.primitives().size(), compilation.sections.geometry.primitives.size());
    EXPECT_EQ(intersection.spheres().size(), compilation.sections.geometry.spheres.size());
    EXPECT_EQ(intersection.materials().size(), compilation.sections.materials.size());
    EXPECT_EQ(1u, compilation.sections.lights.size());
    EXPECT_EQ(3u, compilation.sections.environment.size());
    EXPECT_EQ(compilation.sections.materials.size(), compilation.diagnostics.materials);
    EXPECT_EQ(compilation.sections.textures.size(), compilation.diagnostics.textures);
    EXPECT_EQ(compilation.sections.lights.size(), compilation.diagnostics.lights);
    EXPECT_EQ(compilation.sections.environment.size(), compilation.diagnostics.environment);
    EXPECT_EQ(compilation.sections.uploadByteCount(), compilation.diagnostics.uploadBytes);
  }

  TEST(GpuTracingScene, CombinedCompilationSupportsTransparentMaterials) {
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    sphere->setMaterial(std::make_shared<TransparentMaterial>());
    Scene scene;
    scene.add(sphere);

    const GpuTracingSceneCompilation compilation = compileGpuTracingScene(scene);

    EXPECT_TRUE(compilation.supported());
    EXPECT_EQ(0u, compilation.diagnostics.unsupportedPrimitives);
    EXPECT_TRUE(compilation.diagnostics.unsupportedPrimitiveReasons.empty());
    const auto transparentKind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Transparent);
    bool foundTransparentMaterial = false;
    for (const GpuTracingMaterialRecord& material : compilation.sections.materials) {
      foundTransparentMaterial = foundTransparentMaterial || material.kind == transparentKind;
    }
    EXPECT_TRUE(foundTransparentMaterial);
  }

  TEST(GpuTracingScene, DiffusePathLoopSupportAcceptsMattePhongAndEmissiveScenes) {
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    sphere->setMaterial(matte);
    auto phong = std::make_shared<PhongMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.5, 0.25, 0.125)), Colord::white(), 16.0);
    auto phongSphere = std::make_shared<Sphere>(Vector3d(-3.0, 0.0, 0.0), 0.5);
    phongSphere->setMaterial(phong);
    auto emissive = std::make_shared<EmissiveMaterial>(Colord(1.0, 0.5, 0.25));
    auto lightSphere = std::make_shared<Sphere>(Vector3d(3.0, 0.0, 0.0), 0.5);
    lightSphere->setMaterial(emissive);
    Scene scene;
    scene.setBackground(Colord(0.1, 0.2, 0.3));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    scene.add(sphere);
    scene.add(phongSphere);
    scene.add(lightSphere);

    const GpuTracingSceneCompilation compilation = compileGpuTracingScene(scene);
    const GpuDiffusePathLoopSupport support = gpuDiffusePathLoopSupport(compilation, scene);

    EXPECT_TRUE(support.supported);
    EXPECT_TRUE(support.reason.empty());
    EXPECT_TRUE(supportsGpuDiffusePathLoop(compilation, scene));
  }

  TEST(GpuTracingScene, DiffusePathLoopSupportAcceptsReflectiveMaterials) {
    auto reflective = std::make_shared<ReflectiveMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    sphere->setMaterial(reflective);
    Scene scene;
    scene.setBackground(Colord(0.1, 0.2, 0.3));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    scene.add(sphere);

    const GpuTracingSceneCompilation compilation = compileGpuTracingScene(scene);
    ASSERT_TRUE(compilation.supported());
    const GpuDiffusePathLoopSupport support = gpuDiffusePathLoopSupport(compilation, scene);

    EXPECT_TRUE(support.supported);
    EXPECT_TRUE(support.reason.empty());
    EXPECT_TRUE(gpuDiffusePathLoopUnsupportedReason(compilation, scene).empty());
  }

  TEST(GpuTracingScene, DiffusePathLoopSupportAcceptsTransparentMaterials) {
    auto transparent = std::make_shared<TransparentMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    sphere->setMaterial(transparent);
    Scene scene;
    scene.setBackground(Colord(0.1, 0.2, 0.3));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    scene.add(sphere);

    const GpuTracingSceneCompilation compilation = compileGpuTracingScene(scene);
    ASSERT_TRUE(compilation.supported());
    const GpuDiffusePathLoopSupport support = gpuDiffusePathLoopSupport(compilation, scene);

    EXPECT_TRUE(support.supported);
    EXPECT_TRUE(support.reason.empty());
    EXPECT_TRUE(gpuDiffusePathLoopUnsupportedReason(compilation, scene).empty());
  }

  TEST(GpuTracingScene, DiffusePathLoopSupportAllowsDifferentBackgroundAndEnvironment) {
    Scene scene;
    scene.setAmbient(Colord(0.7, 0.8, 0.9));
    scene.setBackground(Colord(0.1, 0.2, 0.3));
    scene.setEnvironmentRadiance(Colord(0.4, 0.5, 0.6));

    const GpuTracingSceneCompilation compilation = compileGpuTracingScene(scene);
    const GpuDiffusePathLoopSupport support = gpuDiffusePathLoopSupport(compilation, scene);

    EXPECT_TRUE(support.supported);
    ASSERT_EQ(3u, compilation.sections.environment.size());
    expectFloat4(compilation.sections.environment[0].color, 0.7f, 0.8f, 0.9f, 1.0f);
    expectFloat4(compilation.sections.environment[1].color, 0.1f, 0.2f, 0.3f, 1.0f);
    expectFloat4(compilation.sections.environment[2].color, 0.4f, 0.5f, 0.6f, 1.0f);
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
    EXPECT_EQ("light type is not supported by GPU tracing scene compiler",
              compilation.unsupportedLights[0].reason);
    EXPECT_EQ(3u, compilation.unsupportedLights[1].lightIndex);
    EXPECT_EQ("UnsupportedB", compilation.unsupportedLights[1].type);

    const std::vector<GpuTracingUnsupportedReasonCount> reasonCounts =
      compilation.unsupportedReasonCounts();
    ASSERT_EQ(1u, reasonCounts.size());
    EXPECT_EQ("light type is not supported by GPU tracing scene compiler", reasonCounts[0].reason);
    EXPECT_EQ(2u, reasonCounts[0].count);
  }

}
