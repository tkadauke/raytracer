#include <gtest/gtest.h>

#include "render/Integrator.h"
#include "render/GpuIntersectionScene.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Triangle.h"

#include "test/helpers/ColorTestHelper.h"

namespace IntegratorTest {
  using namespace render;

  namespace {
    class FixedRayCaster final : public RayCaster {
    public:
      Colord rayColor(const Rayd&, State& state) const override {
        state.numRays += 10;
        return Colord(0.25, 0.5, 0.75);
      }
    };

    class RecursiveProbeIntegrator final : public Integrator {
    public:
      std::unique_ptr<Integrator> clone() const override {
        return std::make_unique<RecursiveProbeIntegrator>();
      }

      Colord radiance(const Scene& scene, const Rayd& ray, State& state,
                      const RayCaster& recursiveRayCaster) const override {
        state.recurseIn();
        const Colord recursiveColor = recursiveRayCaster.rayColor(ray, state);
        state.recurseOut();
        return recursiveColor + scene.background();
      }
    };

    class SplitEstimateIntegrator final : public Integrator {
    public:
      std::unique_ptr<Integrator> clone() const override {
        return std::make_unique<SplitEstimateIntegrator>();
      }

      std::uint64_t estimatedClosestHitRaysPerPrimarySample() const override {
        return 3;
      }

      std::uint64_t estimatedAnyHitRaysPerPrimarySample() const override {
        return 5;
      }

      Colord radiance(const Scene&, const Rayd&, State&, const RayCaster&) const override {
        return Colord::black();
      }
    };
  }

  TEST(Integrator, ContractCarriesSceneRayStateAndRecursiveRayCaster) {
    Scene scene;
    scene.setBackground(Colord(0.1, 0.2, 0.3));
    State state;
    FixedRayCaster rayCaster;
    RecursiveProbeIntegrator integrator;

    const Colord color =
      integrator.radiance(scene, Rayd(Vector3d::null, Vector3d::forward()), state, rayCaster);

    EXPECT_EQ(0, state.recursionDepth);
    EXPECT_EQ(11, state.numRays);
    EXPECT_EQ(1, state.maxRecursionDepth);
    ASSERT_COLOR_NEAR(Colord(0.35, 0.7, 1.05), color, 1e-12);
  }

  TEST(Integrator, DefaultExpectedRayEstimateIsDerivedFromQueryFamilies) {
    RecursiveProbeIntegrator defaultIntegrator;
    EXPECT_EQ(1u, defaultIntegrator.estimatedClosestHitRaysPerPrimarySample());
    EXPECT_EQ(0u, defaultIntegrator.estimatedAnyHitRaysPerPrimarySample());
    EXPECT_EQ(1u, defaultIntegrator.estimatedIntersectionRaysPerPrimarySample());

    SplitEstimateIntegrator splitIntegrator;
    EXPECT_EQ(3u, splitIntegrator.estimatedClosestHitRaysPerPrimarySample());
    EXPECT_EQ(5u, splitIntegrator.estimatedAnyHitRaysPerPrimarySample());
    EXPECT_EQ(8u, splitIntegrator.estimatedIntersectionRaysPerPrimarySample());
  }

  TEST(Integrator, BatchedRadianceReportsScalarSampleDepthWork) {
    Scene scene;
    scene.setBackground(Colord(0.1, 0.2, 0.3));
    FixedRayCaster rayCaster;
    RecursiveProbeIntegrator integrator;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr},
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::right()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics);

    ASSERT_EQ(2u, colors.size());
    EXPECT_TRUE(metrics.usedScalarFallback);
    EXPECT_EQ((std::vector<std::uint64_t>{2u}), metrics.activeSamplesPerDepth);
    EXPECT_TRUE(metrics.frontierPacketChunksPerDepth.empty());
    EXPECT_TRUE(metrics.frontierPacketRaysPerDepth.empty());
    EXPECT_TRUE(metrics.frontierClosestHitBatchChunksPerDepth.empty());
    EXPECT_TRUE(metrics.frontierClosestHitBatchRaysPerDepth.empty());
    EXPECT_TRUE(metrics.directLightAnyHitBatchChunksPerDepth.empty());
    EXPECT_TRUE(metrics.directLightAnyHitBatchRaysPerDepth.empty());
    EXPECT_TRUE(metrics.frontierRay4PacketChunksPerDepth.empty());
    EXPECT_TRUE(metrics.frontierRay8PacketChunksPerDepth.empty());
    EXPECT_TRUE(metrics.frontierScalarRaysPerDepth.empty());
    EXPECT_TRUE(metrics.frontierPacketScalarFallbackRaysPerDepth.empty());
    EXPECT_TRUE(metrics.frontierPacketScalarFallbackRaysByReason.empty());
    EXPECT_TRUE(metrics.frontierPacketRefinedRaysPerDepth.empty());
    EXPECT_TRUE(metrics.frontierPacketRefinedRaysByMaterial.empty());
    EXPECT_TRUE(metrics.intersectionSceneUnsupportedReasons.empty());
    EXPECT_EQ(0u, metrics.intersectionBackendClosestHitFrontierPackedRayBytes);
    EXPECT_EQ(0u, metrics.intersectionBackendAnyHitFrontierPackedRayBytes);
    EXPECT_EQ(0u, metrics.intersectionBackendClosestHitFrontierHostQueryBytes);
    EXPECT_EQ(0u, metrics.intersectionBackendAnyHitFrontierHostQueryBytes);
    EXPECT_EQ(0u, metrics.intersectionBackendClosestHitFrontierStateHandleBytes);
    EXPECT_EQ(0u, metrics.intersectionBackendAnyHitFrontierStateHandleBytes);
    EXPECT_EQ(2u, metrics.activeSampleDepthsProcessed);
    EXPECT_DOUBLE_EQ(0.0, metrics.frontierPartitionWorkerSeconds);
  }

  TEST(Integrator, FrontierResidencyMetricsTrackFrontierPayloadBytes) {
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);

    metrics.recordClosestHitFrontierResidency("packed_host", 64, 96, 8);
    metrics.recordClosestHitFrontierResidency("packed_host", 128, 160, 16);
    metrics.recordAnyHitFrontierResidency("host", 0, 48, 0);
    metrics.recordAnyHitFrontierResidency("packed_host", 32, 64, 8);

    EXPECT_EQ("packed_host", metrics.intersectionBackendClosestHitFrontierResidency);
    EXPECT_EQ("mixed", metrics.intersectionBackendAnyHitFrontierResidency);
    EXPECT_EQ(192u, metrics.intersectionBackendClosestHitFrontierPackedRayBytes);
    EXPECT_EQ(32u, metrics.intersectionBackendAnyHitFrontierPackedRayBytes);
    EXPECT_EQ(256u, metrics.intersectionBackendClosestHitFrontierHostQueryBytes);
    EXPECT_EQ(112u, metrics.intersectionBackendAnyHitFrontierHostQueryBytes);
    EXPECT_EQ(24u, metrics.intersectionBackendClosestHitFrontierStateHandleBytes);
    EXPECT_EQ(8u, metrics.intersectionBackendAnyHitFrontierStateHandleBytes);
  }

  TEST(Integrator, IntersectionBackendMetricsTrackQuerySpecificExecutionPaths) {
    auto triangle =
      std::make_shared<Triangle>(Vector3d(-1, -1, 0), Vector3d(1, -1, 0), Vector3d(0, 1, 0));
    Scene scene;
    scene.add(triangle);
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);

    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);
    WavefrontIntersectionQueryTiming closestTiming;
    closestTiming.uploadSeconds = 0.001;
    closestTiming.kernelSeconds = 0.002;
    closestTiming.readbackSeconds = 0.003;
    closestTiming.recordExecutionPath("packed_cpu");
    closestTiming.recordFallbackReason("platform closest-hit failed");
    metrics.recordClosestHitQuery(*backend, 4, closestTiming);

    EXPECT_EQ("gpu", metrics.intersectionBackendRequest);
    EXPECT_EQ("cpu", metrics.intersectionBackend);
    EXPECT_EQ(backend->platformName(), metrics.intersectionBackendPlatform);
    EXPECT_EQ("fallback", metrics.intersectionBackendAvailability);
    EXPECT_EQ("platform closest-hit failed", metrics.intersectionBackendFallbackReason);
    EXPECT_EQ("packed_cpu", metrics.intersectionBackendExecutionPath);
    EXPECT_EQ("packed_cpu", metrics.intersectionBackendClosestHitExecutionPath);
    EXPECT_TRUE(metrics.intersectionBackendAnyHitExecutionPath.empty());
    EXPECT_EQ(backend->platformGpuDeviceAvailable(),
              metrics.intersectionBackendPlatformGpuDeviceAvailable);
    EXPECT_EQ(backend->platformGpuRenderPathAvailable(),
              metrics.intersectionBackendPlatformGpuRenderPathAvailable);
    EXPECT_EQ(4u, metrics.intersectionRaysSubmitted);
    EXPECT_EQ(4u, metrics.closestHitRaysSubmitted);
    EXPECT_EQ(0u, metrics.anyHitRaysSubmitted);
    EXPECT_EQ(1u, metrics.closestHitQueries);
    EXPECT_EQ(0u, metrics.anyHitQueries);
    EXPECT_TRUE(metrics.intersectionBackendPrefersClosestHitBatch);
    EXPECT_FALSE(metrics.intersectionBackendPrefersAnyHitBatch);
    EXPECT_FALSE(metrics.intersectionBackendSupportsResidentFrontiers);
    EXPECT_FALSE(metrics.intersectionBackendSupportsGpuFrontierCompaction);
    EXPECT_EQ(backend->gpuFrontierCompactionUnavailableReason(),
              metrics.intersectionBackendGpuFrontierCompactionUnavailableReason);
    EXPECT_FALSE(metrics.intersectionBackendSupportsPreparedRayBatchCompaction);
    EXPECT_FALSE(metrics.intersectionBackendSupportsResidentDirectLightBatches);
    EXPECT_EQ(backend->estimatedClosestHitRayUploadBytes(4),
              metrics.intersectionEstimatedRayUploadBytes);
    EXPECT_EQ(backend->estimatedClosestHitRayUploadBytes(4),
              metrics.intersectionEstimatedClosestHitRayUploadBytes);
    EXPECT_EQ(0u, metrics.intersectionEstimatedAnyHitRayUploadBytes);
    EXPECT_EQ(backend->estimatedClosestHitReadbackBytes(4),
              metrics.intersectionEstimatedClosestHitReadbackBytes);
    EXPECT_EQ(0u, metrics.intersectionEstimatedAnyHitReadbackBytes);
    EXPECT_EQ(backend->estimatedClosestHitRayUploadBytes(4) +
                backend->estimatedClosestHitReadbackBytes(4),
              metrics.intersectionEstimatedQueryTransferBytes);
    EXPECT_EQ(metrics.intersectionEstimatedQueryTransferBytes,
              metrics.intersectionEstimatedClosestHitQueryTransferBytes);
    EXPECT_EQ(0u, metrics.intersectionEstimatedAnyHitQueryTransferBytes);
    EXPECT_EQ(1u, metrics.intersectionEstimatedQueryRoundTrips);
    EXPECT_EQ(1u, metrics.intersectionEstimatedClosestHitQueryRoundTrips);
    EXPECT_EQ(0u, metrics.intersectionEstimatedAnyHitQueryRoundTrips);
    EXPECT_DOUBLE_EQ(0.001, metrics.intersectionBackendUploadWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.002, metrics.intersectionBackendKernelWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.003, metrics.intersectionBackendReadbackWorkerSeconds);
    EXPECT_TRUE(metrics.intersectionSceneTriangleClosestHitEligible);
    EXPECT_TRUE(metrics.intersectionSceneBasicHitEligible);
    EXPECT_TRUE(metrics.intersectionScenePackedClosestHitEligible);
    EXPECT_TRUE(metrics.intersectionScenePackedAnyHitEligible);

    WavefrontIntersectionQueryTiming anyTiming;
    anyTiming.uploadSeconds = 0.004;
    anyTiming.kernelSeconds = 0.005;
    anyTiming.readbackSeconds = 0.006;
    anyTiming.recordExecutionPath("compiled_cpu");
    anyTiming.recordFallbackReason("platform any-hit failed");
    metrics.recordAnyHitQuery(*backend, 1, anyTiming);

    EXPECT_EQ("mixed", metrics.intersectionBackendExecutionPath);
    EXPECT_EQ("packed_cpu", metrics.intersectionBackendClosestHitExecutionPath);
    EXPECT_EQ("compiled_cpu", metrics.intersectionBackendAnyHitExecutionPath);
    EXPECT_EQ("mixed", metrics.intersectionBackendFallbackReason);
    EXPECT_EQ(5u, metrics.intersectionRaysSubmitted);
    EXPECT_EQ(4u, metrics.closestHitRaysSubmitted);
    EXPECT_EQ(1u, metrics.anyHitRaysSubmitted);
    EXPECT_EQ(1u, metrics.closestHitQueries);
    EXPECT_EQ(1u, metrics.anyHitQueries);
    EXPECT_TRUE(metrics.intersectionBackendPrefersClosestHitBatch);
    EXPECT_TRUE(metrics.intersectionBackendPrefersAnyHitBatch);
    EXPECT_EQ(backend->supportsResidentFrontiers(),
              metrics.intersectionBackendSupportsResidentFrontiers);
    EXPECT_EQ(backend->supportsGpuFrontierCompaction(),
              metrics.intersectionBackendSupportsGpuFrontierCompaction);
    EXPECT_EQ(backend->gpuFrontierCompactionUnavailableReason(),
              metrics.intersectionBackendGpuFrontierCompactionUnavailableReason);
    EXPECT_EQ(backend->supportsPreparedRayBatchCompaction(),
              metrics.intersectionBackendSupportsPreparedRayBatchCompaction);
    EXPECT_EQ(backend->supportsResidentDirectLightBatches(),
              metrics.intersectionBackendSupportsResidentDirectLightBatches);
    EXPECT_EQ(backend->estimatedClosestHitRayUploadBytes(4) +
                backend->estimatedAnyHitRayUploadBytes(1),
              metrics.intersectionEstimatedRayUploadBytes);
    EXPECT_EQ(backend->estimatedClosestHitRayUploadBytes(4),
              metrics.intersectionEstimatedClosestHitRayUploadBytes);
    EXPECT_EQ(backend->estimatedAnyHitRayUploadBytes(1),
              metrics.intersectionEstimatedAnyHitRayUploadBytes);
    EXPECT_EQ(backend->estimatedClosestHitReadbackBytes(4),
              metrics.intersectionEstimatedClosestHitReadbackBytes);
    EXPECT_EQ(backend->estimatedAnyHitReadbackBytes(1),
              metrics.intersectionEstimatedAnyHitReadbackBytes);
    EXPECT_EQ(
      backend->estimatedClosestHitRayUploadBytes(4) + backend->estimatedClosestHitReadbackBytes(4) +
        backend->estimatedAnyHitRayUploadBytes(1) + backend->estimatedAnyHitReadbackBytes(1),
      metrics.intersectionEstimatedQueryTransferBytes);
    EXPECT_EQ(backend->estimatedClosestHitRayUploadBytes(4) +
                backend->estimatedClosestHitReadbackBytes(4),
              metrics.intersectionEstimatedClosestHitQueryTransferBytes);
    EXPECT_EQ(backend->estimatedAnyHitRayUploadBytes(1) + backend->estimatedAnyHitReadbackBytes(1),
              metrics.intersectionEstimatedAnyHitQueryTransferBytes);
    EXPECT_EQ(2u, metrics.intersectionEstimatedQueryRoundTrips);
    EXPECT_EQ(1u, metrics.intersectionEstimatedClosestHitQueryRoundTrips);
    EXPECT_EQ(1u, metrics.intersectionEstimatedAnyHitQueryRoundTrips);
    EXPECT_DOUBLE_EQ(0.005, metrics.intersectionBackendUploadWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.007, metrics.intersectionBackendKernelWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.009, metrics.intersectionBackendReadbackWorkerSeconds);
  }

  TEST(Integrator, BatchMetricsReportMixedQueryDepths) {
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);

    metrics.recordFrontierClosestHitBatch(1, 4);
    metrics.recordFrontierClosestHitBatch(1, 8);
    metrics.recordDirectLightAnyHitBatch(0, 1, 3);
    metrics.recordDirectLightAnyHitBatch(2, 1, 5);

    EXPECT_TRUE(metrics.hasMixedQueryDepth(0));
    EXPECT_FALSE(metrics.hasMixedQueryDepth(1));
    EXPECT_FALSE(metrics.hasMixedQueryDepth(2));
    EXPECT_EQ(4u, metrics.frontierQueryRoundTrips());
    EXPECT_EQ(3u, metrics.residentFrontierQueryRoundTripsEstimate());
    EXPECT_EQ(1u, metrics.residentFrontierQueryRoundTripSavingsEstimate());
    EXPECT_EQ(1u, metrics.mixedQueryDepthCount());
    EXPECT_EQ(2u, metrics.mixedQueryDepthRoundTrips());
    EXPECT_EQ(7u, metrics.mixedQueryDepthRays());
    EXPECT_EQ(4u, metrics.mixedQueryDepthClosestHitRays());
    EXPECT_EQ(3u, metrics.mixedQueryDepthAnyHitRays());
  }

  TEST(Integrator, BatchMetricsReportCompactionCandidates) {
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);

    metrics.recordActiveDepth(4);
    metrics.recordActiveDepth(3);
    metrics.recordActiveDepth(7);
    metrics.recordRetainedActiveDepth(1);
    metrics.recordRetainedActiveDepth(3);
    metrics.recordRetainedActiveDepth(2);

    EXPECT_TRUE(metrics.hasCompactionCandidateDepth(0));
    EXPECT_FALSE(metrics.hasCompactionCandidateDepth(1));
    EXPECT_TRUE(metrics.hasCompactionCandidateDepth(2));
    EXPECT_EQ(3u, metrics.compactionCandidateSamplesAtDepth(0));
    EXPECT_EQ(0u, metrics.compactionCandidateSamplesAtDepth(1));
    EXPECT_EQ(5u, metrics.compactionCandidateSamplesAtDepth(2));
    EXPECT_EQ(2u, metrics.compactionCandidateDepthCount());
    EXPECT_EQ(8u, metrics.compactionCandidateSampleCount());
    EXPECT_EQ(8u * sizeof(GpuIntersectionRay), metrics.compactionCandidatePackedRayBytes());
    EXPECT_EQ(8u * sizeof(State*), metrics.compactionCandidateStateHandleBytes());
    EXPECT_DOUBLE_EQ(8.0 / 14.0, metrics.compactionCandidateSampleFraction());
    EXPECT_EQ(2u, metrics.largestCompactionCandidateDepth());
    EXPECT_EQ(5u, metrics.largestCompactionCandidateSampleCount());
    EXPECT_EQ(5u * sizeof(GpuIntersectionRay), metrics.largestCompactionCandidatePackedRayBytes());
    EXPECT_EQ(5u * sizeof(State*), metrics.largestCompactionCandidateStateHandleBytes());
    EXPECT_DOUBLE_EQ(5.0 / 7.0, metrics.largestCompactionCandidateSampleFraction());
  }

  TEST(Integrator, BatchMetricsReportFrontierCompaction) {
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);

    metrics.recordFrontierCompaction(/*inputSamples=*/3, /*retainedSamples=*/2,
                                     /*movedSamples=*/1, "vulkan",
                                     /*retainedIndexBytes=*/2u * sizeof(std::uint32_t));
    metrics.recordHostFrontierCompaction(/*inputSamples=*/10, /*retainedSamples=*/6,
                                         /*movedSamples=*/4);
    metrics.recordHostFrontierCompaction(/*inputSamples=*/4, /*retainedSamples=*/4,
                                         /*movedSamples=*/0);

    EXPECT_EQ(3u, metrics.frontierCompactionPasses);
    EXPECT_EQ(17u, metrics.frontierCompactionInputSamples);
    EXPECT_EQ(12u, metrics.frontierCompactionRetainedSamples);
    EXPECT_EQ(5u, metrics.frontierCompactionRemovedSamples);
    EXPECT_EQ(5u, metrics.frontierCompactionMovedSamples);
    EXPECT_EQ(12u * sizeof(std::uint32_t), metrics.frontierCompactionRetainedIndexBytes);
    EXPECT_EQ("mixed", metrics.frontierCompactionExecutionPath);
    EXPECT_DOUBLE_EQ(5.0 / 17.0, metrics.frontierCompactionRemovedSampleFraction());
    EXPECT_DOUBLE_EQ(5.0 / 12.0, metrics.frontierCompactionMovedRetainedSampleFraction());
  }
}
