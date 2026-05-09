#include "test/functional/support/WireframeFeatureTest.h"

namespace WireframeFunctionalTest {
  using namespace ::testing;

  // Sanity checks for engine pluralism. The same step library used by
  // the Raytracer-based functional tests drives Wireframe renders here
  // — the only thing that switches is the engine factory and the
  // primary-colour key for visibility assertions (white edges instead
  // of red filled pixels). When Wireframe gains coverage gaps that the
  // engine-agnostic step library can't express (LOD-driven edge
  // density, edge-rasterisation correctness), those go into focused
  // unit tests under test/unit/render/WireframeTest.cpp instead.

  struct WireframeSphereTest : public WireframeFeatureTest {};

  TEST_F(WireframeSphereTest, ShouldBeVisibleInFrontOfTheCamera) {
    given("a centered sphere");
    when("i look at the origin");
    then("i should see the sphere");
  }

  TEST_F(WireframeSphereTest, ShouldNotBeVisibileOutsideOfViewFrustum) {
    given("a displaced sphere");
    when("i look at the origin");
    then("i should not see the sphere");
  }

  TEST_F(WireframeSphereTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered sphere");
    when("i look away from the origin");
    then("i should not see the sphere");
  }

  struct WireframeBoxTest : public WireframeFeatureTest {};

  TEST_F(WireframeBoxTest, ShouldBeVisibleInFrontOfTheCamera) {
    given("a centered box");
    when("i look at the origin");
    then("i should see the box");
  }

  TEST_F(WireframeBoxTest, ShouldNotBeVisibileBehindTheCamera) {
    given("a centered box");
    when("i look away from the origin");
    then("i should not see the box");
  }

  struct WireframeEmptyTest : public WireframeFeatureTest {};

  TEST_F(WireframeEmptyTest, EmptySceneRendersBackgroundOnly) {
    given("an empty scene");
    when("i look at the origin");
    then("i should see nothing");
  }
}

namespace WireframeEngineFunctionalTest {
  using namespace ::testing;

  struct WireframeLodTest : public WireframeFeatureTest {};

  // LOD scaling — Sphere tessellate produces 4× more quads per LOD step,
  // so a higher-LOD wireframe render must have strictly more edge pixels.
  TEST_F(WireframeLodTest, SphereEdgeCountIncreasesWithLod) {
    given("a centered sphere");
    when("i look at the origin");

    setLod(0);
    render();
    int lowLodEdges = objectSize();

    setLod(2);
    render();
    int highLodEdges = objectSize();

    ASSERT_GT(lowLodEdges, 0);
    EXPECT_GT(highLodEdges, lowLodEdges);
  }

  struct WireframeCancelTest : public WireframeFeatureTest {};

  // Cancellation — engine cancelled before render() is called must
  // produce a buffer that is entirely background: the clear runs first
  // (so every pixel gets the background colour), then the face loop
  // checks the flag immediately and exits without drawing any edges.
  TEST_F(WireframeCancelTest, PreCancelProducesOnlyBackground) {
    given("a centered sphere");
    when("i look at the origin");

    cancel();
    render();

    EXPECT_EQ(0, objectSize());
    EXPECT_EQ(buffer().width() * buffer().height(), colorCount(Colord::black()));
  }
}
