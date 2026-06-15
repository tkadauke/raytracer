#include <gtest/gtest.h>

#include "core/math/Constants.h"
#include "render/GpuDirectLightCpuReference.h"
#include "render/GpuDirectLightWork.h"
#include "render/GpuIntersectionScene.h"
#include "render/GpuTracingScene.h"
#include "render/samplers/GpuSampleStream.h"

#include <cmath>
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
}
