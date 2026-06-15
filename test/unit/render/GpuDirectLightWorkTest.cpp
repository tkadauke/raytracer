#include <gtest/gtest.h>

#include "render/GpuDirectLightWork.h"
#include "render/GpuIntersectionScene.h"

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
  }

  TEST(GpuDirectLightWork, RecordsHaveStableKernelFriendlyLayout) {
    expectKernelRecordLayout<GpuDirectLightSurfaceRecord>();
    expectKernelRecordLayout<GpuDirectLightSampleStateRecord>();
    expectKernelRecordLayout<GpuDirectLightSelectionRecord>();
    expectKernelRecordLayout<GpuDirectLightVisibilityRecord>();
    expectKernelRecordLayout<GpuDirectLightWorkRecord>();
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
}
