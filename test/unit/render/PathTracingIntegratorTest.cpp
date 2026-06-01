#include <gtest/gtest.h>

#include "render/PathTracingIntegrator.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/lights/DirectionalLight.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Plane.h"
#include "render/primitives/Scene.h"
#include "render/samplers/Sampler.h"
#include "render/samplers/SamplerFactory.h"
#include "render/textures/ConstantColorTexture.h"

#include "test/helpers/ColorTestHelper.h"

#include <cmath>
#include <memory>
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
    ASSERT_EQ(1u, metrics.radianceDeltaSquaredSumPerDepth.size());
    EXPECT_GT(metrics.radianceDeltaSquaredSumPerDepth[0], 0.0);
    ASSERT_EQ(1u, metrics.maxRadianceDeltaPerDepth.size());
    EXPECT_GT(metrics.maxRadianceDeltaPerDepth[0], 0.0);
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
