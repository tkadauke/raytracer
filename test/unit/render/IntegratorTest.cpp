#include <gtest/gtest.h>

#include "render/Integrator.h"
#include "render/RayCaster.h"
#include "render/State.h"
#include "render/primitives/Scene.h"

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
      Colord radiance(const Scene& scene, const Rayd& ray, State& state,
                      const RayCaster& recursiveRayCaster) const override {
        state.recurseIn();
        const Colord recursiveColor = recursiveRayCaster.rayColor(ray, state);
        state.recurseOut();
        return recursiveColor + scene.background();
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
}
