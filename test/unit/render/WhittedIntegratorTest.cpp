#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "render/RayCaster.h"
#include "render/State.h"
#include "render/WhittedIntegrator.h"
#include "render/materials/Material.h"
#include "render/primitives/Scene.h"

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

    std::shared_ptr<NiceMock<MockPrimitive>> makeAlwaysHit(double distance = 1.0) {
      auto primitive = std::make_shared<NiceMock<MockPrimitive>>();
      BoundingBoxd bbox(Vector3d(-100, -100, -100), Vector3d(100, 100, 100));
      HitPoint hit(primitive.get(), distance, Vector4d(0, 0, distance, 1), Vector3d(0, 0, -1));
      ON_CALL(*primitive, calculateBoundingBox()).WillByDefault(Return(bbox));
      ON_CALL(*primitive, intersect(_, _, _))
        .WillByDefault(DoAll(AddHitPoint(hit), Return(primitive.get())));
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
}
