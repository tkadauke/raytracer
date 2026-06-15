#include <gtest/gtest.h>

#include "core/Color.h"
#include "core/math/Constants.h"
#include "core/math/Vector.h"
#include "render/GpuCompiledLightSampler.h"
#include "render/GpuDirectLightCpuReference.h"
#include "render/GpuDirectLightWork.h"
#include "render/GpuIntersectionScene.h"
#include "render/IntersectionService.h"
#include "render/GpuTracingScene.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/LightSampler.h"
#include "render/lights/PointLight.h"
#include "render/lights/RectangularAreaLight.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/samplers/GpuSampleStream.h"

#include <cmath>
#include <memory>
#include <type_traits>

namespace GpuDirectLightWorkTest {
  using namespace render;

  namespace {
    template<typename Record>
    void expectKernelRecordLayout() {
      EXPECT_TRUE(std::is_standard_layout_v<Record>);
      EXPECT_EQ(16u, alignof(Record));
      EXPECT_EQ(0u, sizeof(Record) % 16u);
    }

    GpuTracingSceneSections oneMattePointLightScene() {
      GpuTracingSceneSections scene;
      scene.materials.resize(2);
      scene.textures.resize(2);

      scene.textures[1].kind = static_cast<std::uint32_t>(GpuTracingTextureKind::ConstantColor);
      scene.textures[1].parameters = {0.5f, 0.25f, 1.0f, 1.0f};

      scene.materials[1].kind = static_cast<std::uint32_t>(GpuTracingMaterialKind::Matte);
      scene.materials[1].albedoTexture = 1u;
      scene.materials[1].parameters = {0.0f, 0.8f, 0.0f, 0.0f};

      GpuTracingLightRecord light;
      light.kind = static_cast<std::uint32_t>(GpuTracingLightKind::Point);
      light.positionOrDirection = {0.0f, 4.0f, 0.0f, 1.0f};
      light.parameters = {2.0f, 3.0f, 4.0f, 1.0f};
      scene.lights.push_back(light);
      return scene;
    }

    GpuDirectLightWorkRecord oneMattePointLightWork() {
      GpuDirectLightWorkRecord work;
      work.surface.material = 1u;
      work.surface.pathIndex = 7u;
      work.surface.point = {0.0f, 0.0f, 0.0f, 1.0f};
      work.surface.normal = {0.0f, 1.0f, 0.0f, 0.0f};
      work.surface.incomingDirection = {0.0f, 1.0f, 0.0f, 0.0f};
      work.surface.throughput = {0.25f, 0.5f, 1.0f, 1.0f};
      work.sample = makeGpuDirectLightSampleState(/*seed=*/42, /*pixelIndex=*/19,
                                                  /*primarySampleIndex=*/3, /*bounce=*/0,
                                                  /*directSampleIndex=*/0);
      work.lightSelection.lightBegin = 0u;
      work.lightSelection.lightCount = 1u;
      return work;
    }

    void expectVectorNear(const Vector3d& actual, const Vector3d& expected) {
      EXPECT_NEAR(expected.x(), actual.x(), 1e-12);
      EXPECT_NEAR(expected.y(), actual.y(), 1e-12);
      EXPECT_NEAR(expected.z(), actual.z(), 1e-12);
    }

    void expectColorNear(const Colord& actual, const Colord& expected) {
      EXPECT_NEAR(expected.r(), actual.r(), 1e-12);
      EXPECT_NEAR(expected.g(), actual.g(), 1e-12);
      EXPECT_NEAR(expected.b(), actual.b(), 1e-12);
    }

    void expectCompiledSampleMatchesRuntime(const GpuTracingLightRecord& record,
                                            const Light& runtimeLight, const Vector3d& point,
                                            const Vector2d& surfaceSample) {
      const GpuCompiledLightSample compiled = sampleGpuCompiledLight(record, point, surfaceSample);
      const LightSample runtime = runtimeLight.sample(point, surfaceSample);

      ASSERT_TRUE(compiled.valid());
      expectVectorNear(compiled.direction, runtime.direction);
      expectColorNear(compiled.radiance, runtime.radiance);
      EXPECT_DOUBLE_EQ(runtime.distance, compiled.distance);
      EXPECT_NEAR(runtime.pdf, compiled.pdf, 1e-12);
      EXPECT_EQ(runtime.delta, compiled.delta);
      EXPECT_DOUBLE_EQ(runtimeLight.pdf(point, runtime.direction),
                       gpuCompiledLightPdf(record, point, runtime.direction));
    }
  }

  TEST(GpuDirectLightWork, RecordsHaveStableKernelFriendlyLayout) {
    expectKernelRecordLayout<GpuDirectLightSurfaceRecord>();
    expectKernelRecordLayout<GpuDirectLightSampleStateRecord>();
    expectKernelRecordLayout<GpuDirectLightSelectionRecord>();
    expectKernelRecordLayout<GpuDirectLightVisibilityRecord>();
    expectKernelRecordLayout<GpuDirectLightWorkRecord>();
    expectKernelRecordLayout<GpuDirectLightContributionRecord>();
  }

  TEST(GpuDirectLightWork, WorkRecordIsSeparateFromIntersectionHitRecord) {
    constexpr bool sameRecordType =
      std::is_same_v<GpuDirectLightWorkRecord, GpuIntersectionHitRecord>;
    EXPECT_FALSE(sameRecordType);

    GpuIntersectionHitRecord hit;
    hit.material = 3;
    hit.rayIndex = 11;

    GpuDirectLightWorkRecord work;
    work.surface.material = hit.material;
    work.surface.pathIndex = hit.rayIndex;
    work.surface.throughput = {0.25f, 0.5f, 0.75f, 1.0f};

    EXPECT_EQ(3u, work.surface.material);
    EXPECT_EQ(11u, work.surface.pathIndex);
    EXPECT_FLOAT_EQ(0.25f, work.surface.throughput[0]);
    EXPECT_FLOAT_EQ(0.5f, work.surface.throughput[1]);
    EXPECT_FLOAT_EQ(0.75f, work.surface.throughput[2]);
    EXPECT_FLOAT_EQ(1.0f, work.surface.throughput[3]);
  }

  TEST(GpuDirectLightWork, SurfaceRecordCarriesDiffuseContributionInputs) {
    GpuDirectLightSurfaceRecord surface;
    surface.material = 7;
    surface.object = 9;
    surface.primitiveRecord = 13;
    surface.pathIndex = 21;
    surface.point = {1.0f, 2.0f, 3.0f, 1.0f};
    surface.normal = {0.0f, 1.0f, 0.0f, 0.0f};
    surface.incomingDirection = {0.0f, 0.0f, -1.0f, 0.0f};
    surface.throughput = {0.4f, 0.5f, 0.6f, 1.0f};

    EXPECT_EQ(7u, surface.material);
    EXPECT_EQ(9u, surface.object);
    EXPECT_EQ(13u, surface.primitiveRecord);
    EXPECT_EQ(21u, surface.pathIndex);
    EXPECT_FLOAT_EQ(1.0f, surface.point[0]);
    EXPECT_FLOAT_EQ(1.0f, surface.normal[1]);
    EXPECT_FLOAT_EQ(-1.0f, surface.incomingDirection[2]);
    EXPECT_FLOAT_EQ(0.6f, surface.throughput[2]);
  }

  TEST(GpuDirectLightWork, SampleStateUsesNamedLightDimensions) {
    const GpuDirectLightSampleStateRecord sample =
      makeGpuDirectLightSampleState(/*seed=*/42, /*pixelIndex=*/19,
                                    /*primarySampleIndex=*/3, /*bounce=*/2,
                                    /*directSampleIndex=*/5);

    EXPECT_EQ(42u, sample.seed);
    EXPECT_EQ(19u, sample.pixelIndex);
    EXPECT_EQ(3u, sample.primarySampleIndex);
    EXPECT_EQ(2u, sample.bounce);
    EXPECT_EQ(5u, sample.directSampleIndex);
    EXPECT_EQ(gpuDirectLightSelectionSampleDimension(/*bounce=*/2, /*directSampleIndex=*/5),
              sample.lightSelectionDimension);
    EXPECT_EQ(gpuDirectLightSurfaceSampleDimension(/*bounce=*/2, /*lightIndex=*/0,
                                                   /*directSampleIndex=*/5),
              sample.lightSurfaceDimensionBase);
    EXPECT_NE(sample.lightSelectionDimension, sample.lightSurfaceDimensionBase);
  }

  TEST(GpuDirectLightWork, LightSurfaceDimensionsIncludeSelectedLightIndex) {
    const std::uint64_t firstLight =
      gpuDirectLightSurfaceSampleDimension(/*bounce=*/1, /*lightIndex=*/0,
                                           /*directSampleIndex=*/2);
    const std::uint64_t secondLight =
      gpuDirectLightSurfaceSampleDimension(/*bounce=*/1, /*lightIndex=*/1,
                                           /*directSampleIndex=*/2);

    EXPECT_NE(firstLight, secondLight);
    EXPECT_EQ(sampleDimensionIndex(SampleDimension::Light,
                                   SampleStream::lightSampleIndex(/*bounce=*/1,
                                                                  /*lightIndex=*/1,
                                                                  /*directSampleIndex=*/2)),
              secondLight);
  }

  TEST(GpuDirectLightWork, SelectionRecordAddressesCompiledLightRange) {
    GpuDirectLightSelectionRecord selection;
    selection.lightBegin = 4;
    selection.lightCount = 6;
    selection.selectedLight = 7;

    EXPECT_EQ(4u, selection.lightBegin);
    EXPECT_EQ(6u, selection.lightCount);
    EXPECT_EQ(7u, selection.selectedLight);
    EXPECT_GE(selection.selectedLight, selection.lightBegin);
    EXPECT_LT(selection.selectedLight, selection.lightBegin + selection.lightCount);
  }

  TEST(GpuDirectLightWork, VisibilityRecordCarriesShadowRayAndContributionInputs) {
    GpuDirectLightVisibilityRecord visibility;
    visibility.workIndex = 3;
    visibility.lightIndex = 8;
    visibility.rayOrigin = {1.0f, 2.0f, 3.0f, 1.0f};
    visibility.rayDirection = {0.0f, 1.0f, 0.0f, 0.0f};
    visibility.lightRadiance = {4.0f, 5.0f, 6.0f, 1.0f};
    visibility.lightSample = {0.25f, 0.75f, 0.0f, 0.0f};
    visibility.minDistance = 0.001f;
    visibility.maxDistance = 9.0f;
    visibility.lightPdf = 0.5f;
    visibility.selectionPdf = 0.25f;

    EXPECT_EQ(3u, visibility.workIndex);
    EXPECT_EQ(8u, visibility.lightIndex);
    EXPECT_FLOAT_EQ(0.001f, visibility.minDistance);
    EXPECT_FLOAT_EQ(9.0f, visibility.maxDistance);
    EXPECT_FLOAT_EQ(0.5f, visibility.lightPdf);
    EXPECT_FLOAT_EQ(0.25f, visibility.selectionPdf);
    EXPECT_FLOAT_EQ(6.0f, visibility.lightRadiance[2]);
    EXPECT_FLOAT_EQ(0.75f, visibility.lightSample[1]);
  }

  TEST(GpuDirectLightCpuReference, BuildsVisibilityRecordFromPackedPointLight) {
    const GpuTracingSceneSections scene = oneMattePointLightScene();
    const GpuDirectLightWorkRecord work = oneMattePointLightWork();

    const GpuDirectLightVisibilityRecord visibility =
      makeGpuDirectLightCpuVisibilityRecord(scene, work, /*workIndex=*/3u);

    EXPECT_EQ(3u, visibility.workIndex);
    EXPECT_EQ(0u, visibility.lightIndex);
    EXPECT_EQ(gpuDirectLightVisibilityValid | gpuDirectLightVisibilityDeltaLight, visibility.flags);
    EXPECT_EQ(0u, visibility.occluded);
    EXPECT_GT(visibility.rayOrigin[1], work.surface.point[1]);
    EXPECT_FLOAT_EQ(0.0f, visibility.rayDirection[0]);
    EXPECT_FLOAT_EQ(1.0f, visibility.rayDirection[1]);
    EXPECT_FLOAT_EQ(0.0f, visibility.rayDirection[2]);
    EXPECT_FLOAT_EQ(4.0f, visibility.maxDistance);
    EXPECT_FLOAT_EQ(1.0f, visibility.lightPdf);
    EXPECT_FLOAT_EQ(1.0f, visibility.selectionPdf);
    EXPECT_FLOAT_EQ(2.0f, visibility.lightRadiance[0]);
    EXPECT_FLOAT_EQ(3.0f, visibility.lightRadiance[1]);
    EXPECT_FLOAT_EQ(4.0f, visibility.lightRadiance[2]);
  }

  TEST(GpuCompiledLightSampler, SamplesSupportedCompiledLightsLikeRuntimeLights) {
    const Vector3d point(0.25, 0.5, -0.75);
    const Vector2d surfaceSample(0.2, 0.7);

    GpuTracingLightRecord pointRecord;
    pointRecord.kind = static_cast<std::uint32_t>(GpuTracingLightKind::Point);
    pointRecord.positionOrDirection = {1.0f, 4.0f, -2.0f, 1.0f};
    pointRecord.parameters = {2.0f, 3.0f, 4.0f, 1.0f};
    const PointLight pointLight(Vector3d(1.0, 4.0, -2.0), Colord(2.0, 3.0, 4.0));
    expectCompiledSampleMatchesRuntime(pointRecord, pointLight, point, surfaceSample);

    GpuTracingLightRecord directionalRecord;
    directionalRecord.kind = static_cast<std::uint32_t>(GpuTracingLightKind::Directional);
    directionalRecord.positionOrDirection = {0.0f, 1.0f, 1.0f, 0.0f};
    directionalRecord.parameters = {0.5f, 0.75f, 1.25f, 1.0f};
    const DirectionalLight directionalLight(Vector3d(0.0, 1.0, 1.0), Colord(0.5, 0.75, 1.25));
    expectCompiledSampleMatchesRuntime(directionalRecord, directionalLight, point, surfaceSample);

    GpuTracingLightRecord areaRecord;
    areaRecord.kind = static_cast<std::uint32_t>(GpuTracingLightKind::RectangularArea);
    areaRecord.positionOrDirection = {0.0f, 4.0f, 0.0f, 1.0f};
    areaRecord.u = {2.0f, 0.0f, 0.0f, 0.0f};
    areaRecord.v = {0.0f, 0.0f, 2.0f, 0.0f};
    areaRecord.parameters = {1.5f, 1.0f, 0.5f, 1.0f};
    const RectangularAreaLight areaLight(Vector3d(0.0, 4.0, 0.0), Vector3d(2.0, 0.0, 0.0),
                                         Vector3d(0.0, 0.0, 2.0), Colord(1.5, 1.0, 0.5));
    expectCompiledSampleMatchesRuntime(areaRecord, areaLight, point, surfaceSample);
  }

  TEST(GpuCompiledLightSampler, SelectsCompiledLightsLikeRuntimeLightSampler) {
    auto pointLight = std::make_shared<PointLight>(Vector3d(1.0, 2.0, 3.0), Colord(2.0, 3.0, 4.0));
    auto directionalLight =
      std::make_shared<DirectionalLight>(Vector3d(0.0, 1.0, 0.0), Colord(1.0, 1.5, 2.0));
    auto areaLight =
      std::make_shared<RectangularAreaLight>(Vector3d(0.0, 4.0, 0.0), Vector3d(2.0, 0.0, 0.0),
                                             Vector3d(0.0, 0.0, 3.0), Colord(0.25, 0.5, 0.75));

    Scene runtimeScene;
    runtimeScene.addLight(pointLight);
    runtimeScene.addLight(directionalLight);
    runtimeScene.addLight(areaLight);
    const LightSampler runtimeSampler(runtimeScene.lights());

    GpuTracingSceneSections compiledScene;
    compiledScene.lights.push_back(*makeGpuTracingLightRecord(*pointLight));
    compiledScene.lights.push_back(*makeGpuTracingLightRecord(*directionalLight));
    compiledScene.lights.push_back(*makeGpuTracingLightRecord(*areaLight));

    GpuDirectLightSelectionRecord selection;
    selection.lightBegin = 0u;
    selection.lightCount = static_cast<std::uint32_t>(compiledScene.lights.size());

    for (const double unitSample : {0.0, 0.3, 0.6, std::nextafter(1.0, 0.0)}) {
      const LightSampler::Selection runtime = runtimeSampler.select(unitSample);
      const GpuCompiledLightSelection compiled =
        selectGpuCompiledLight(compiledScene, selection, unitSample);

      ASSERT_TRUE(compiled.valid);
      EXPECT_EQ(runtime.lightIndex, compiled.lightIndex);
      EXPECT_NEAR(runtime.pdf, compiled.pdf, 1e-12);
    }
  }

  TEST(GpuCompiledLightSampler, UnsupportedCompiledLightReturnsExplicitFallbackStatus) {
    GpuTracingLightRecord unsupported;
    unsupported.kind = static_cast<std::uint32_t>(GpuTracingLightKind::Unsupported);
    unsupported.parameters = {9.0f, 9.0f, 9.0f, 1.0f};

    const GpuCompiledLightSample sample =
      sampleGpuCompiledLight(unsupported, Vector3d(0.0, 0.0, 0.0), Vector2d(0.25, 0.75));

    EXPECT_FALSE(sample.valid());
    EXPECT_EQ(GpuCompiledLightSampleStatus::UnsupportedLight, sample.status);
    EXPECT_DOUBLE_EQ(0.0, sample.pdf);
    EXPECT_DOUBLE_EQ(0.0, gpuCompiledLightSelectionWeight(unsupported));
  }

  TEST(GpuDirectLightCpuReference, ResolvesMatteContributionWithCurrentPathTracerEstimator) {
    const GpuTracingSceneSections scene = oneMattePointLightScene();
    const GpuDirectLightWorkRecord work = oneMattePointLightWork();
    const GpuDirectLightVisibilityRecord visibility =
      makeGpuDirectLightCpuVisibilityRecord(scene, work, /*workIndex=*/0u);

    const GpuDirectLightContributionRecord contribution =
      makeGpuDirectLightCpuContributionRecord(scene, work, visibility);

    EXPECT_EQ(gpuDirectLightContributionValid | gpuDirectLightContributionContributing,
              contribution.flags);
    EXPECT_EQ(0u, contribution.occluded);
    EXPECT_FLOAT_EQ(static_cast<float>(0.25 * (0.5 * 0.8 * invPI) * 2.0),
                    contribution.contribution[0]);
    EXPECT_FLOAT_EQ(static_cast<float>(0.5 * (0.25 * 0.8 * invPI) * 3.0),
                    contribution.contribution[1]);
    EXPECT_FLOAT_EQ(static_cast<float>(1.0 * (1.0 * 0.8 * invPI) * 4.0),
                    contribution.contribution[2]);
    EXPECT_FLOAT_EQ(1.0f, contribution.contribution[3]);
  }

  TEST(GpuDirectLightCpuReference, NonDeltaContributionUsesPowerHeuristicMis) {
    const GpuTracingSceneSections scene = oneMattePointLightScene();
    GpuDirectLightWorkRecord work = oneMattePointLightWork();
    work.surface.throughput = {0.5f, 0.25f, 1.0f, 1.0f};

    const double normalDotOut = 0.6;
    GpuDirectLightVisibilityRecord visibility;
    visibility.workIndex = 5u;
    visibility.lightIndex = 3u;
    visibility.flags = gpuDirectLightVisibilityValid;
    visibility.rayDirection = {0.8f, static_cast<float>(normalDotOut), 0.0f, 0.0f};
    visibility.lightRadiance = {3.0f, 2.0f, 1.0f, 1.0f};
    visibility.lightPdf = 0.75f;
    visibility.selectionPdf = 0.25f;

    const GpuDirectLightContributionRecord contribution =
      makeGpuDirectLightCpuContributionRecord(scene, work, visibility);

    const double bsdfPdf = normalDotOut * invPI;
    const double lightPdfSquared = visibility.lightPdf * visibility.lightPdf;
    const double bsdfPdfSquared = bsdfPdf * bsdfPdf;
    const double misWeight = lightPdfSquared / (lightPdfSquared + bsdfPdfSquared);
    const double estimatorScale = normalDotOut * misWeight / visibility.lightPdf;
    const Colord bsdfValue(0.5 * 0.8 * invPI, 0.25 * 0.8 * invPI, 1.0 * 0.8 * invPI);
    const Colord expected = bsdfValue * Colord(3.0, 2.0, 1.0) * estimatorScale /
                            visibility.selectionPdf * Colord(0.5, 0.25, 1.0);

    EXPECT_EQ(5u, contribution.workIndex);
    EXPECT_EQ(3u, contribution.lightIndex);
    EXPECT_EQ(gpuDirectLightContributionValid | gpuDirectLightContributionContributing,
              contribution.flags);
    EXPECT_NEAR(expected.r(), contribution.contribution[0], 1e-7);
    EXPECT_NEAR(expected.g(), contribution.contribution[1], 1e-7);
    EXPECT_NEAR(expected.b(), contribution.contribution[2], 1e-7);
    EXPECT_FLOAT_EQ(1.0f, contribution.contribution[3]);
  }

  TEST(GpuDirectLightCpuReference, InvalidLightPdfDoesNotContribute) {
    const GpuTracingSceneSections scene = oneMattePointLightScene();
    const GpuDirectLightWorkRecord work = oneMattePointLightWork();
    GpuDirectLightVisibilityRecord visibility =
      makeGpuDirectLightCpuVisibilityRecord(scene, work, /*workIndex=*/0u);
    visibility.flags = gpuDirectLightVisibilityValid;
    visibility.lightPdf = 0.0f;

    const GpuDirectLightContributionRecord contribution =
      makeGpuDirectLightCpuContributionRecord(scene, work, visibility);

    EXPECT_EQ(gpuDirectLightContributionValid, contribution.flags);
    EXPECT_EQ(0u, contribution.flags & gpuDirectLightContributionContributing);
    EXPECT_FLOAT_EQ(0.0f, contribution.contribution[0]);
    EXPECT_FLOAT_EQ(0.0f, contribution.contribution[1]);
    EXPECT_FLOAT_EQ(0.0f, contribution.contribution[2]);
  }

  TEST(GpuDirectLightCpuReference, OccludedVisibilityResolvesToZeroContribution) {
    const GpuTracingSceneSections scene = oneMattePointLightScene();
    const GpuDirectLightWorkRecord work = oneMattePointLightWork();
    GpuDirectLightVisibilityRecord visibility =
      makeGpuDirectLightCpuVisibilityRecord(scene, work, /*workIndex=*/0u);
    visibility.occluded = 1u;

    const GpuDirectLightContributionRecord contribution =
      makeGpuDirectLightCpuContributionRecord(scene, work, visibility);

    EXPECT_EQ(gpuDirectLightContributionValid | gpuDirectLightContributionOccluded,
              contribution.flags);
    EXPECT_EQ(1u, contribution.occluded);
    EXPECT_FLOAT_EQ(0.0f, contribution.contribution[0]);
    EXPECT_FLOAT_EQ(0.0f, contribution.contribution[1]);
    EXPECT_FLOAT_EQ(0.0f, contribution.contribution[2]);
  }

  TEST(GpuDirectLightCpuReference, ResolvesVisibilityOcclusionThroughIntersectionService) {
    const GpuTracingSceneSections scene = oneMattePointLightScene();
    const GpuDirectLightWorkRecord work = oneMattePointLightWork();
    const std::vector<GpuDirectLightVisibilityRecord> visibility = {
      makeGpuDirectLightCpuVisibilityRecord(scene, work, /*workIndex=*/0u)};

    Scene runtimeScene;
    runtimeScene.add(std::make_shared<Sphere>(Vector3d(0.0, 2.0, 0.0), 0.5));
    IntersectionService intersectionService(runtimeScene,
                                            WavefrontIntersectionBackendChoice::cpu());

    const std::vector<GpuDirectLightVisibilityRecord> resolved =
      resolveGpuDirectLightCpuVisibilityOcclusionBatch(intersectionService, visibility);

    ASSERT_EQ(1u, resolved.size());
    EXPECT_EQ(1u, resolved[0].occluded);
    EXPECT_EQ("runtime_scene", intersectionService.diagnostics().lastAnyHitTiming.executionPath);

    const GpuDirectLightContributionRecord contribution =
      makeGpuDirectLightCpuContributionRecord(scene, work, resolved[0]);
    EXPECT_NE(0u, contribution.flags & gpuDirectLightContributionOccluded);
    EXPECT_EQ(0u, contribution.flags & gpuDirectLightContributionContributing);
  }

  TEST(GpuDirectLightCpuReference, AreaLightVisibilityUsesGpuSampleDimension) {
    GpuTracingSceneSections scene = oneMattePointLightScene();
    GpuTracingLightRecord area;
    area.kind = static_cast<std::uint32_t>(GpuTracingLightKind::RectangularArea);
    area.positionOrDirection = {0.0f, 4.0f, 0.0f, 1.0f};
    area.u = {2.0f, 0.0f, 0.0f, 0.0f};
    area.v = {0.0f, 0.0f, 2.0f, 0.0f};
    area.parameters = {1.0f, 1.0f, 1.0f, 1.0f};
    scene.lights = {area};
    GpuDirectLightWorkRecord work = oneMattePointLightWork();

    const GpuDirectLightVisibilityRecord visibility =
      makeGpuDirectLightCpuVisibilityRecord(scene, work, /*workIndex=*/0u);
    const std::uint32_t dimension = static_cast<std::uint32_t>(gpuDirectLightSurfaceSampleDimension(
      work.sample.bounce, /*lightIndex=*/0u, work.sample.directSampleIndex));
    const Vector2d expectedSample = GpuSampleStream::sample2D(
      work.sample.seed, work.sample.pixelIndex, work.sample.primarySampleIndex, dimension);

    EXPECT_EQ(gpuDirectLightVisibilityValid, visibility.flags);
    EXPECT_FLOAT_EQ(static_cast<float>(expectedSample.x()), visibility.lightSample[0]);
    EXPECT_FLOAT_EQ(static_cast<float>(expectedSample.y()), visibility.lightSample[1]);
    EXPECT_GT(visibility.lightPdf, 0.0f);
  }

  TEST(GpuDirectLightCpuReference, BatchApiKeepsOneOutputRecordPerWorkRecord) {
    const GpuTracingSceneSections scene = oneMattePointLightScene();
    std::vector<GpuDirectLightWorkRecord> work = {oneMattePointLightWork(),
                                                  oneMattePointLightWork()};
    work[1].surface.normal = {0.0f, -1.0f, 0.0f, 0.0f};

    const GpuDirectLightCpuReferenceBatch batch = makeGpuDirectLightCpuReferenceBatch(scene, work);

    ASSERT_EQ(2u, batch.visibility.size());
    ASSERT_EQ(2u, batch.contributions.size());
    EXPECT_NE(0u, batch.visibility[0].flags & gpuDirectLightVisibilityValid);
    EXPECT_EQ(0u, batch.visibility[1].flags & gpuDirectLightVisibilityValid);
    EXPECT_NE(0u, batch.contributions[0].flags & gpuDirectLightContributionContributing);
    EXPECT_EQ(0u, batch.contributions[1].flags & gpuDirectLightContributionContributing);
  }

  TEST(GpuDirectLightCpuReference, ReferenceBatchCanConsumeIntersectionServiceOcclusionFlags) {
    const GpuTracingSceneSections scene = oneMattePointLightScene();
    std::vector<GpuDirectLightWorkRecord> work = {oneMattePointLightWork(),
                                                  oneMattePointLightWork()};
    work[1].surface.point = {2.0f, 0.0f, 0.0f, 1.0f};

    Scene runtimeScene;
    runtimeScene.add(std::make_shared<Sphere>(Vector3d(0.0, 2.0, 0.0), 0.5));
    IntersectionService intersectionService(runtimeScene,
                                            WavefrontIntersectionBackendChoice::cpu());

    const GpuDirectLightCpuReferenceBatch batch =
      makeGpuDirectLightCpuReferenceBatch(scene, work, intersectionService);

    ASSERT_EQ(2u, batch.visibility.size());
    ASSERT_EQ(2u, batch.contributions.size());
    EXPECT_EQ(1u, batch.visibility[0].occluded);
    EXPECT_EQ(0u, batch.visibility[1].occluded);
    EXPECT_NE(0u, batch.contributions[0].flags & gpuDirectLightContributionOccluded);
    EXPECT_NE(0u, batch.contributions[1].flags & gpuDirectLightContributionContributing);
  }
}
