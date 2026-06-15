#include <gtest/gtest.h>

#include "render/TracingPathStateBuffer.h"
#include "test/helpers/ColorTestHelper.h"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace TracingPathStateBufferTest {
  using namespace render;

  TEST(TracingPathStateLayout, DefinesActiveAndNextGpuPathStateBuffers) {
    const TracingPathStateLayout layout = TracingPathStateLayout::pathCapacity(128);

    EXPECT_EQ(128u, layout.capacity);
    EXPECT_EQ(TracingPathStateFormat::GpuPathStateRecordV1, layout.activeFormat);
    EXPECT_EQ(TracingPathStateFormat::GpuPathStateRecordV1, layout.nextFormat);
    EXPECT_TRUE(layout.hasActiveAndNextBuffers());
    EXPECT_EQ(sizeof(GpuPathStateRecord), layout.pathStateBytes());
    EXPECT_EQ(128u * sizeof(GpuPathStateRecord), layout.activeBytes());
    EXPECT_EQ(128u * sizeof(GpuPathStateRecord), layout.nextBytes());
    EXPECT_EQ(2u * 128u * sizeof(GpuPathStateRecord), layout.totalBytes());
  }

  TEST(TracingPathStateLayout, UsesStableGpuRecordAbi) {
    EXPECT_TRUE(std::is_standard_layout_v<GpuPathStateRecord>);
    EXPECT_EQ(16u, alignof(GpuPathStateRecord));
    EXPECT_EQ(0u, sizeof(GpuPathStateRecord) % 16u);
    EXPECT_EQ("gpu_path_state_record_v1",
              std::string(toString(TracingPathStateFormat::GpuPathStateRecordV1)));
  }

  TEST(TracingPathStateLayout, RejectsInvalidCapacityAndByteOverflow) {
    EXPECT_THROW(TracingPathStateLayout::pathCapacity(0), std::invalid_argument);

    TracingPathStateLayout layout;
    layout.capacity = std::numeric_limits<std::uint64_t>::max() / sizeof(GpuPathStateRecord) + 1u;
    EXPECT_THROW(layout.validate(), std::overflow_error);
  }

  TEST(TracingPathStateRecord, RoundTripsCpuReferenceRayAndPathStateFields) {
    const Rayd ray(Vector4d(1.0, 2.0, 3.0, 1.0), Vector3d(0.25, -0.5, 0.75));

    GpuPathStateRecord record = makeGpuPathStateRecord(
      ray, Colord(0.2, 0.4, 0.6), Colord(1.0, 1.5, 2.0), /*pixelIndex=*/17,
      /*sampleIndex=*/3, /*depth=*/2,
      flagValue(GpuPathStateFlags::Active) | flagValue(GpuPathStateFlags::SampledFromBsdf),
      /*bsdfSamplePdf=*/0.125, /*timeSample=*/0.5, /*animationFrame=*/12.0,
      /*animationTime=*/12.5, /*rngSeed=*/99);
    setFlag(record, GpuPathStateFlags::BackgroundVisible, true);
    setFlag(record, GpuPathStateFlags::SampledFromBsdf, false);

    const Rayd decodedRay = rayFromGpuPathStateRecord(record);
    EXPECT_DOUBLE_EQ(1.0, decodedRay.origin().x());
    EXPECT_DOUBLE_EQ(2.0, decodedRay.origin().y());
    EXPECT_DOUBLE_EQ(3.0, decodedRay.origin().z());
    EXPECT_DOUBLE_EQ(1.0, decodedRay.origin().w());
    EXPECT_DOUBLE_EQ(0.25, decodedRay.direction().x());
    EXPECT_DOUBLE_EQ(-0.5, decodedRay.direction().y());
    EXPECT_DOUBLE_EQ(0.75, decodedRay.direction().z());
    ASSERT_COLOR_NEAR(Colord(0.2, 0.4, 0.6), throughputFromGpuPathStateRecord(record), 1e-6);
    ASSERT_COLOR_NEAR(Colord(1.0, 1.5, 2.0), accumulatedRadianceFromGpuPathStateRecord(record),
                      1e-6);
    EXPECT_EQ(17u, record.pixelIndex);
    EXPECT_EQ(3u, record.sampleIndex);
    EXPECT_EQ(2u, record.depth);
    EXPECT_FLOAT_EQ(0.125f, record.continuation[0]);
    EXPECT_FLOAT_EQ(0.5f, record.continuation[1]);
    EXPECT_FLOAT_EQ(12.0f, record.continuation[2]);
    EXPECT_FLOAT_EQ(12.5f, record.continuation[3]);
    EXPECT_EQ(99u, record.rngSeed);
    EXPECT_TRUE(hasFlag(record, GpuPathStateFlags::Active));
    EXPECT_TRUE(hasFlag(record, GpuPathStateFlags::BackgroundVisible));
    EXPECT_FALSE(hasFlag(record, GpuPathStateFlags::SampledFromBsdf));
  }

  TEST(TracingPathStateBuffers, CpuReferenceExercisesActiveAndNextPingPongLayout) {
    TracingPathStateBuffers buffers(3);
    const Rayd ray(Vector4d(0.0, 0.0, 0.0, 1.0), Vector3d(0.0, 0.0, 1.0));

    buffers.appendActive(makeGpuPathStateRecord(ray, Colord::white(), Colord::black(), 0, 0, 0));
    buffers.appendNext(
      makeGpuPathStateRecord(ray, Colord(0.5, 0.5, 0.5), Colord(0.1, 0.2, 0.3), 0, 0, 1));
    buffers.appendNext(
      makeGpuPathStateRecord(ray, Colord(0.25, 0.25, 0.25), Colord::black(), 1, 0, 1));

    EXPECT_EQ(1u, buffers.active().size());
    EXPECT_EQ(2u, buffers.next().size());
    EXPECT_EQ(1u, buffers.diagnostics().activeCount);
    EXPECT_EQ(2u, buffers.diagnostics().nextCount);

    buffers.swapActiveAndNext();

    ASSERT_EQ(2u, buffers.active().size());
    EXPECT_TRUE(buffers.next().empty());
    ASSERT_COLOR_NEAR(Colord(0.5, 0.5, 0.5), throughputFromGpuPathStateRecord(buffers.active()[0]),
                      1e-6);
    EXPECT_EQ(2u, buffers.diagnostics().activeCount);
    EXPECT_EQ(0u, buffers.diagnostics().nextCount);
    EXPECT_EQ(1u, buffers.diagnostics().swapOperations);
    EXPECT_EQ(2u * 3u * sizeof(GpuPathStateRecord), buffers.diagnostics().residentBytes);
  }

  TEST(TracingPathStateBuffers, RejectsAppendingPastCapacity) {
    TracingPathStateBuffers buffers(1);
    const Rayd ray(Vector4d(0.0, 0.0, 0.0, 1.0), Vector3d(0.0, 0.0, 1.0));
    const GpuPathStateRecord record =
      makeGpuPathStateRecord(ray, Colord::white(), Colord::black(), 0, 0, 0);

    buffers.appendActive(record);
    EXPECT_THROW(buffers.appendActive(record), std::overflow_error);

    buffers.appendNext(record);
    EXPECT_THROW(buffers.appendNext(record), std::overflow_error);
  }

  TEST(ResidentPathCompactionContract, ReportsAllRetainedPathsWithoutMoves) {
    const ResidentPathCompactionContract contract =
      ResidentPathCompactionContract::fromRetainedIndices(
        /*inputPathCount=*/4, {0u, 1u, 2u, 3u}, "gpu_resident",
        /*pathStateBytesPerPath=*/sizeof(GpuPathStateRecord));

    EXPECT_EQ(4u, contract.inputPathCount());
    EXPECT_EQ(4u, contract.retainedPathCount());
    EXPECT_EQ(0u, contract.removedPathCount());
    EXPECT_EQ(0u, contract.movedPathCount());
    EXPECT_DOUBLE_EQ(0.0, contract.removedPathFraction());
    EXPECT_DOUBLE_EQ(0.0, contract.movedRetainedPathFraction());
    EXPECT_EQ((std::vector<std::uint32_t>{0u, 1u, 2u, 3u}), contract.retainedPathIndices());
    EXPECT_EQ("gpu_resident", contract.executionPath());
    EXPECT_EQ(4u * sizeof(std::uint32_t), contract.retainedIndexBytes());
    EXPECT_EQ(4u * sizeof(GpuPathStateRecord), contract.inputResidentPathStateBytes());
    EXPECT_EQ(4u * sizeof(GpuPathStateRecord), contract.retainedResidentPathStateBytes());
    EXPECT_EQ(0u, contract.removedResidentPathStateBytes());
  }

  TEST(ResidentPathCompactionContract, ReportsRemovedAndMovedRetainedPaths) {
    const ResidentPathCompactionContract contract =
      ResidentPathCompactionContract::fromRetainedIndices(
        /*inputPathCount=*/6, {0u, 2u, 5u}, "vulkan_resident_compaction",
        /*pathStateBytesPerPath=*/64u);

    EXPECT_EQ(6u, contract.inputPathCount());
    EXPECT_EQ(3u, contract.retainedPathCount());
    EXPECT_EQ(3u, contract.removedPathCount());
    EXPECT_EQ(2u, contract.movedPathCount());
    EXPECT_DOUBLE_EQ(3.0 / 6.0, contract.removedPathFraction());
    EXPECT_DOUBLE_EQ(2.0 / 3.0, contract.movedRetainedPathFraction());
    EXPECT_EQ(3u * sizeof(std::uint32_t), contract.retainedIndexBytes());
    EXPECT_EQ(6u * 64u, contract.inputResidentPathStateBytes());
    EXPECT_EQ(3u * 64u, contract.retainedResidentPathStateBytes());
    EXPECT_EQ(3u * 64u, contract.removedResidentPathStateBytes());
    EXPECT_EQ("vulkan_resident_compaction", contract.executionPath());
  }

  TEST(ResidentPathCompactionContract, ReportsAllRemovedPathsWithoutMoves) {
    const ResidentPathCompactionContract contract =
      ResidentPathCompactionContract::fromRetainedIndices(
        /*inputPathCount=*/3, {}, "gpu_resident", /*pathStateBytesPerPath=*/32u);

    EXPECT_EQ(0u, contract.retainedPathCount());
    EXPECT_EQ(3u, contract.removedPathCount());
    EXPECT_EQ(0u, contract.movedPathCount());
    EXPECT_DOUBLE_EQ(1.0, contract.removedPathFraction());
    EXPECT_DOUBLE_EQ(0.0, contract.movedRetainedPathFraction());
    EXPECT_EQ(0u, contract.retainedIndexBytes());
    EXPECT_EQ(96u, contract.inputResidentPathStateBytes());
    EXPECT_EQ(0u, contract.retainedResidentPathStateBytes());
    EXPECT_EQ(96u, contract.removedResidentPathStateBytes());
  }

  TEST(ResidentPathCompactionContract, RejectsInvalidRetainedIndices) {
    EXPECT_THROW(ResidentPathCompactionContract::fromRetainedIndices(3, {0u, 3u}, "gpu"),
                 std::out_of_range);
    EXPECT_THROW(ResidentPathCompactionContract::fromRetainedIndices(3, {1u, 1u}, "gpu"),
                 std::invalid_argument);
    EXPECT_THROW(ResidentPathCompactionContract::fromRetainedIndices(3, {2u, 1u}, "gpu"),
                 std::invalid_argument);
  }
}
