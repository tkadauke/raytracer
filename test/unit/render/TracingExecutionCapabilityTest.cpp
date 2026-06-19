#include <gtest/gtest.h>

#include "render/TracingExecutionCapability.h"

#include <algorithm>

namespace TracingExecutionCapabilityTest {
  using namespace render;

  TEST(TracingCapabilityRecord, RepresentsCpuHybridGpuUnsupportedAndFallbackStates) {
    const auto cpu =
      TracingCapabilityRecord::cpu(TracingExecutionDomain::BSDF, "shading.bsdf_eval");
    EXPECT_TRUE(cpu.supported());
    EXPECT_TRUE(cpu.usesCpu());
    EXPECT_FALSE(cpu.usesGpu());
    EXPECT_FALSE(cpu.fallsBack());

    const auto hybrid = TracingCapabilityRecord::hybrid(TracingExecutionDomain::PathState,
                                                        "state.frontier_compaction", "mixed");
    EXPECT_TRUE(hybrid.supported());
    EXPECT_TRUE(hybrid.usesCpu());
    EXPECT_TRUE(hybrid.usesGpu());

    const auto gpu = TracingCapabilityRecord::gpu(TracingExecutionDomain::Intersection,
                                                  "geometry.closest_hit", "vulkan", "vulkan");
    EXPECT_TRUE(gpu.supported());
    EXPECT_FALSE(gpu.usesCpu());
    EXPECT_TRUE(gpu.usesGpu());

    const auto unsupported = TracingCapabilityRecord::unsupported(
      TracingExecutionDomain::Sampling, "sampling.gpu_rng", "not implemented");
    EXPECT_FALSE(unsupported.supported());
    EXPECT_FALSE(unsupported.usesCpu());
    EXPECT_FALSE(unsupported.usesGpu());
    EXPECT_FALSE(unsupported.fallsBack());

    const auto fallback = TracingCapabilityRecord::fallbackRecord(
      TracingExecutionDomain::Intersection, "geometry.any_hit", TracingExecutionDevice::GPU,
      TracingExecutionDevice::CPU, "packed_cpu", "GPU backend unavailable");
    EXPECT_TRUE(fallback.supported());
    EXPECT_TRUE(fallback.usesCpu());
    EXPECT_FALSE(fallback.usesGpu());
    EXPECT_TRUE(fallback.fallsBack());
    EXPECT_TRUE(fallback.fallback.active);
    EXPECT_EQ(TracingExecutionDevice::GPU, fallback.fallback.requestedDevice);
    EXPECT_EQ(TracingExecutionDevice::CPU, fallback.fallback.resolvedDevice);
    EXPECT_EQ("GPU backend unavailable", fallback.fallback.reason);
  }

  TEST(TracingExecutionCapabilityRecords, FlattensAllCapabilityGroupsAndReportsFallbacks) {
    TracingExecutionCapabilityRecords records;
    records.intersection.closestHit =
      TracingCapabilityRecord::cpu(TracingExecutionDomain::Intersection, "geometry.closest_hit");
    records.intersection.anyHit = TracingCapabilityRecord::fallbackRecord(
      TracingExecutionDomain::Intersection, "geometry.any_hit", TracingExecutionDevice::GPU,
      TracingExecutionDevice::CPU, "packed_cpu", "scene unsupported");

    const auto flattened = records.flattened();

    EXPECT_EQ(20u, flattened.size());
    EXPECT_TRUE(records.hasFallback());
    EXPECT_NE(flattened.end(),
              std::find_if(flattened.begin(), flattened.end(), [](const auto& record) {
                return record.name == "geometry.any_hit" && record.fallsBack();
              }));
  }
}
