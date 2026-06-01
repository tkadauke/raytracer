#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "render/RayCaster.h"
#include "render/State.h"
#include "render/WhittedIntegrator.h"
#include "render/materials/Material.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Scene.h"
#include "render/textures/ConstantColorTexture.h"

#include "core/math/BoundingBox.h"
#include "core/math/HitPoint.h"
#include "core/math/HitPointInterval.h"

#include "test/helpers/ColorTestHelper.h"
#include "test/mocks/raytracer/MockPrimitive.h"

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
    EXPECT_EQ((std::vector<std::uint64_t>{1u}), metrics.frontierRayHitsPerDepth);
    EXPECT_EQ((std::vector<std::uint64_t>{0u}), metrics.frontierRayMissesPerDepth);
    EXPECT_EQ(1u, metrics.activeSampleDepthsProcessed);
    EXPECT_EQ(1u, metrics.radianceDeltaSquaredSumPerDepth.size());
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
