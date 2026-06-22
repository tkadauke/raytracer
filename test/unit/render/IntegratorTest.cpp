#include <gtest/gtest.h>

#include <optional>

#include "render/Integrator.h"
#include "render/GpuIntersectionScene.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/TracingPathStateBuffer.h"
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

    class RecordingDepthObserver final : public IntegratorBatchObserver {
    public:
      IntegratorBatchFeedback depthCompleted(std::uint64_t completedDepth,
                                             const std::vector<Colord>& sampleColors,
                                             std::uint64_t activeSamples) override {
        ++calls;
        lastCompletedDepth = completedDepth;
        lastSampleCount = sampleColors.size();
        lastActiveSamples = activeSamples;
        IntegratorBatchFeedback feedback;
        feedback.convergenceRadianceDeltaRms = convergenceRadianceDeltaRms;
        return feedback;
      }

      std::uint64_t calls{0};
      std::uint64_t lastCompletedDepth{0};
      std::size_t lastSampleCount{0};
      std::uint64_t lastActiveSamples{0};
      std::optional<double> convergenceRadianceDeltaRms;
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
    EXPECT_TRUE(metrics.directLightAnyHitFrontierPackedRayBytesPerDepth.empty());
    EXPECT_TRUE(metrics.directLightAnyHitFrontierHostQueryBytesPerDepth.empty());
    EXPECT_TRUE(metrics.directLightAnyHitFrontierStateHandleBytesPerDepth.empty());
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

  TEST(Integrator, BatchedRadiancePublishesScalarProgressThroughSharedLifecycle) {
    Scene scene;
    scene.setBackground(Colord(0.1, 0.2, 0.3));
    FixedRayCaster rayCaster;
    RecursiveProbeIntegrator integrator;
    RecordingDepthObserver observer;
    observer.convergenceRadianceDeltaRms = 0.01;
    IntegratorBatchSettings settings;
    settings.progressObserver = &observer;
    settings.convergenceEnabled = true;
    settings.activeSampleFractionThreshold = 1.0;
    settings.radianceDeltaRmsThreshold = 0.05;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr},
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::right()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics, settings);

    ASSERT_EQ(2u, colors.size());
    EXPECT_EQ(1u, observer.calls);
    EXPECT_EQ(1u, observer.lastCompletedDepth);
    EXPECT_EQ(2u, observer.lastSampleCount);
    EXPECT_EQ(2u, observer.lastActiveSamples);
    EXPECT_TRUE(metrics.stoppedByConvergence);
    EXPECT_EQ(1u, metrics.stoppedAfterDepth);
    EXPECT_EQ(1u, metrics.observerConvergenceFeedbackDepths);
  }

  TEST(IntegratorBatchSettings, DepthProgressUsesObserverConvergenceFeedback) {
    std::vector<Colord> colors{Colord(0.25, 0.5, 0.75), Colord(0.0, 0.0, 0.0)};
    RecordingDepthObserver observer;
    observer.convergenceRadianceDeltaRms = 0.01;
    IntegratorBatchSettings settings;
    settings.progressObserver = &observer;
    settings.convergenceEnabled = true;
    settings.activeSampleFractionThreshold = 0.5;
    settings.radianceDeltaRmsThreshold = 0.05;
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);
    metrics.recordActiveDepth(4);

    const bool stopped = settings.publishDepthProgressAndCheckConvergence(
      IntegratorBatchDepthProgress{/*completedDepth=*/3,
                                   /*sampleColors=*/&colors,
                                   /*retainedActiveSamples=*/2,
                                   /*totalSamples=*/4,
                                   /*activeSamplesAtDepth=*/4,
                                   /*radianceDeltaSquaredSum=*/100.0},
      &metrics);

    EXPECT_TRUE(stopped);
    EXPECT_EQ(1u, observer.calls);
    EXPECT_EQ(3u, observer.lastCompletedDepth);
    EXPECT_EQ(2u, observer.lastSampleCount);
    EXPECT_EQ(2u, observer.lastActiveSamples);
    EXPECT_TRUE(metrics.stoppedByConvergence);
    EXPECT_EQ(1u, metrics.stoppedAfterDepth);
    EXPECT_EQ(1u, metrics.observerConvergenceFeedbackDepths);
  }

  TEST(IntegratorBatchSettings, DepthProgressUsesRawRadianceDeltaWhenObserverHasNoFeedback) {
    std::vector<Colord> colors{Colord(0.25, 0.5, 0.75)};
    RecordingDepthObserver observer;
    IntegratorBatchSettings settings;
    settings.progressObserver = &observer;
    settings.convergenceEnabled = true;
    settings.activeSampleFractionThreshold = 0.5;
    settings.radianceDeltaRmsThreshold = 0.5;
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);
    metrics.recordActiveDepth(4);

    const bool stopped = settings.publishDepthProgressAndCheckConvergence(
      IntegratorBatchDepthProgress{/*completedDepth=*/1,
                                   /*sampleColors=*/&colors,
                                   /*retainedActiveSamples=*/1,
                                   /*totalSamples=*/4,
                                   /*activeSamplesAtDepth=*/4,
                                   /*radianceDeltaSquaredSum=*/4.0},
      &metrics);

    EXPECT_FALSE(stopped);
    EXPECT_EQ(1u, observer.calls);
    EXPECT_FALSE(metrics.stoppedByConvergence);
    EXPECT_EQ(0u, metrics.stoppedAfterDepth);
    EXPECT_EQ(0u, metrics.observerConvergenceFeedbackDepths);
  }

  TEST(IntegratorBatchMetrics, MergeFromAccumulatesHostByteDiagnostics) {
    IntegratorBatchMetrics target;
    target.reset(/*scalarFallback=*/false);
    target.recordActiveDepth(3);
    target.recordActiveHitHostBytes(7);
    target.recordDirectLightSelectionHostBytes(/*depth=*/0, /*bytes=*/2);
    target.recordDirectLightOcclusionHostBytes(/*depth=*/0, /*bytes=*/4);
    target.recordDirectLightContributionHostBytes(/*depth=*/0, /*bytes=*/6);
    target.recordDirectLightContributionExecution("cpu");
    target.recordDirectLightAnyHitBatch(/*depth=*/0, /*batchChunks=*/1, /*batchRays=*/3,
                                        /*packedRayBytes=*/30, /*hostQueryBytes=*/15,
                                        /*stateHandleBytes=*/9);
    target.recordRadianceDeltaDepth(/*squaredSum=*/4.0, /*maxDelta=*/2.0);

    IntegratorBatchMetrics source;
    source.reset(/*scalarFallback=*/true);
    source.recordActiveDepth(5);
    source.recordActiveHitHostBytes(11);
    source.recordDirectLightSelectionHostBytes(/*depth=*/0, /*bytes=*/3);
    source.recordDirectLightSelectionHostBytes(/*depth=*/1, /*bytes=*/5);
    source.recordDirectLightOcclusionHostBytes(/*depth=*/0, /*bytes=*/7);
    source.recordDirectLightOcclusionHostBytes(/*depth=*/1, /*bytes=*/13);
    source.recordDirectLightContributionHostBytes(/*depth=*/0, /*bytes=*/17);
    source.recordDirectLightContributionHostBytes(/*depth=*/1, /*bytes=*/19);
    source.recordDirectLightContributionExecution(
      "cpu", "GPU diffuse direct-light contribution kernel unavailable");
    source.recordDirectLightAnyHitBatch(/*depth=*/0, /*batchChunks=*/2, /*batchRays=*/5,
                                        /*packedRayBytes=*/50, /*hostQueryBytes=*/25,
                                        /*stateHandleBytes=*/15);
    source.recordDirectLightAnyHitBatch(/*depth=*/1, /*batchChunks=*/3, /*batchRays=*/7,
                                        /*packedRayBytes=*/70, /*hostQueryBytes=*/35,
                                        /*stateHandleBytes=*/21);
    source.recordRadianceDeltaDepth(/*squaredSum=*/9.0, /*maxDelta=*/1.0);

    target.mergeFrom(source);

    EXPECT_TRUE(target.usedScalarFallback);
    EXPECT_EQ((std::vector<std::uint64_t>{8u}), target.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{18u}), target.activeHitHostBytesPerDepth);
    EXPECT_EQ(18u, target.activeHitHostBytesProcessed);
    EXPECT_EQ((std::vector<std::uint64_t>{5u, 5u}), target.directLightSelectionHostBytesPerDepth);
    EXPECT_EQ(10u, target.directLightSelectionHostBytes);
    EXPECT_EQ((std::vector<std::uint64_t>{11u, 13u}), target.directLightOcclusionHostBytesPerDepth);
    EXPECT_EQ(24u, target.directLightOcclusionHostBytes);
    EXPECT_EQ((std::vector<std::uint64_t>{23u, 19u}),
              target.directLightContributionHostBytesPerDepth);
    EXPECT_EQ(42u, target.directLightContributionHostBytes);
    EXPECT_EQ("cpu", target.directLightContributionExecutionPath);
    EXPECT_EQ("GPU diffuse direct-light contribution kernel unavailable",
              target.directLightContributionFallbackReason);
    EXPECT_EQ((std::vector<std::uint64_t>{3u, 3u}), target.directLightAnyHitBatchChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{8u, 7u}), target.directLightAnyHitBatchRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{80u, 70u}),
              target.directLightAnyHitFrontierPackedRayBytesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{40u, 35u}),
              target.directLightAnyHitFrontierHostQueryBytesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{24u, 21u}),
              target.directLightAnyHitFrontierStateHandleBytesPerDepth);
    EXPECT_EQ(150u, target.directLightAnyHitFrontierPackedRayBytes);
    EXPECT_EQ(75u, target.directLightAnyHitFrontierHostQueryBytes);
    EXPECT_EQ(45u, target.directLightAnyHitFrontierStateHandleBytes);
    EXPECT_EQ((std::vector<double>{13.0}), target.radianceDeltaSquaredSumPerDepth);
    EXPECT_EQ((std::vector<double>{2.0}), target.maxRadianceDeltaPerDepth);
  }

  TEST(IntegratorBatchMetrics, DirectLightVisibilityDepthRecordsOneDepthRow) {
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);

    metrics.recordDirectLightVisibilityDepth(/*depth=*/2,
                                             /*selectionHostBytes=*/12,
                                             /*occlusionHostBytes=*/4,
                                             /*batchChunks=*/3,
                                             /*batchRays=*/9,
                                             /*packedRayBytes=*/90,
                                             /*hostQueryBytes=*/45,
                                             /*stateHandleBytes=*/27);

    ASSERT_EQ(3u, metrics.directLightSelectionHostBytesPerDepth.size());
    ASSERT_EQ(3u, metrics.directLightAnyHitBatchChunksPerDepth.size());
    ASSERT_EQ(3u, metrics.directLightAnyHitBatchRaysPerDepth.size());
    ASSERT_EQ(3u, metrics.directLightOcclusionHostBytesPerDepth.size());
    EXPECT_EQ(12u, metrics.directLightSelectionHostBytesPerDepth[2]);
    EXPECT_EQ(3u, metrics.directLightAnyHitBatchChunksPerDepth[2]);
    EXPECT_EQ(9u, metrics.directLightAnyHitBatchRaysPerDepth[2]);
    EXPECT_EQ(90u, metrics.directLightAnyHitFrontierPackedRayBytesPerDepth[2]);
    EXPECT_EQ(45u, metrics.directLightAnyHitFrontierHostQueryBytesPerDepth[2]);
    EXPECT_EQ(27u, metrics.directLightAnyHitFrontierStateHandleBytesPerDepth[2]);
    EXPECT_EQ(4u, metrics.directLightOcclusionHostBytesPerDepth[2]);
    EXPECT_EQ(12u, metrics.directLightSelectionHostBytes);
    EXPECT_EQ(4u, metrics.directLightOcclusionHostBytes);
    EXPECT_EQ(90u, metrics.directLightAnyHitFrontierPackedRayBytes);
    EXPECT_EQ(45u, metrics.directLightAnyHitFrontierHostQueryBytes);
    EXPECT_EQ(27u, metrics.directLightAnyHitFrontierStateHandleBytes);
  }

  TEST(IntegratorBatchMetrics, ResidentDirectLightBatchCandidatesIdentifyLargestDepth) {
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);

    metrics.recordDirectLightVisibilityDepth(/*depth=*/1,
                                             /*selectionHostBytes=*/5,
                                             /*occlusionHostBytes=*/2,
                                             /*batchChunks=*/1,
                                             /*batchRays=*/4,
                                             /*packedRayBytes=*/40,
                                             /*hostQueryBytes=*/20,
                                             /*stateHandleBytes=*/12);
    metrics.recordDirectLightContributionHostBytes(/*depth=*/1, /*bytes=*/7);
    metrics.recordDirectLightVisibilityDepth(/*depth=*/3,
                                             /*selectionHostBytes=*/3,
                                             /*occlusionHostBytes=*/5,
                                             /*batchChunks=*/2,
                                             /*batchRays=*/9,
                                             /*packedRayBytes=*/90,
                                             /*hostQueryBytes=*/45,
                                             /*stateHandleBytes=*/27);
    metrics.recordDirectLightContributionHostBytes(/*depth=*/3, /*bytes=*/11);

    EXPECT_FALSE(metrics.hasResidentDirectLightBatchCandidateDepth(0));
    EXPECT_TRUE(metrics.hasResidentDirectLightBatchCandidateDepth(1));
    EXPECT_FALSE(metrics.hasResidentDirectLightBatchCandidateDepth(2));
    EXPECT_TRUE(metrics.hasResidentDirectLightBatchCandidateDepth(3));
    EXPECT_EQ(2u, metrics.residentDirectLightBatchCandidateDepthCount());
    EXPECT_EQ(13u, metrics.residentDirectLightBatchCandidateRayCount());
    EXPECT_EQ(46u, metrics.residentDirectLightBatchHostBytesAtDepth(1));
    EXPECT_EQ(91u, metrics.residentDirectLightBatchHostBytesAtDepth(3));
    EXPECT_EQ(137u, metrics.residentDirectLightBatchCandidateHostBytes());
    EXPECT_EQ(3u, metrics.largestResidentDirectLightBatchDepth());
    EXPECT_EQ(9u, metrics.largestResidentDirectLightBatchRayCount());
    EXPECT_EQ(90u, metrics.largestResidentDirectLightBatchPackedRayBytes());
    EXPECT_EQ(91u, metrics.largestResidentDirectLightBatchHostBytes());
  }

  TEST(IntegratorBatchMetrics, CpuDirectLightContributionExecutionReportsGpuRequestFallback) {
    Scene scene;
    const std::shared_ptr<const WavefrontIntersectionBackend> cpuBackend =
      WavefrontIntersectionBackendChoice::cpu().createBackendForScene(scene);
    const std::shared_ptr<const WavefrontIntersectionBackend> gpuRequestedBackend =
      WavefrontIntersectionBackendChoice::gpu().createBackendForScene(scene);
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);

    metrics.recordCpuDirectLightContributionExecution(*cpuBackend, "GPU contribution unavailable");

    EXPECT_EQ("cpu", metrics.directLightContributionExecutionPath);
    EXPECT_TRUE(metrics.directLightContributionFallbackReason.empty());

    metrics.recordCpuDirectLightContributionExecution(*gpuRequestedBackend,
                                                      "GPU contribution unavailable");

    EXPECT_EQ("cpu", metrics.directLightContributionExecutionPath);
    EXPECT_EQ("GPU contribution unavailable", metrics.directLightContributionFallbackReason);
  }

  TEST(IntegratorBatchMetrics, DirectLightAnyHitFrontierQueryRecordsVisibilityAndQuery) {
    Scene scene;
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::cpu().createBackendForScene(scene);
    WavefrontIntersectionQueryTiming timing;
    timing.recordExecutionPath("runtime_cpu");
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);

    metrics.recordDirectLightAnyHitFrontierQuery(
      /*depth=*/3, /*selectionHostBytes=*/24, /*occlusionHostBytes=*/6, *backend, "host",
      /*submittedRays=*/4, /*packedRayBytes=*/0, /*hostQueryBytes=*/128,
      /*stateHandleBytes=*/0, timing);

    ASSERT_EQ(4u, metrics.directLightSelectionHostBytesPerDepth.size());
    ASSERT_EQ(4u, metrics.directLightAnyHitBatchChunksPerDepth.size());
    ASSERT_EQ(4u, metrics.directLightAnyHitBatchRaysPerDepth.size());
    ASSERT_EQ(4u, metrics.directLightOcclusionHostBytesPerDepth.size());
    EXPECT_EQ(24u, metrics.directLightSelectionHostBytesPerDepth[3]);
    EXPECT_EQ(1u, metrics.directLightAnyHitBatchChunksPerDepth[3]);
    EXPECT_EQ(4u, metrics.directLightAnyHitBatchRaysPerDepth[3]);
    EXPECT_EQ(128u, metrics.directLightAnyHitFrontierHostQueryBytesPerDepth[3]);
    EXPECT_EQ(6u, metrics.directLightOcclusionHostBytesPerDepth[3]);
    EXPECT_EQ(24u, metrics.directLightSelectionHostBytes);
    EXPECT_EQ(6u, metrics.directLightOcclusionHostBytes);
    EXPECT_EQ(128u, metrics.directLightAnyHitFrontierHostQueryBytes);
    EXPECT_EQ("host", metrics.intersectionBackendAnyHitFrontierResidency);
    EXPECT_EQ(128u, metrics.intersectionBackendAnyHitFrontierHostQueryBytes);
    EXPECT_EQ(4u, metrics.anyHitRaysSubmitted);
    EXPECT_EQ(1u, metrics.anyHitQueries);
    EXPECT_EQ("runtime_cpu", metrics.intersectionBackendAnyHitExecutionPath);
  }

  TEST(IntegratorBatchMetrics, FrontierQueryOverloadsReadPayloadFromFrontiers) {
    Scene scene;
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::cpu().createBackendForScene(scene);
    State closestStateA;
    State closestStateB;
    State anyStateA;
    State anyStateB;
    State anyStateC;
    HostWavefrontClosestHitFrontier closestFrontier(std::vector<WavefrontClosestHitQuery>{
      {Rayd(Vector3d(0.0, 0.0, -1.0), Vector3d(0.0, 0.0, 1.0)), &closestStateA},
      {Rayd(Vector3d(1.0, 0.0, -1.0), Vector3d(0.0, 0.0, 1.0)), &closestStateB}});
    HostWavefrontAnyHitFrontier anyFrontier(std::vector<WavefrontAnyHitQuery>{
      {Rayd(Vector3d(0.0, 1.0, -1.0), Vector3d(0.0, 0.0, 1.0)), 2.0, &anyStateA},
      {Rayd(Vector3d(1.0, 1.0, -1.0), Vector3d(0.0, 0.0, 1.0)), 3.0, &anyStateB},
      {Rayd(Vector3d(2.0, 1.0, -1.0), Vector3d(0.0, 0.0, 1.0)), 4.0, &anyStateC}});
    WavefrontIntersectionQueryTiming closestTiming;
    closestTiming.recordExecutionPath("closest_path");
    WavefrontIntersectionQueryTiming anyTiming;
    anyTiming.recordExecutionPath("any_path");
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);

    metrics.recordClosestHitFrontierQuery(*backend, closestFrontier, closestTiming);
    metrics.recordDirectLightAnyHitFrontierQuery(
      /*depth=*/1, /*selectionHostBytes=*/20, /*occlusionHostBytes=*/3, *backend, anyFrontier,
      anyTiming);

    EXPECT_EQ("host", metrics.intersectionBackendClosestHitFrontierResidency);
    EXPECT_EQ(2u * sizeof(WavefrontClosestHitQuery),
              metrics.intersectionBackendClosestHitFrontierHostQueryBytes);
    EXPECT_EQ(2u, metrics.closestHitRaysSubmitted);
    EXPECT_EQ(1u, metrics.closestHitQueries);
    EXPECT_EQ("closest_path", metrics.intersectionBackendClosestHitExecutionPath);

    EXPECT_EQ((std::vector<std::uint64_t>{0u, 1u}), metrics.directLightAnyHitBatchChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 3u}), metrics.directLightAnyHitBatchRaysPerDepth);
    EXPECT_EQ(3u * sizeof(WavefrontAnyHitQuery),
              metrics.directLightAnyHitFrontierHostQueryBytesPerDepth[1]);
    EXPECT_EQ("host", metrics.intersectionBackendAnyHitFrontierResidency);
    EXPECT_EQ(3u * sizeof(WavefrontAnyHitQuery),
              metrics.intersectionBackendAnyHitFrontierHostQueryBytes);
    EXPECT_EQ(3u, metrics.anyHitRaysSubmitted);
    EXPECT_EQ(1u, metrics.anyHitQueries);
    EXPECT_EQ("any_path", metrics.intersectionBackendAnyHitExecutionPath);
  }

  TEST(IntegratorBatchMetrics, SkippedDepthDiagnosticsPublishesZeroRows) {
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);
    metrics.recordActiveDepth(3);

    metrics.recordSkippedDepthDiagnostics(/*depth=*/2);

    EXPECT_EQ((std::vector<std::uint64_t>{3u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.activeHitHostBytesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierPacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierPacketRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierClosestHitBatchChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierClosestHitBatchRaysPerDepth);
    ASSERT_EQ(3u, metrics.directLightAnyHitBatchChunksPerDepth.size());
    ASSERT_EQ(3u, metrics.directLightAnyHitBatchRaysPerDepth.size());
    ASSERT_EQ(3u, metrics.directLightSelectionHostBytesPerDepth.size());
    ASSERT_EQ(3u, metrics.directLightOcclusionHostBytesPerDepth.size());
    ASSERT_EQ(3u, metrics.directLightContributionHostBytesPerDepth.size());
    EXPECT_EQ(0u, metrics.directLightAnyHitBatchChunksPerDepth[2]);
    EXPECT_EQ(0u, metrics.directLightAnyHitBatchRaysPerDepth[2]);
    EXPECT_EQ(0u, metrics.directLightSelectionHostBytesPerDepth[2]);
    EXPECT_EQ(0u, metrics.directLightOcclusionHostBytesPerDepth[2]);
    EXPECT_EQ(0u, metrics.directLightContributionHostBytesPerDepth[2]);
    EXPECT_EQ((std::vector<double>{0.0}), metrics.radianceDeltaSquaredSumPerDepth);
    EXPECT_EQ((std::vector<double>{0.0}), metrics.maxRadianceDeltaPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.spawnedContinuationSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}),
              metrics.spawnedContinuationHostPathStateBytesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.retainedActiveSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.retainedHostPathStateBytesPerDepth);
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

  TEST(Integrator, FrontierQueryMetricsRecordResidencyAndQuery) {
    Scene scene;
    const std::shared_ptr<const WavefrontIntersectionBackend> backend =
      WavefrontIntersectionBackendChoice::cpu().createBackendForScene(scene);
    WavefrontIntersectionQueryTiming closestTiming;
    closestTiming.recordExecutionPath("runtime_cpu");
    WavefrontIntersectionQueryTiming anyTiming;
    anyTiming.recordExecutionPath("runtime_cpu");
    IntegratorBatchMetrics metrics;
    metrics.reset(/*scalarFallback=*/false);

    metrics.recordClosestHitFrontierQuery(*backend, "host", /*submittedRays=*/5,
                                          /*packedRayBytes=*/0,
                                          /*hostQueryBytes=*/80,
                                          /*stateHandleBytes=*/0, closestTiming);
    metrics.recordAnyHitFrontierQuery(*backend, "packed_host", /*submittedRays=*/7,
                                      /*packedRayBytes=*/112,
                                      /*hostQueryBytes=*/0,
                                      /*stateHandleBytes=*/56, anyTiming);

    EXPECT_EQ("host", metrics.intersectionBackendClosestHitFrontierResidency);
    EXPECT_EQ("packed_host", metrics.intersectionBackendAnyHitFrontierResidency);
    EXPECT_EQ(80u, metrics.intersectionBackendClosestHitFrontierHostQueryBytes);
    EXPECT_EQ(112u, metrics.intersectionBackendAnyHitFrontierPackedRayBytes);
    EXPECT_EQ(56u, metrics.intersectionBackendAnyHitFrontierStateHandleBytes);
    EXPECT_EQ(5u, metrics.closestHitRaysSubmitted);
    EXPECT_EQ(7u, metrics.anyHitRaysSubmitted);
    EXPECT_EQ(1u, metrics.closestHitQueries);
    EXPECT_EQ(1u, metrics.anyHitQueries);
    EXPECT_EQ("runtime_cpu", metrics.intersectionBackendClosestHitExecutionPath);
    EXPECT_EQ("runtime_cpu", metrics.intersectionBackendAnyHitExecutionPath);
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
    EXPECT_EQ(backend->residentDirectLightBatchesUnavailableReason(),
              metrics.intersectionBackendResidentDirectLightBatchesUnavailableReason);
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
    EXPECT_EQ(backend->residentDirectLightBatchesUnavailableReason(),
              metrics.intersectionBackendResidentDirectLightBatchesUnavailableReason);
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
                                     /*retainedIndexBytes=*/2u * sizeof(std::uint32_t),
                                     /*inputHostPathStateBytes=*/300u,
                                     /*retainedHostPathStateBytes=*/200u,
                                     /*removedHostPathStateBytes=*/100u);
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
    EXPECT_EQ(300u, metrics.frontierCompactionInputHostPathStateBytes);
    EXPECT_EQ(200u, metrics.frontierCompactionRetainedHostPathStateBytes);
    EXPECT_EQ(100u, metrics.frontierCompactionRemovedHostPathStateBytes);
    EXPECT_EQ("mixed", metrics.frontierCompactionExecutionPath);
    EXPECT_EQ("host", metrics.frontierCompactionPathStateResidency);
    EXPECT_DOUBLE_EQ(5.0 / 17.0, metrics.frontierCompactionRemovedSampleFraction());
    EXPECT_DOUBLE_EQ(5.0 / 12.0, metrics.frontierCompactionMovedRetainedSampleFraction());
  }

  TEST(Integrator, BatchMetricsMergeResidentPathLoopAccumulation) {
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 2);
    TracingAccumulationDiagnostics first = TracingAccumulationDiagnostics::forLayout(
      layout, "gpu_resident_path_loop", "resident_accumulation_resolve");
    first.recordClear();
    first.recordAdd(/*samples=*/3, /*operations=*/1);
    first.recordResolve();
    first.recordReadback(layout.resolveBytes());

    TracingAccumulationDiagnostics second = TracingAccumulationDiagnostics::forLayout(
      layout, "gpu_resident_path_loop", "resident_accumulation_resolve");
    second.recordClear();
    second.recordAdd(/*samples=*/2, /*operations=*/1);
    second.recordResolve();
    second.recordReadback(layout.resolveBytes());

    IntegratorBatchMetrics target;
    target.reset(/*scalarFallback=*/false);
    target.recordResidentPathLoopAccumulation(first);

    IntegratorBatchMetrics source;
    source.reset(/*scalarFallback=*/false);
    source.recordResidentPathLoopAccumulation(second);
    target.mergeFrom(source);

    ASSERT_TRUE(target.residentPathLoopAccumulation);
    EXPECT_EQ("gpu_resident_path_loop", target.residentPathLoopAccumulation->backend);
    EXPECT_EQ("resident_accumulation_resolve", target.residentPathLoopAccumulation->residency);
    EXPECT_EQ(layout.totalBytes(), target.residentPathLoopAccumulation->residentBytes);
    EXPECT_EQ(2u, target.residentPathLoopAccumulation->clearOperations);
    EXPECT_EQ(2u, target.residentPathLoopAccumulation->addOperations);
    EXPECT_EQ(5u, target.residentPathLoopAccumulation->addedSamples);
    EXPECT_EQ(2u, target.residentPathLoopAccumulation->resolveOperations);
    EXPECT_EQ(2u, target.residentPathLoopAccumulation->readbackOperations);
    EXPECT_EQ(2u * layout.resolveBytes(), target.residentPathLoopAccumulation->readbackBytes);

    target.reset(/*scalarFallback=*/false);
    EXPECT_FALSE(target.residentPathLoopAccumulation);
  }

  TEST(Integrator, BatchMetricsRecordResidentPathLoopActualExecution) {
    TracingPathStateBuffers buffers(2);
    const Rayd ray(Vector4d(0.0, 0.0, 0.0, 1.0), Vector3d(0.0, 0.0, 1.0));
    buffers.appendActive(makeGpuPathStateRecord(ray, Colord::white(), Colord::black(),
                                                /*pixelIndex=*/0, /*sampleIndex=*/0,
                                                /*depth=*/0));
    buffers.appendActive(makeGpuPathStateRecord(ray, Colord::white(), Colord::black(),
                                                /*pixelIndex=*/1, /*sampleIndex=*/0,
                                                /*depth=*/0));

    ResidentPathLoopSettings settings;
    settings.maxDepth = 3;
    settings.russianRouletteDepth = 10;
    const ResidentPathLoopDiagnostics diagnostics = loopResidentDiffusePaths(
      buffers, settings, [](const GpuPathStateRecord& record, std::uint32_t) {
        if (record.pixelIndex == 1) {
          return std::optional<GpuPathStateRecord>();
        }
        GpuPathStateRecord next = record;
        next.origin[0] += 1.0f;
        return std::optional<GpuPathStateRecord>(next);
      });

    IntegratorBatchMetrics target;
    target.reset(/*scalarFallback=*/false);
    target.recordResidentPathLoopExecution(diagnostics, /*roundTrips=*/1);

    EXPECT_EQ("gpu_resident_path_loop", target.residentPathLoopExecutionPath);
    EXPECT_EQ("cpu_host", target.residentPathLoopResidency);
    EXPECT_EQ(3u, target.residentPathLoopDepths);
    EXPECT_EQ(4u, target.residentPathLoopInputPaths);
    EXPECT_EQ(2u, target.residentPathLoopRetainedPaths);
    EXPECT_EQ(2u, target.residentPathLoopRemovedPaths);
    EXPECT_EQ(0u, target.residentPathLoopMovedPaths);
    EXPECT_EQ(2u * sizeof(std::uint32_t), target.residentPathLoopRetainedIndexBytes);
    EXPECT_EQ(diagnostics.buffers.residentBytes, target.residentPathLoopResidentPathStateBytes);
    EXPECT_EQ(4u * sizeof(GpuPathStateRecord), target.residentPathLoopInputResidentPathStateBytes);
    EXPECT_EQ(2u * sizeof(GpuPathStateRecord),
              target.residentPathLoopRetainedResidentPathStateBytes);
    EXPECT_EQ(2u * sizeof(GpuPathStateRecord),
              target.residentPathLoopRemovedResidentPathStateBytes);
    EXPECT_EQ(3u, target.residentPathLoopCompactionPasses);
    EXPECT_EQ(1u, target.residentPathLoopRoundTrips);
    EXPECT_EQ(3u, target.residentPathLoopSavedHostReadbacks);
    EXPECT_EQ(4u * sizeof(GpuPathStateRecord), target.residentPathLoopSavedHostReadbackBytes);

    IntegratorBatchMetrics source;
    source.reset(/*scalarFallback=*/false);
    source.recordResidentPathLoopExecution(diagnostics, /*roundTrips=*/2);
    target.mergeFrom(source);

    EXPECT_EQ(6u, target.residentPathLoopDepths);
    EXPECT_EQ(8u, target.residentPathLoopInputPaths);
    EXPECT_EQ(3u, target.residentPathLoopRoundTrips);
    EXPECT_EQ(6u, target.residentPathLoopSavedHostReadbacks);

    target.reset(/*scalarFallback=*/false);
    EXPECT_TRUE(target.residentPathLoopExecutionPath.empty());
    EXPECT_EQ(0u, target.residentPathLoopDepths);
  }
}
