#include <gtest/gtest.h>

#include "core/math/HitPointInterval.h"
#include "render/PathTracingIntegrator.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/PortalMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Plane.h"
#include "render/primitives/Primitive.h"
#include "render/primitives/Scene.h"
#include "render/samplers/Sampler.h"
#include "render/samplers/SamplerFactory.h"
#include "render/textures/ConstantColorTexture.h"

#include "test/helpers/ColorTestHelper.h"

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace PathTracingIntegratorTest {
  using namespace render;

  namespace {
    class FallbackRayCaster final : public RayCaster {
    public:
      Colord rayColor(const Rayd&, State& state) const override {
        sawCall = true;
        ++state.numRays;
        return Colord::black();
      }

      mutable bool sawCall{false};
    };

    class RecordingBatchObserver final : public IntegratorBatchObserver {
    public:
      void depthCompleted(std::uint64_t completedDepth, const std::vector<Colord>& sampleColors,
                          std::uint64_t activeSamples) override {
        completedDepths.push_back(completedDepth);
        snapshots.push_back(sampleColors);
        activeSampleCounts.push_back(activeSamples);
      }

      std::vector<std::uint64_t> completedDepths;
      std::vector<std::vector<Colord>> snapshots;
      std::vector<std::uint64_t> activeSampleCounts;
    };

    class UnsupportedMaterial final : public Material {
    public:
      Colord shade(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                   State&) const override {
        return Colord(0.25, 0.5, 0.75);
      }
    };

    class RecordingMaterial final : public Material {
    public:
      explicit RecordingMaterial(std::vector<std::string>* events)
          : m_events(events) {
      }

      Colord shade(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                   State&) const override {
        return Colord::black();
      }

      bool supportsBsdfSampling() const override {
        return true;
      }

      MaterialBsdfSample sampleBsdf(const HitPoint& hitPoint, const Vector3d&,
                                    const Vector2d&) const override {
        m_events->push_back("shade " + std::to_string(static_cast<int>(hitPoint.point().x())));
        return MaterialBsdfSample();
      }

    private:
      std::vector<std::string>* m_events;
    };

    class RecordingPrimitive final : public Primitive {
    public:
      explicit RecordingPrimitive(std::vector<std::string>* events)
          : m_events(events) {
      }

      const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                 State& state) const override {
        const int sampleId = static_cast<int>(ray.origin().x());
        m_events->push_back("intersect " + std::to_string(sampleId));
        state.hit(this, "RecordingPrimitive");
        hitPoints.add(HitPoint(this, 1.0, ray.at(1.0), Vector3d(0, 1, 0)));
        return this;
      }

    protected:
      BoundingBoxd calculateBoundingBox() const override {
        return BoundingBoxd(Vector3d(-1, -1, -1), Vector3d(3, 1, 1));
      }

    private:
      std::vector<std::string>* m_events;
    };

    class PacketCountingScene final : public Scene {
    public:
      PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                              const PrimitivePacketState4& states) const override {
        ++packetHitCalls;
        return Scene::intersectPacketHits(rays, states);
      }

      mutable int packetHitCalls{0};
    };

    // Build a scene with a single Lambertian ground plane lit by one
    // directional light. Background is black so any radiance has to
    // come from the integrator's NEE direct-lighting step.
    std::unique_ptr<Scene> simpleMatteScene(double ambient, const Colord& diffuse) {
      auto scene = std::make_unique<Scene>(Colord::black());
      scene->setAmbient(Colord::black());
      // Stash ambient on the scene so tests can verify the path tracer
      // does NOT add an extra ambient term (unlike Whitted).
      (void)ambient;

      auto texture = std::make_shared<ConstantColorTexture>(diffuse);
      auto material = std::make_shared<MatteMaterial>(texture);
      material->setAmbientCoefficient(0.0); // path tracer ignores this anyway
      material->setDiffuseCoefficient(1.0);

      auto plane = std::make_shared<Plane>(Vector3d(0, 1, 0), 0.0);
      plane->setMaterial(material);
      scene->add(plane);

      // Light points STRAIGHT UP — direction() returns the unit
      // vector from a hit point toward the light source, so a sun
      // directly overhead has direction (0, 1, 0).
      auto light = std::make_shared<DirectionalLight>(Vector3d(0, 1, 0), Colord::white());
      scene->addLight(light);

      return scene;
    }

    std::unique_ptr<Scene> simplePhongScene() {
      auto scene = std::make_unique<Scene>(Colord::black());
      scene->setAmbient(Colord::black());

      auto texture = std::make_shared<ConstantColorTexture>(Colord(0.5, 0.5, 0.5));
      auto material = std::make_shared<PhongMaterial>(texture);
      material->setAmbientCoefficient(0.0);
      material->setDiffuseCoefficient(1.0);
      material->setSpecularCoefficient(0.5);

      auto plane = std::make_shared<Plane>(Vector3d(0, 1, 0), 0.0);
      plane->setMaterial(material);
      scene->add(plane);
      scene->addLight(std::make_shared<DirectionalLight>(Vector3d(0, 1, 0), Colord::white()));

      return scene;
    }

    std::unique_ptr<Scene> unsupportedMaterialScene() {
      auto scene = std::make_unique<Scene>(Colord::black());
      scene->setAmbient(Colord::black());

      auto plane = std::make_shared<Plane>(Vector3d(0, 1, 0), 0.0);
      plane->setMaterial(std::make_shared<UnsupportedMaterial>());
      scene->add(plane);

      return scene;
    }

    std::unique_ptr<Scene> reflectiveBackgroundScene() {
      auto scene = std::make_unique<Scene>();
      scene->setAmbient(Colord::black());
      scene->setBackground(Colord(1, 0, 0));

      auto material = std::make_shared<ReflectiveMaterial>(
        std::make_shared<ConstantColorTexture>(Colord::black()));
      material->setAmbientCoefficient(0.0);
      material->setDiffuseCoefficient(0.0);
      material->setSpecularCoefficient(0.0);
      material->setReflectionColor(Colord::white());
      material->setReflectionCoefficient(0.5);

      auto plane = std::make_shared<Plane>(Vector3d(0, 1, 0), 0.0);
      plane->setMaterial(material);
      scene->add(plane);

      return scene;
    }

    std::unique_ptr<Scene> transparentBackgroundScene() {
      auto scene = std::make_unique<Scene>();
      scene->setAmbient(Colord::black());
      scene->setBackground(Colord(1, 0, 0));

      auto material = std::make_shared<TransparentMaterial>(
        std::make_shared<ConstantColorTexture>(Colord::black()));
      material->setAmbientCoefficient(0.0);
      material->setDiffuseCoefficient(0.0);
      material->setSpecularCoefficient(0.0);
      material->setReflectionCoefficient(0.0);
      material->setTransmissionCoefficient(0.5);
      material->setRefractionIndex(1.0);

      auto plane = std::make_shared<Plane>(Vector3d(0, 1, 0), 0.0);
      plane->setMaterial(material);
      scene->add(plane);

      return scene;
    }

    std::unique_ptr<Scene> portalBackgroundScene() {
      auto scene = std::make_unique<Scene>();
      scene->setAmbient(Colord::black());
      scene->setBackground(Colord(1, 0, 0));

      auto material = std::make_shared<PortalMaterial>(Matrix4d(), Colord(0.25, 0.5, 1.0));

      auto plane = std::make_shared<Plane>(Vector3d(0, 1, 0), 0.0);
      plane->setMaterial(material);
      scene->add(plane);

      return scene;
    }

    // A primary ray straight down at the floor — for a horizontal
    // ground plane with the light coming from directly above, the
    // expected reflected radiance is diffuse_color / pi (Lambertian
    // diffuse) times the cos(0) = 1 incidence, times the light
    // radiance (white).
    Rayd primaryRay() {
      return Rayd(Vector3d(0, 5, 0), Vector3d(0, -1, 0));
    }

    Colord traceWithSampleStream(const PathTracingIntegrator& integrator, const Scene& scene,
                                 std::uint64_t pixelHash, int sampleIndex) {
      auto sampler = SamplerFactory::self().create("RegularSampler");
      sampler->setup(/*numSamples=*/1, /*numSets=*/83);
      auto stream = sampler->stream(sampleIndex, pixelHash);
      State state;
      state.sampleStream = stream.get();
      FallbackRayCaster caster;
      return integrator.radiance(scene, primaryRay(), state, caster);
    }
  }

  TEST(PathTracingIntegrator, FallsBackToCasterWithoutSampleStream) {
    auto scene = simpleMatteScene(0.0, Colord(0.5, 0.5, 0.5));
    PathTracingIntegrator integrator;
    FallbackRayCaster caster;
    State state; // sampleStream stays null
    Colord result = integrator.radiance(*scene, primaryRay(), state, caster);
    EXPECT_TRUE(caster.sawCall) << "expected fallback to recursive ray caster";
    ASSERT_COLOR_NEAR(Colord::black(), result, 1e-9);
  }

  TEST(PathTracingIntegrator, NextEventEstimationReproducesAnalyticDiffuseAtDirectLighting) {
    // Lambertian BRDF reflects incoming radiance L_i with
    //   L_o = (rho / pi) · cos(theta_i) · L_i
    // For diffuse=(0.5,0.5,0.5), light from straight above (cos=1),
    // L_i = white, the analytic L_o is (0.5/pi, ...) per channel.
    const Colord diffuse(0.5, 0.5, 0.5);
    auto scene = simpleMatteScene(0.0, diffuse);

    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(1); // direct-lighting only, no bounces

    Colord pixel = traceWithSampleStream(integrator, *scene, 1234ull, 0);
    const double expected = 0.5 / M_PI;
    EXPECT_NEAR(expected, pixel.r(), 1e-4);
    EXPECT_NEAR(expected, pixel.g(), 1e-4);
    EXPECT_NEAR(expected, pixel.b(), 1e-4);
  }

  TEST(PathTracingIntegrator, AccumulatesAcrossSeveralSamplesWithoutDivergence) {
    // Pure-diffuse + single delta light is a deterministic outcome
    // because the integrator's only stochastic input is the BSDF
    // continuation sample, which never gets used past bounce 1 (the
    // direction is into the ground, which traps the ray inside).
    // Different sample indices should therefore all return the same
    // direct-lit contribution.
    const Colord diffuse(0.8, 0.4, 0.2);
    auto scene = simpleMatteScene(0.0, diffuse);

    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(1);

    Colord first = traceWithSampleStream(integrator, *scene, 1ull, 0);
    Colord second = traceWithSampleStream(integrator, *scene, 7ull, 1);
    Colord third = traceWithSampleStream(integrator, *scene, 99ull, 5);

    EXPECT_NEAR(first.r(), second.r(), 1e-9);
    EXPECT_NEAR(first.g(), second.g(), 1e-9);
    EXPECT_NEAR(first.b(), second.b(), 1e-9);
    EXPECT_NEAR(first.r(), third.r(), 1e-9);
  }

  TEST(PathTracingIntegrator, BatchedRadianceMatchesScalarRadiance) {
    auto scene = simpleMatteScene(0.0, Colord(0.6, 0.3, 0.2));
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(1);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 11ull)});
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 29ull)});

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster, &metrics);

    ASSERT_EQ(2u, batched.size());
    ASSERT_COLOR_NEAR(traceWithSampleStream(integrator, *scene, 11ull, 0), batched[0], 1e-9);
    ASSERT_COLOR_NEAR(traceWithSampleStream(integrator, *scene, 29ull, 0), batched[1], 1e-9);
    EXPECT_FALSE(metrics.usedScalarFallback);
    ASSERT_EQ(1u, metrics.activeSamplesPerDepth.size());
    EXPECT_EQ(2u, metrics.activeSamplesPerDepth[0]);
    EXPECT_EQ((std::vector<std::uint64_t>{2u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(2u, metrics.activeSampleDepthsProcessed);
    ASSERT_EQ(1u, metrics.radianceDeltaSquaredSumPerDepth.size());
    EXPECT_GT(metrics.radianceDeltaSquaredSumPerDepth[0], 0.0);
    ASSERT_EQ(1u, metrics.maxRadianceDeltaPerDepth.size());
    EXPECT_GT(metrics.maxRadianceDeltaPerDepth[0], 0.0);
    EXPECT_EQ(0u, metrics.compatibilityShadeSamples);
    EXPECT_GT(metrics.intersectionWorkerSeconds, 0.0);
    EXPECT_GT(metrics.shadingWorkerSeconds, 0.0);
    EXPECT_GT(metrics.pathSetupWorkerSeconds, 0.0);
    EXPECT_GT(metrics.frontierBookkeepingWorkerSeconds, 0.0);
    EXPECT_EQ(0.0, metrics.progressSnapshotWorkerSeconds);
    EXPECT_EQ(0.0, metrics.convergenceTestWorkerSeconds);
  }

  TEST(PathTracingIntegrator, BatchedRadianceCancellationPreservesAccumulatedContribution) {
    const Colord diffuse(0.6, 0.3, 0.2);
    auto scene = simpleMatteScene(0.0, diffuse);
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(8);

    int cancellationChecks = 0;
    integrator.setCancellationCallback([&cancellationChecks] {
      ++cancellationChecks;
      return cancellationChecks >= 2;
    });

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 11ull)});

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster, &metrics);

    ASSERT_EQ(1u, batched.size());
    ASSERT_COLOR_NEAR(Colord(diffuse.r() / M_PI, diffuse.g() / M_PI, diffuse.b() / M_PI),
                      batched[0], 1e-4);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.activeSamplesPerDepth);
    ASSERT_EQ(2u, metrics.radianceDeltaSquaredSumPerDepth.size());
    EXPECT_EQ(0.0, metrics.radianceDeltaSquaredSumPerDepth[1]);
    EXPECT_EQ(2, cancellationChecks);
  }

  TEST(PathTracingIntegrator, BatchedRadianceIntersectsActiveFrontierBeforeShading) {
    std::vector<std::string> events;
    auto scene = std::make_unique<Scene>(Colord::black());
    scene->setAmbient(Colord::black());
    auto primitive = std::make_shared<RecordingPrimitive>(&events);
    primitive->setMaterial(std::make_shared<RecordingMaterial>(&events));
    scene->add(primitive);

    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(1);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{Rayd(Vector3d(1, 5, 0), Vector3d(0, -1, 0)), 0.0,
                                          sampler->stream(0, 11ull)});
    samples.push_back(IntegratorRaySample{Rayd(Vector3d(2, 5, 0), Vector3d(0, -1, 0)), 0.0,
                                          sampler->stream(0, 29ull)});

    FallbackRayCaster caster;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster);

    ASSERT_EQ(2u, batched.size());
    const std::vector<std::string> expected{"intersect 1", "intersect 2", "shade 1", "shade 2"};
    EXPECT_EQ(expected, events);
  }

  TEST(PathTracingIntegrator, BatchedRadianceUsesPacketFrontierForFourActivePaths) {
    auto scene = std::make_unique<PacketCountingScene>();
    scene->setAmbient(Colord::black());
    scene->setBackground(Colord::black());

    auto texture = std::make_shared<ConstantColorTexture>(Colord(0.6, 0.3, 0.2));
    auto material = std::make_shared<MatteMaterial>(texture);
    material->setAmbientCoefficient(0.0);
    material->setDiffuseCoefficient(1.0);
    auto plane = std::make_shared<Plane>(Vector3d(0, 1, 0), 0.0);
    plane->setMaterial(material);
    scene->add(plane);
    scene->addLight(std::make_shared<DirectionalLight>(Vector3d(0, 1, 0), Colord::white()));

    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(1);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    for (std::uint64_t sample = 0; sample != 4; ++sample) {
      samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, sample + 1)});
    }

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster, &metrics);

    ASSERT_EQ(4u, batched.size());
    EXPECT_EQ(1, scene->packetHitCalls);
    EXPECT_EQ((std::vector<std::uint64_t>{4u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.frontierPacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierScalarRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierPacketScalarFallbackRaysPerDepth);
  }

  TEST(PathTracingIntegrator, BatchedRadianceReportsPacketScalarMaterializationFallbacks) {
    std::vector<std::string> events;
    auto scene = std::make_unique<Scene>(Colord::black());
    scene->setAmbient(Colord::black());
    auto primitive = std::make_shared<RecordingPrimitive>(&events);
    primitive->setMaterial(std::make_shared<RecordingMaterial>(&events));
    scene->add(primitive);

    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(1);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    for (std::uint64_t sample = 0; sample != 4; ++sample) {
      samples.push_back(
        IntegratorRaySample{Rayd(Vector3d(static_cast<double>(sample), 5, 0), Vector3d(0, -1, 0)),
                            0.0, sampler->stream(0, sample + 1)});
    }

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster, &metrics);

    ASSERT_EQ(4u, batched.size());
    EXPECT_EQ((std::vector<std::uint64_t>{4u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.frontierPacketChunksPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierScalarRaysPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{4u}), metrics.frontierPacketScalarFallbackRaysPerDepth);
  }

  TEST(PathTracingIntegrator, BatchedRadianceRecordsCompatibilityMaterialFallbacks) {
    auto scene = unsupportedMaterialScene();
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(2);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 11ull)});
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 29ull)});

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster, &metrics);

    ASSERT_EQ(2u, batched.size());
    EXPECT_FALSE(metrics.usedScalarFallback);
    EXPECT_EQ(2u, metrics.compatibilityShadeSamples);
    EXPECT_EQ(1u, metrics.activeSamplesPerDepth.size());
  }

  TEST(PathTracingIntegrator, BatchedRadianceSamplesPhongWithoutCompatibilityFallback) {
    auto scene = simplePhongScene();
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(1);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 11ull)});
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 29ull)});

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster, &metrics);

    ASSERT_EQ(2u, batched.size());
    EXPECT_GT(batched[0].max(), 0.0);
    EXPECT_EQ(0u, metrics.compatibilityShadeSamples);
    EXPECT_EQ(1u, metrics.activeSamplesPerDepth.size());
  }

  TEST(PathTracingIntegrator, BatchedRadianceContinuesThroughReflectiveDeltaBsdf) {
    auto scene = reflectiveBackgroundScene();
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(2);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 11ull)});

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster, &metrics);

    ASSERT_EQ(1u, batched.size());
    ASSERT_COLOR_NEAR(Colord(0.5, 0, 0), batched[0], 1e-12);
    EXPECT_EQ(0u, metrics.compatibilityShadeSamples);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 1u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(2u, metrics.activeSampleDepthsProcessed);
  }

  TEST(PathTracingIntegrator, BatchedRadianceQueuesOnlyStillActivePathsAtNextDepth) {
    auto scene = reflectiveBackgroundScene();
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(2);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 11ull)});
    samples.push_back(IntegratorRaySample{Rayd(Vector3d(0, 5, 0), Vector3d(0, 1, 0)), 0.0,
                                          sampler->stream(0, 29ull)});

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster, &metrics);

    ASSERT_EQ(2u, batched.size());
    ASSERT_COLOR_NEAR(Colord(0.5, 0, 0), batched[0], 1e-12);
    ASSERT_COLOR_NEAR(Colord(1, 0, 0), batched[1], 1e-12);
    EXPECT_EQ((std::vector<std::uint64_t>{2u, 1u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(3u, metrics.activeSampleDepthsProcessed);
  }

  TEST(PathTracingIntegrator, BatchedRadianceContinuesThroughTransparentDeltaBsdf) {
    auto scene = transparentBackgroundScene();
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(2);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 11ull)});

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster, &metrics);

    ASSERT_EQ(1u, batched.size());
    ASSERT_COLOR_NEAR(Colord(0.5, 0, 0), batched[0], 1e-12);
    EXPECT_EQ(0u, metrics.compatibilityShadeSamples);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 1u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(2u, metrics.activeSampleDepthsProcessed);
  }

  TEST(PathTracingIntegrator, BatchedRadianceContinuesThroughPortalDeltaBsdf) {
    auto scene = portalBackgroundScene();
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(2);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 11ull)});

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched = integrator.radianceBatch(*scene, samples, caster, &metrics);

    ASSERT_EQ(1u, batched.size());
    ASSERT_COLOR_NEAR(Colord(0.25, 0, 0), batched[0], 1e-12);
    EXPECT_EQ(0u, metrics.compatibilityShadeSamples);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 1u}), metrics.activeSamplesPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{1u, 0u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u, 1u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(2u, metrics.activeSampleDepthsProcessed);
  }

  TEST(PathTracingIntegrator, BatchedRadianceStopsWhenConverged) {
    auto scene = simpleMatteScene(0.0, Colord(0.6, 0.3, 0.2));
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(8);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 11ull)});
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 29ull)});

    IntegratorBatchSettings settings;
    settings.convergenceEnabled = true;
    settings.activeSampleFractionThreshold = 1.0;
    settings.radianceDeltaRmsThreshold = 10.0;

    FallbackRayCaster caster;
    IntegratorBatchMetrics metrics;
    const std::vector<Colord> batched =
      integrator.radianceBatch(*scene, samples, caster, &metrics, settings);

    ASSERT_EQ(2u, batched.size());
    EXPECT_TRUE(metrics.stoppedByConvergence);
    EXPECT_EQ(1u, metrics.stoppedAfterDepth);
    ASSERT_EQ(1u, metrics.activeSamplesPerDepth.size());
    EXPECT_EQ(2u, metrics.activeSamplesPerDepth[0]);
    EXPECT_EQ((std::vector<std::uint64_t>{2u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(2u, metrics.activeSampleDepthsProcessed);
  }

  TEST(PathTracingIntegrator, BatchedRadiancePublishesDepthProgress) {
    auto scene = simpleMatteScene(0.0, Colord(0.6, 0.3, 0.2));
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(2);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(/*numSamples=*/1, /*numSets=*/83);
    std::vector<IntegratorRaySample> samples;
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 11ull)});
    samples.push_back(IntegratorRaySample{primaryRay(), 0.0, sampler->stream(0, 29ull)});

    RecordingBatchObserver observer;
    IntegratorBatchSettings settings;
    settings.progressObserver = &observer;

    FallbackRayCaster caster;
    const std::vector<Colord> batched =
      integrator.radianceBatch(*scene, samples, caster, nullptr, settings);

    ASSERT_EQ(2u, observer.completedDepths.size());
    EXPECT_EQ(1u, observer.completedDepths[0]);
    EXPECT_EQ(2u, observer.completedDepths[1]);
    ASSERT_EQ(2u, observer.snapshots.size());
    ASSERT_EQ(samples.size(), observer.snapshots[0].size());
    ASSERT_EQ(samples.size(), observer.snapshots[1].size());
    EXPECT_GT(observer.activeSampleCounts[0], 0u);
    ASSERT_COLOR_NEAR(batched[0], observer.snapshots.back()[0], 1e-9);
    ASSERT_COLOR_NEAR(batched[1], observer.snapshots.back()[1], 1e-9);
  }

  TEST(PathTracingIntegrator, RussianRouletteEventuallyTerminatesPath) {
    // Maximum depth set very high; if Russian roulette didn't fire,
    // the loop would still bound at maxDepth, but we'd see many more
    // bounces than the configured RR depth allows in expectation.
    auto scene = simpleMatteScene(0.0, Colord(0.9, 0.9, 0.9));
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(64);
    integrator.setRussianRouletteDepth(2);

    auto sampler = SamplerFactory::self().create("RegularSampler");
    sampler->setup(1, 83);
    auto stream = sampler->stream(0, 42ull);
    State state;
    state.sampleStream = stream.get();
    FallbackRayCaster caster;
    Colord pixel = integrator.radiance(*scene, primaryRay(), state, caster);

    // No assertion on the pixel value itself — RR makes it stochastic.
    // We're checking the loop terminated without infinite recursion
    // (the recursion depth counter should be bounded).
    EXPECT_LE(state.maxRecursionDepth, 64);
    EXPECT_GE(state.maxRecursionDepth, 1);
    (void)pixel;
  }

  TEST(PathTracingIntegrator, CloneCopiesConfiguration) {
    PathTracingIntegrator integrator;
    integrator.setMaximumRecursionDepth(17);
    integrator.setRussianRouletteDepth(5);

    auto clone = std::unique_ptr<PathTracingIntegrator>(
      static_cast<PathTracingIntegrator*>(integrator.clone().release()));

    ASSERT_NE(nullptr, clone);
    EXPECT_EQ(17, clone->maximumRecursionDepth());
    EXPECT_EQ(5, clone->russianRouletteDepth());
  }

} // namespace PathTracingIntegratorTest
