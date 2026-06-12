#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "render/RayCaster.h"
#include "render/State.h"
#include "render/WavefrontIntersectionBackend.h"
#include "render/WhittedIntegrator.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/math/BoundingBox.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"

#include "test/helpers/ColorTestHelper.h"
#include "test/mocks/raytracer/MockPrimitive.h"

#include <stdexcept>

namespace WhittedIntegratorTest {
  using namespace ::testing;
  using namespace render;

  namespace {
    class FixedRayCaster final : public RayCaster {
    public:
      Colord rayColor(const Rayd&, State& state) const override {
        state.numRays += 10;
        return Colord(0.25, 0.5, 0.75);
      }
    };

    class IntegratorRayCaster final : public RayCaster {
    public:
      IntegratorRayCaster(const Scene& scene, const WhittedIntegrator& integrator)
          : m_scene(scene),
            m_integrator(integrator) {
      }

      Colord rayColor(const Rayd& ray, State& state) const override {
        return m_integrator.radiance(m_scene, ray, state, *this);
      }

    private:
      const Scene& m_scene;
      const WhittedIntegrator& m_integrator;
    };

    class RecordingBatchObserver final : public IntegratorBatchObserver {
    public:
      IntegratorBatchFeedback depthCompleted(std::uint64_t completedDepth,
                                             const std::vector<Colord>& sampleColors,
                                             std::uint64_t activeSamples) override {
        completedDepths.push_back(completedDepth);
        snapshots.push_back(sampleColors);
        activeSampleCounts.push_back(activeSamples);
        return feedback;
      }

      IntegratorBatchFeedback feedback;
      std::vector<std::uint64_t> completedDepths;
      std::vector<std::vector<Colord>> snapshots;
      std::vector<std::uint64_t> activeSampleCounts;
    };

    class RecursiveProbeMaterial final : public Material {
    public:
      Colord shade(const RayCaster* raycaster, const Scene& scene, const Rayd&,
                   const HitPoint& hitPoint, State& state) const override {
        sawSceneAmbient = scene.ambient();
        sawHitPoint = hitPoint;
        return scene.ambient() +
               raycaster->rayColor(Rayd(hitPoint.point(), Vector3d::forward()), state);
      }

      mutable Colord sawSceneAmbient{Colord::black()};
      mutable HitPoint sawHitPoint;
    };

    class ContinuationMaterial final : public Material {
    public:
      bool supportsWhittedContinuations() const override {
        return true;
      }

      Colord shade(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                   State&) const override {
        return Colord::black();
      }

      WhittedShadeResult shadeWhitted(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                                      State&) const override {
        WhittedShadeResult result;
        result.localRadiance = Colord(0.1, 0.0, 0.0);
        result.continuations.push_back(WhittedContinuation{
          Rayd(Vector3d(10, 0, 0), Vector3d::forward()), Colord(0.5, 0.5, 0.5), 0.5});
        return result;
      }
    };

    class BranchingContinuationMaterial final : public Material {
    public:
      bool supportsWhittedContinuations() const override {
        return true;
      }

      Colord shade(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                   State&) const override {
        return Colord::black();
      }

      WhittedShadeResult shadeWhitted(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                                      State&) const override {
        WhittedShadeResult result;
        result.localRadiance = Colord(0.1, 0.0, 0.0);
        result.continuations.push_back(WhittedContinuation{
          Rayd(Vector3d(10, 0, 0), Vector3d::forward()), Colord(0.25, 0.25, 0.25), 0.5});
        result.continuations.push_back(WhittedContinuation{
          Rayd(Vector3d(20, 0, 0), Vector3d::forward()), Colord(0.25, 0.25, 0.25), 0.5});
        return result;
      }
    };

    class TerminalContinuationMaterial final : public Material {
    public:
      bool supportsWhittedContinuations() const override {
        return true;
      }

      Colord shade(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                   State&) const override {
        return Colord::black();
      }

      WhittedShadeResult shadeWhitted(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                                      State&) const override {
        WhittedShadeResult result;
        result.localRadiance = Colord(0.1, 0.0, 0.0);
        result.continuations.push_back(WhittedContinuation{
          Rayd(Vector3d(10, 0, 0), Vector3d::forward()), Colord(0.5, 0.5, 0.5), 1e-6});
        return result;
      }
    };

    class AlternatingContinuationMaterial final : public Material {
    public:
      bool supportsWhittedContinuations() const override {
        return true;
      }

      Colord shade(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                   State&) const override {
        return Colord::black();
      }

      WhittedShadeResult shadeWhitted(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                                      State&) const override {
        const bool traceableContinuation = (m_shadeCalls++ % 2) == 0;
        WhittedShadeResult result;
        result.continuations.push_back(
          WhittedContinuation{Rayd(Vector3d(10, 0, 0), Vector3d::forward()), Colord::white(),
                              traceableContinuation ? 1.0 : 1e-6});
        return result;
      }

    private:
      mutable int m_shadeCalls{0};
    };

    class PacketCountingScene final : public Scene {
    public:
      PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                              const PrimitivePacketState4& states) const override {
        ++packet4HitCalls;
        return Scene::intersectPacketHits(rays, states);
      }

      PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                              const PrimitivePacketState8& states) const override {
        ++packet8HitCalls;
        return Scene::intersectPacketHits(rays, states);
      }

      mutable int packet4HitCalls{0};
      mutable int packet8HitCalls{0};
    };

    class CountingIntersectionBackend : public WavefrontIntersectionBackend {
    public:
      const char* name() const override {
        return "counting_cpu";
      }

      const Primitive*
      intersectClosest(const Scene& scene, const Rayd& ray, HitPointInterval& hitPoints,
                       State& state,
                       WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        ++scalarQueries;
        return CpuWavefrontIntersectionBackend::instance().intersectClosest(scene, ray, hitPoints,
                                                                            state, timing);
      }

      bool intersectAny(const Scene& scene, const Rayd& ray, double maxDistance, State& state,
                        WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        ++anyQueries;
        return CpuWavefrontIntersectionBackend::instance().intersectAny(scene, ray, maxDistance,
                                                                        state, timing);
      }

      bool prefersClosestHitBatch(std::uint64_t submittedRays) const override {
        return preferClosestHitBatch && submittedRays > 1;
      }

      std::vector<WavefrontClosestHitResult>
      intersectClosestBatch(const Scene& scene,
                            const std::vector<WavefrontClosestHitQuery>& queries,
                            WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        ++closestHitBatchQueries;
        closestHitBatchSizes.push_back(queries.size());
        return WavefrontIntersectionBackend::intersectClosestBatch(scene, queries, timing);
      }

      PrimitivePacketHit4
      intersectPacketClosest(const Scene& scene, const Ray4& rays,
                             const PrimitivePacketState4& states,
                             WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        ++packet4Queries;
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states, timing);
      }

      PrimitivePacketHit8
      intersectPacketClosest(const Scene& scene, const Ray8& rays,
                             const PrimitivePacketState8& states,
                             WavefrontIntersectionQueryTiming* timing = nullptr) const override {
        ++packet8Queries;
        return CpuWavefrontIntersectionBackend::instance().intersectPacketClosest(scene, rays,
                                                                                  states, timing);
      }

      mutable int scalarQueries{0};
      mutable int closestHitBatchQueries{0};
      mutable std::vector<std::size_t> closestHitBatchSizes;
      mutable int packet4Queries{0};
      mutable int packet8Queries{0};
      mutable int anyQueries{0};
      bool preferClosestHitBatch{false};
    };

    class ShortClosestHitFrontierBackend final : public CountingIntersectionBackend {
    public:
      bool prefersClosestHitBatch(std::uint64_t submittedRays) const override {
        return submittedRays > 1;
      }

      std::unique_ptr<WavefrontClosestHitFrontier>
      createClosestHitFrontier(std::vector<WavefrontClosestHitQuery> queries) const override {
        return WavefrontIntersectionBackend::createClosestHitFrontier(std::move(queries));
      }

      std::vector<WavefrontClosestHitResult>
      intersectClosestFrontier(const Scene&, const WavefrontClosestHitFrontier& frontier,
                               WavefrontIntersectionQueryTiming* = nullptr) const override {
        requestedRayCounts.push_back(frontier.rayCount());
        if (frontier.rayCount() == 0) {
          return {};
        }
        return std::vector<WavefrontClosestHitResult>(
          static_cast<std::size_t>(frontier.rayCount() - 1));
      }

      mutable std::vector<std::uint64_t> requestedRayCounts;
    };

    std::shared_ptr<NiceMock<MockPrimitive>> makeAlwaysHit(double distance = 1.0) {
      auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
      BoundingBoxd bbox(Vector3d(-100, -100, -100), Vector3d(100, 100, 100));
      HitPoint hit(primitive.get(), distance, Vector4d(0, 0, distance, 1), Vector3d(0, 0, -1));
      ON_CALL(*primitive, calculateBoundingBox()).WillByDefault(Return(bbox));
      ON_CALL(*primitive, intersect(_, _, _))
        .WillByDefault(DoAll(AddHitPoint(hit), Return(primitive.get())));
      return primitive;
    }

    std::shared_ptr<NiceMock<MockPrimitive>> makePrimaryOnlyHit() {
      auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
      BoundingBoxd bbox(Vector3d(-100, -100, -100), Vector3d(100, 100, 100));
      HitPoint hit(primitive.get(), 1.0, Vector4d(0, 0, 1, 1), Vector3d(0, 0, -1));
      ON_CALL(*primitive, calculateBoundingBox()).WillByDefault(Return(bbox));
      ON_CALL(*primitive, intersect(_, _, _))
        .WillByDefault(
          Invoke([primitive = primitive.get(), hit](const Rayd& ray, HitPointInterval& hits,
                                                    State&) -> const Primitive* {
            if (ray.origin() == Vector3d::null) {
              hits.add(hit);
              return primitive;
            }
            return nullptr;
          }));
      return primitive;
    }
  }

  TEST(WhittedIntegrator, EstimatesIntersectionRaysFromRecursionDepth) {
    WhittedIntegrator integrator;

    EXPECT_EQ(10u, integrator.estimatedIntersectionRaysPerPrimarySample());
    EXPECT_EQ(10u, integrator.estimatedClosestHitRaysPerPrimarySample());
    EXPECT_EQ(0u, integrator.estimatedAnyHitRaysPerPrimarySample());

    integrator.setMaximumRecursionDepth(4);

    EXPECT_EQ(4u, integrator.estimatedIntersectionRaysPerPrimarySample());
    EXPECT_EQ(4u, integrator.estimatedClosestHitRaysPerPrimarySample());
    EXPECT_EQ(0u, integrator.estimatedAnyHitRaysPerPrimarySample());

    integrator.setMaximumRecursionDepth(0);

    EXPECT_EQ(1u, integrator.estimatedIntersectionRaysPerPrimarySample());
    EXPECT_EQ(1u, integrator.estimatedClosestHitRaysPerPrimarySample());
    EXPECT_EQ(0u, integrator.estimatedAnyHitRaysPerPrimarySample());
  }

  TEST(WhittedIntegrator, ReturnsSceneBackgroundWhenRayMissesEverything) {
    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    State state;

    const Colord color =
      integrator.radiance(scene, Rayd(Vector3d::null, Vector3d::forward()), state, rayCaster);

    ASSERT_COLOR_NEAR(scene.background(), color, 1e-12);
    EXPECT_EQ(0, state.recursionDepth);
    EXPECT_EQ(1, state.numRays);
  }

  TEST(WhittedIntegrator, ReturnsBlackWhenHitPrimitiveHasNoMaterial) {
    Scene scene;
    scene.setBackground(Colord::white());
    scene.add(makeAlwaysHit());
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    State state;

    const Colord color =
      integrator.radiance(scene, Rayd(Vector3d::null, Vector3d::forward()), state, rayCaster);

    ASSERT_COLOR_NEAR(Colord::black(), color, 1e-12);
  }

  TEST(WhittedIntegrator, ShadesHitMaterialWithSceneStateAndRecursiveRayCaster) {
    Scene scene;
    scene.setAmbient(Colord(0.1, 0.2, 0.3));
    auto primitive = makeAlwaysHit(2.0);
    auto material = std::make_shared<RecursiveProbeMaterial>();
    primitive->setMaterial(material);
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    State state;

    const Colord color =
      integrator.radiance(scene, Rayd(Vector3d::null, Vector3d::forward()), state, rayCaster);

    ASSERT_COLOR_NEAR(Colord(0.35, 0.7, 1.05), color, 1e-12);
    ASSERT_COLOR_NEAR(scene.ambient(), material->sawSceneAmbient, 1e-12);
    EXPECT_EQ(primitive.get(), material->sawHitPoint.primitive());
    EXPECT_EQ(11, state.numRays);
    EXPECT_EQ(0, state.recursionDepth);
  }

  TEST(WhittedIntegrator, ReturnsBackgroundAtMaximumRecursionDepthBeforeIntersecting) {
    Scene scene;
    scene.setBackground(Colord(0.7, 0.4, 0.1));
    auto primitive = makeAlwaysHit();
    scene.add(primitive);
    WhittedIntegrator integrator;
    integrator.setMaximumRecursionDepth(1);
    FixedRayCaster rayCaster;
    State state;

    EXPECT_CALL(*primitive, intersect(_, _, _)).Times(0);

    const Colord color =
      integrator.radiance(scene, Rayd(Vector3d::null, Vector3d::forward()), state, rayCaster);

    ASSERT_COLOR_NEAR(scene.background(), color, 1e-12);
    EXPECT_EQ(0, state.recursionDepth);
    EXPECT_EQ(1, state.maxRecursionDepth);
  }

  TEST(WhittedIntegrator, ReturnsBackgroundWhenThroughputIsBelowCutoffBeforeIntersecting) {
    Scene scene;
    scene.setBackground(Colord(0.3, 0.6, 0.9));
    auto primitive = makeAlwaysHit();
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    State state;
    state.throughput = 1e-5;

    EXPECT_CALL(*primitive, intersect(_, _, _)).Times(0);

    const Colord color =
      integrator.radiance(scene, Rayd(Vector3d::null, Vector3d::forward()), state, rayCaster);

    ASSERT_COLOR_NEAR(scene.background(), color, 1e-12);
    EXPECT_EQ(1, state.numRays);
  }

  TEST(WhittedIntegrator, BatchedRadianceProcessesExplicitContinuationsDepthMajor) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    auto primitive = makePrimaryOnlyHit();
    primitive->setMaterial(std::make_shared<ContinuationMaterial>());
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics);

    ASSERT_EQ(1u, colors.size());
    ASSERT_COLOR_NEAR(Colord(0.2, 0.2, 0.3), colors[0], 1e-12);
    EXPECT_STREQ("depth_major_whitted", integrator.batchExecutionMode());
    EXPECT_FALSE(metrics.usedScalarFallback);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 1u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(2u, metrics.activeSampleDepthsProcessed);
    EXPECT_EQ(2u, metrics.radianceDeltaSquaredSumPerDepth.size());
    EXPECT_GT(metrics.intersectionWorkerSeconds, 0.0);
    EXPECT_GT(metrics.shadingWorkerSeconds, 0.0);
  }

  TEST(WhittedIntegrator, BatchedRadianceUsesConfiguredIntersectionBackend) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    CountingIntersectionBackend backend;
    IntegratorBatchSettings settings;
    settings.intersectionBackend = &backend;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics, settings);

    ASSERT_EQ(1u, colors.size());
    ASSERT_COLOR_NEAR(scene.background(), colors[0], 1e-12);
    EXPECT_EQ(1, backend.scalarQueries);
    EXPECT_EQ(0, backend.packet4Queries);
    EXPECT_EQ(0, backend.packet8Queries);
    EXPECT_EQ("counting_cpu", metrics.intersectionBackendRequest);
    EXPECT_EQ("counting_cpu", metrics.intersectionBackend);
    EXPECT_EQ("available", metrics.intersectionBackendAvailability);
    EXPECT_TRUE(metrics.intersectionBackendFallbackReason.empty());
    EXPECT_EQ(1u, metrics.intersectionRaysSubmitted);
    EXPECT_EQ(1u, metrics.closestHitQueries);
    EXPECT_EQ(0u, metrics.anyHitQueries);
  }

  TEST(WhittedIntegrator, BatchedRadianceUsesClosestHitBatchWhenBackendPrefersIt) {
    Scene scene;
    scene.setAmbient(Colord::black());
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    auto primitive = makeAlwaysHit();
    primitive->setMaterial(std::make_shared<MatteMaterial>());
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    CountingIntersectionBackend backend;
    backend.preferClosestHitBatch = true;
    IntegratorBatchSettings settings;
    settings.intersectionBackend = &backend;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples;
    for (std::size_t sample = 0; sample != 5; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr});
    }

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics, settings);

    ASSERT_EQ(5u, colors.size());
    EXPECT_EQ(1, backend.closestHitBatchQueries);
    EXPECT_EQ((std::vector<std::size_t>{5u}), backend.closestHitBatchSizes);
    EXPECT_EQ(5, backend.scalarQueries);
    EXPECT_EQ(0, backend.packet4Queries);
    EXPECT_EQ(0, backend.packet8Queries);
    EXPECT_EQ((std::vector<std::uint64_t>{5u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierPacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierPacketRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.frontierClosestHitBatchChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{5u}), metrics.frontierClosestHitBatchRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierScalarRaysPerDepth);
    EXPECT_EQ(1u, metrics.closestHitQueries);
    EXPECT_EQ(5u, metrics.closestHitRaysSubmitted);
    EXPECT_EQ("host", metrics.intersectionBackendClosestHitFrontierResidency);
    EXPECT_EQ(0u, metrics.intersectionBackendClosestHitFrontierPackedRayBytes);
    EXPECT_GT(metrics.activeHitHostBytesProcessed, 0u);
    EXPECT_EQ((std::vector<std::uint64_t>{metrics.activeHitHostBytesProcessed}),
              metrics.activeHitHostBytesPerDepth);
  }

  TEST(WhittedIntegrator, BatchedRadianceRejectsMismatchedClosestHitResults) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    ShortClosestHitFrontierBackend backend;
    IntegratorBatchSettings settings;
    settings.intersectionBackend = &backend;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples;
    for (std::size_t sample = 0; sample != 3; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr});
    }

    EXPECT_THROW(integrator.radianceBatch(scene, samples, rayCaster, &metrics, settings),
                 std::logic_error);
    EXPECT_EQ((std::vector<std::uint64_t>{3u}), backend.requestedRayCounts);
  }

  TEST(WhittedIntegrator, BatchedRadianceReportsSetupTiming) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples;
    samples.reserve(256);
    for (std::size_t sample = 0; sample != 256; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr});
    }

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics);

    ASSERT_EQ(samples.size(), colors.size());
    EXPECT_GT(metrics.pathSetupWorkerSeconds, 0.0);
    EXPECT_EQ(0.0, metrics.progressSnapshotWorkerSeconds);
    EXPECT_EQ(0.0, metrics.convergenceTestWorkerSeconds);
  }

  TEST(WhittedIntegrator, BatchedRadianceStopsExplicitContinuationsWhenConverged) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    auto primitive = makePrimaryOnlyHit();
    primitive->setMaterial(std::make_shared<ContinuationMaterial>());
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    IntegratorBatchSettings settings;
    settings.convergenceEnabled = true;
    settings.activeSampleFractionThreshold = 1.0;
    settings.radianceDeltaRmsThreshold = 10.0;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics, settings);

    ASSERT_EQ(1u, colors.size());
    ASSERT_COLOR_NEAR(Colord(0.1, 0.0, 0.0), colors[0], 1e-12);
    EXPECT_TRUE(metrics.stoppedByConvergence);
    EXPECT_EQ(1u, metrics.stoppedAfterDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.retainedActiveSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(1u, metrics.activeSampleDepthsProcessed);
    EXPECT_EQ(1u, metrics.radianceDeltaSquaredSumPerDepth.size());
    EXPECT_EQ(0.0, metrics.progressSnapshotWorkerSeconds);
  }

  TEST(WhittedIntegrator, BatchedRadianceUsesObserverFeedbackForConvergenceDelta) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    auto primitive = makePrimaryOnlyHit();
    primitive->setMaterial(std::make_shared<ContinuationMaterial>());
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    RecordingBatchObserver observer;
    observer.feedback.convergenceRadianceDeltaRms = 0.0;
    IntegratorBatchSettings settings;
    settings.convergenceEnabled = true;
    settings.activeSampleFractionThreshold = 1.0;
    settings.radianceDeltaRmsThreshold = 0.0;
    settings.progressObserver = &observer;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics, settings);

    ASSERT_EQ(1u, colors.size());
    ASSERT_COLOR_NEAR(Colord(0.1, 0.0, 0.0), colors[0], 1e-12);
    EXPECT_TRUE(metrics.stoppedByConvergence);
    EXPECT_EQ(1u, metrics.stoppedAfterDepth);
    EXPECT_EQ(1u, metrics.observerConvergenceFeedbackDepths);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.retainedActiveSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), observer.completedDepths);
  }

  TEST(WhittedIntegrator, BatchedRadianceReportsProgressSnapshotTiming) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    auto primitive = makePrimaryOnlyHit();
    primitive->setMaterial(std::make_shared<ContinuationMaterial>());
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    RecordingBatchObserver observer;
    IntegratorBatchSettings settings;
    settings.progressObserver = &observer;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics, settings);

    ASSERT_EQ(1u, colors.size());
    ASSERT_EQ(2u, observer.completedDepths.size());
    EXPECT_EQ(1u, observer.completedDepths[0]);
    EXPECT_EQ(2u, observer.completedDepths[1]);
    ASSERT_EQ(2u, observer.snapshots.size());
    ASSERT_EQ(samples.size(), observer.snapshots[0].size());
    ASSERT_EQ(samples.size(), observer.snapshots[1].size());
    EXPECT_EQ(1u, observer.activeSampleCounts[0]);
    EXPECT_EQ(0u, observer.activeSampleCounts[1]);
    ASSERT_COLOR_NEAR(colors[0], observer.snapshots.back()[0], 1e-12);
    EXPECT_GT(metrics.progressSnapshotWorkerSeconds, 0.0);
    EXPECT_EQ(0.0, metrics.convergenceTestWorkerSeconds);
  }

  TEST(WhittedIntegrator, BatchedRadianceCountsBranchedContinuationsAsOneActiveSample) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    auto primitive = makePrimaryOnlyHit();
    primitive->setMaterial(std::make_shared<BranchingContinuationMaterial>());
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics);

    ASSERT_EQ(1u, colors.size());
    ASSERT_COLOR_NEAR(Colord(0.2, 0.2, 0.3), colors[0], 1e-12);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 2u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(2u, metrics.activeSampleDepthsProcessed);
  }

  TEST(WhittedIntegrator, BatchedRadianceResolvesTerminalContinuationsWithoutNextDepthFrontier) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    auto primitive = makePrimaryOnlyHit();
    primitive->setMaterial(std::make_shared<TerminalContinuationMaterial>());
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    RecordingBatchObserver observer;
    IntegratorBatchSettings settings;
    settings.progressObserver = &observer;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics, settings);

    ASSERT_EQ(1u, colors.size());
    ASSERT_COLOR_NEAR(Colord(0.2, 0.2, 0.3), colors[0], 1e-12);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(1u, metrics.activeSampleDepthsProcessed);
    ASSERT_EQ(1u, observer.completedDepths.size());
    EXPECT_EQ(1u, observer.completedDepths[0]);
    EXPECT_EQ(0u, observer.activeSampleCounts[0]);
    ASSERT_EQ(1u, observer.snapshots.size());
    ASSERT_COLOR_NEAR(colors[0], observer.snapshots[0][0], 1e-12);
  }

  TEST(WhittedIntegrator, BatchedRadianceUsesPacketFrontierForFourQueuedRays) {
    auto scene = std::make_unique<PacketCountingScene>();
    scene->setBackground(Colord(0.2, 0.4, 0.6));
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0);
    sphere->setMaterial(std::make_shared<ContinuationMaterial>());
    scene->add(sphere);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples;
    for (std::size_t sample = 0; sample != 4; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr});
    }

    const std::vector<Colord> colors =
      integrator.radianceBatch(*scene, samples, rayCaster, &metrics);

    ASSERT_EQ(4u, colors.size());
    for (const auto& color : colors) {
      ASSERT_COLOR_NEAR(Colord(0.2, 0.2, 0.3), color, 1e-12);
    }
    EXPECT_EQ(2, scene->packet4HitCalls);
    EXPECT_EQ(0, scene->packet8HitCalls);
    EXPECT_EQ((std::vector<std::uint64_t>{4u, 4u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{4u, 0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 4u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierPacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{4u, 4u}), metrics.frontierPacketRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierRay4PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierRay8PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierScalarRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}),
              metrics.frontierPacketScalarFallbackRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{4u, 0u}), metrics.frontierPacketRefinedRaysPerDepth);
    EXPECT_EQ(4u, metrics.frontierPacketRefinedRaysByMaterial.at("custom"));
  }

  TEST(WhittedIntegrator, BatchedRadianceUsesPartialPacketFrontierForTwoQueuedRays) {
    auto scene = std::make_unique<PacketCountingScene>();
    scene->setBackground(Colord(0.2, 0.4, 0.6));
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0);
    sphere->setMaterial(std::make_shared<ContinuationMaterial>());
    scene->add(sphere);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples;
    for (std::size_t sample = 0; sample != 2; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr});
    }

    const std::vector<Colord> colors =
      integrator.radianceBatch(*scene, samples, rayCaster, &metrics);

    ASSERT_EQ(2u, colors.size());
    for (const auto& color : colors) {
      ASSERT_COLOR_NEAR(Colord(0.2, 0.2, 0.3), color, 1e-12);
    }
    EXPECT_EQ(2, scene->packet4HitCalls);
    EXPECT_EQ(0, scene->packet8HitCalls);
    EXPECT_EQ((std::vector<std::uint64_t>{2u, 2u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{2u, 0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 2u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierPacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{2u, 2u}), metrics.frontierPacketRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierRay4PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierRay8PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierScalarRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}),
              metrics.frontierPacketScalarFallbackRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{2u, 0u}), metrics.frontierPacketRefinedRaysPerDepth);
    EXPECT_EQ(2u, metrics.frontierPacketRefinedRaysByMaterial.at("custom"));
  }

  TEST(WhittedIntegrator, BatchedRadianceUsesPartialRay8FrontierForFiveQueuedRays) {
    auto scene = std::make_unique<PacketCountingScene>();
    scene->setBackground(Colord(0.2, 0.4, 0.6));
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0);
    sphere->setMaterial(std::make_shared<ContinuationMaterial>());
    scene->add(sphere);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples;
    for (std::size_t sample = 0; sample != 5; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr});
    }

    const std::vector<Colord> colors =
      integrator.radianceBatch(*scene, samples, rayCaster, &metrics);

    ASSERT_EQ(5u, colors.size());
    for (const auto& color : colors) {
      ASSERT_COLOR_NEAR(Colord(0.2, 0.2, 0.3), color, 1e-12);
    }
    EXPECT_EQ(0, scene->packet4HitCalls);
    EXPECT_EQ(2, scene->packet8HitCalls);
    EXPECT_EQ((std::vector<std::uint64_t>{5u, 5u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{5u, 0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 5u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierPacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{5u, 5u}), metrics.frontierPacketRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierRay4PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierRay8PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierScalarRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}),
              metrics.frontierPacketScalarFallbackRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{5u, 0u}), metrics.frontierPacketRefinedRaysPerDepth);
    EXPECT_EQ(5u, metrics.frontierPacketRefinedRaysByMaterial.at("custom"));
  }

  TEST(WhittedIntegrator, BatchedRadianceUsesRay8PacketFrontierForEightQueuedRays) {
    auto scene = std::make_unique<PacketCountingScene>();
    scene->setBackground(Colord(0.2, 0.4, 0.6));
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0);
    sphere->setMaterial(std::make_shared<ContinuationMaterial>());
    scene->add(sphere);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples;
    for (std::size_t sample = 0; sample != 8; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr});
    }

    const std::vector<Colord> colors =
      integrator.radianceBatch(*scene, samples, rayCaster, &metrics);

    ASSERT_EQ(8u, colors.size());
    for (const auto& color : colors) {
      ASSERT_COLOR_NEAR(Colord(0.2, 0.2, 0.3), color, 1e-12);
    }
    EXPECT_EQ(0, scene->packet4HitCalls);
    EXPECT_EQ(2, scene->packet8HitCalls);
    EXPECT_EQ((std::vector<std::uint64_t>{8u, 8u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{8u, 0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 8u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierPacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{8u, 8u}), metrics.frontierPacketRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierRay4PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierRay8PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierScalarRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}),
              metrics.frontierPacketScalarFallbackRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{8u, 0u}), metrics.frontierPacketRefinedRaysPerDepth);
    EXPECT_EQ(8u, metrics.frontierPacketRefinedRaysByMaterial.at("custom"));
  }

  TEST(WhittedIntegrator, BatchedRadianceCompactsTraceableContinuationsBeforePacketFrontier) {
    auto scene = std::make_unique<PacketCountingScene>();
    scene->setBackground(Colord(0.2, 0.4, 0.6));
    auto primitive = makePrimaryOnlyHit();
    primitive->setMaterial(std::make_shared<AlternatingContinuationMaterial>());
    scene->add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples;
    for (std::size_t sample = 0; sample != 8; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr});
    }

    const std::vector<Colord> colors =
      integrator.radianceBatch(*scene, samples, rayCaster, &metrics);

    ASSERT_EQ(8u, colors.size());
    EXPECT_EQ(1, scene->packet4HitCalls);
    EXPECT_EQ(1, scene->packet8HitCalls);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierPacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{8u, 4u}), metrics.frontierPacketRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 1u}), metrics.frontierRay4PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 0u}), metrics.frontierRay8PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierScalarRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{8u, 4u}),
              metrics.frontierPacketScalarFallbackRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierPacketRefinedRaysPerDepth);
    EXPECT_TRUE(metrics.frontierPacketRefinedRaysByMaterial.empty());
  }

  TEST(WhittedIntegrator, BatchedRadianceLeavesLocalMaterialPacketHitsUnrefined) {
    auto scene = std::make_unique<PacketCountingScene>();
    scene->setAmbient(Colord(0.2, 0.3, 0.4));
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0);
    sphere->setMaterial(
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white())));
    scene->add(sphere);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples;
    for (std::size_t sample = 0; sample != 4; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr});
    }

    const std::vector<Colord> colors =
      integrator.radianceBatch(*scene, samples, rayCaster, &metrics);

    ASSERT_EQ(4u, colors.size());
    for (const auto& color : colors) {
      ASSERT_COLOR_NEAR(scene->ambient(), color, 1e-12);
    }
    EXPECT_EQ(1, scene->packet4HitCalls);
    EXPECT_EQ(0, scene->packet8HitCalls);
    EXPECT_EQ((std::vector<std::uint64_t>{4u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{4u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.frontierPacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{4u}), metrics.frontierPacketRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.frontierRay4PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRay8PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierScalarRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierPacketScalarFallbackRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierPacketRefinedRaysPerDepth);
    EXPECT_TRUE(metrics.frontierPacketRefinedRaysByMaterial.empty());
  }

  TEST(WhittedIntegrator, BatchedRadianceLeavesBuiltInContinuationPacketHitsUnrefined) {
    auto scene = std::make_unique<PacketCountingScene>();
    scene->setBackground(Colord(0.2, 0.4, 0.6));
    scene->setAmbient(Colord::black());
    auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, 3), 1.0);
    auto material =
      std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord::black()));
    material->setAmbientCoefficient(0.0);
    material->setDiffuseCoefficient(0.0);
    material->setSpecularCoefficient(0.0);
    material->setReflectionCoefficient(0.5);
    material->setReflectionColor(Colord::white());
    sphere->setMaterial(material);
    scene->add(sphere);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples;
    for (std::size_t sample = 0; sample != 4; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr});
    }

    const std::vector<Colord> colors =
      integrator.radianceBatch(*scene, samples, rayCaster, &metrics);

    ASSERT_EQ(4u, colors.size());
    for (const auto& color : colors) {
      ASSERT_COLOR_NEAR(Colord(0.1, 0.2, 0.3), color, 1e-12);
    }
    EXPECT_EQ(2, scene->packet4HitCalls);
    EXPECT_EQ((std::vector<std::uint64_t>{4u, 4u}), metrics.frontierPacketRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierRay4PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierRay8PacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}),
              metrics.frontierPacketScalarFallbackRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 0u}), metrics.frontierPacketRefinedRaysPerDepth);
    EXPECT_TRUE(metrics.frontierPacketRefinedRaysByMaterial.empty());
  }

  TEST(WhittedIntegrator, BatchedRadianceConvergesBranchedContinuationsByActiveSampleCount) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    auto primitive = makePrimaryOnlyHit();
    primitive->setMaterial(std::make_shared<BranchingContinuationMaterial>());
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    IntegratorBatchSettings settings;
    settings.convergenceEnabled = true;
    settings.activeSampleFractionThreshold = 1.0;
    settings.radianceDeltaRmsThreshold = 10.0;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics, settings);

    ASSERT_EQ(1u, colors.size());
    ASSERT_COLOR_NEAR(Colord(0.1, 0.0, 0.0), colors[0], 1e-12);
    EXPECT_TRUE(metrics.stoppedByConvergence);
    EXPECT_EQ(1u, metrics.stoppedAfterDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(1u, metrics.activeSampleDepthsProcessed);
  }

  TEST(WhittedIntegrator, BatchedRadianceFallsBackForUnsupportedMaterials) {
    Scene scene;
    scene.setAmbient(Colord(0.1, 0.2, 0.3));
    auto primitive = makeAlwaysHit(2.0);
    primitive->setMaterial(std::make_shared<RecursiveProbeMaterial>());
    scene.add(primitive);
    WhittedIntegrator integrator;
    FixedRayCaster rayCaster;
    IntegratorBatchMetrics metrics;
    std::vector<IntegratorRaySample> samples{
      IntegratorRaySample{Rayd(Vector3d::null, Vector3d::forward()), 0.0, nullptr}};

    const std::vector<Colord> colors =
      integrator.radianceBatch(scene, samples, rayCaster, &metrics);

    ASSERT_EQ(1u, colors.size());
    ASSERT_COLOR_NEAR(Colord(0.35, 0.7, 1.05), colors[0], 1e-12);
    EXPECT_TRUE(metrics.usedScalarFallback);
    EXPECT_EQ(1u, metrics.compatibilityShadeSamples);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
  }

  TEST(WhittedIntegrator, BatchedRadianceMatchesScalarReflectiveContinuations) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    scene.setAmbient(Colord::black());
    auto primitive = makePrimaryOnlyHit();
    auto material =
      std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord::black()));
    material->setReflectionCoefficient(0.2);
    material->setReflectionColor(Colord::white());
    material->setSpecularCoefficient(0.0);
    primitive->setMaterial(material);
    scene.add(primitive);
    WhittedIntegrator integrator;
    IntegratorRayCaster rayCaster(scene, integrator);
    const Rayd primaryRay(Vector3d::null, Vector3d::forward());
    State scalarState;
    const Colord scalar = integrator.radiance(scene, primaryRay, scalarState, rayCaster);
    std::vector<IntegratorRaySample> samples{IntegratorRaySample{primaryRay, 0.0, nullptr}};

    const std::vector<Colord> batched = integrator.radianceBatch(scene, samples, rayCaster);

    ASSERT_EQ(1u, batched.size());
    ASSERT_COLOR_NEAR(scalar, batched[0], 1e-12);
  }

  TEST(WhittedIntegrator, BatchedRadianceMatchesScalarTransparentContinuations) {
    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    scene.setAmbient(Colord::black());
    auto primitive = makePrimaryOnlyHit();
    auto material = std::make_shared<TransparentMaterial>(
      std::make_shared<ConstantColorTexture>(Colord::black()));
    material->setReflectionCoefficient(0.2);
    material->setReflectionColor(Colord::white());
    material->setTransmissionCoefficient(0.3);
    material->setRefractionIndex(1.0);
    material->setSpecularCoefficient(0.0);
    primitive->setMaterial(material);
    scene.add(primitive);
    WhittedIntegrator integrator;
    IntegratorRayCaster rayCaster(scene, integrator);
    const Rayd primaryRay(Vector3d::null, Vector3d::forward());
    State scalarState;
    const Colord scalar = integrator.radiance(scene, primaryRay, scalarState, rayCaster);
    std::vector<IntegratorRaySample> samples{IntegratorRaySample{primaryRay, 0.0, nullptr}};

    const std::vector<Colord> batched = integrator.radianceBatch(scene, samples, rayCaster);

    ASSERT_EQ(1u, batched.size());
    ASSERT_COLOR_NEAR(scalar, batched[0], 1e-12);
  }
}
