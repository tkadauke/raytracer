#include <gtest/gtest.h>
#include "render/brdf/BRDFSampling.h"

namespace BRDFSamplingTest {
  using namespace render;

  TEST(BRDFSampling, MirrorReflectionDirectionReflectsNormalIncidenceOntoItself) {
    // wi points away from the surface, toward where the ray came from. At
    // normal incidence (straight down the normal), the mirror reflection of
    // the incoming ray heads right back out along wi.
    const Vector3d n = Vector3d::up();
    const Vector3d wi = Vector3d::up();
    ASSERT_EQ(Vector3d::up(), mirrorReflectionDirection(n, wi));
  }

  TEST(BRDFSampling, MirrorReflectionDirectionFlipsTangentialComponent) {
    const Vector3d n = Vector3d(0, 1, 0);
    const Vector3d wi = Vector3d(1, 1, 0).normalized();
    const Vector3d result = mirrorReflectionDirection(n, wi);

    ASSERT_NEAR(-wi.x(), result.x(), 1e-12);
    ASSERT_NEAR(wi.y(), result.y(), 1e-12);
    ASSERT_NEAR(1.0, result.length(), 1e-12);
  }
}
