#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/cameras/ThinLensCamera.h"
#include "render/primitives/Sphere.h"
#include "core/math/Vector.h"
#include "core/Color.h"
#include "core/Buffer.h"

using namespace testing;
using namespace render;

namespace {
  // Count pixels that are neither the background white nor the sphere's pure red.
  // These intermediate colours appear only at the silhouette boundary of a sharp
  // render (one pixel wide), but spread across the full defocus-blurred band of
  // an out-of-focus render.  The ratio between the two counts is the DOF signal.
  int edgeTransitionCount(const Buffer<unsigned int>& buffer) {
    const unsigned int white = Colord(1, 1, 1).rgb();
    const unsigned int red = Colord(1, 0, 0).rgb();
    int count = 0;
    for (int y = 0; y < buffer.height(); ++y)
      for (int x = 0; x < buffer.width(); ++x) {
        unsigned int px = buffer[y][x];
        if (px != white && px != red)
          ++count;
      }
    return count;
  }
}

GIVEN(EngineFeatureTest, "a thin-lens camera") {
  test->setCamera(std::make_shared<ThinLensCamera>());
}

GIVEN(EngineFeatureTest, "a thin-lens camera with aperture radius ([\\d.]+)") {
  auto cam = std::make_shared<ThinLensCamera>();
  cam->setApertureRadius(std::stod(match[1]));
  cam->setViewPlane(cam->viewPlane());
  test->setCamera(cam);
}

GIVEN(EngineFeatureTest, "a thin-lens camera focused at distance ([\\d.]+)") {
  auto cam = std::make_shared<ThinLensCamera>();
  cam->setApertureRadius(0.5);
  cam->setFocalDistance(std::stod(match[1]));
  // Trigger the ThinLensCamera::setViewPlane override so the factory-default
  // 1-spp RegularSampler is upgraded to a 16-spp JitteredSampler.  Without
  // this the lens-disc sample is always the disc centre (u=v=0), collapsing
  // ThinLens to pinhole behaviour and making the DOF contrast unobservable.
  cam->setViewPlane(cam->viewPlane());
  test->setCamera(cam);
}

// Sphere 10 units in front of the camera (placed at z=5 when the camera
// sits at z=-5 via lookAtOrigin).  Used to test a sphere that is off the
// default focal plane.
GIVEN(EngineFeatureTest, "a sphere at distance ([\\d.]+)") {
  auto sphere = std::make_shared<Sphere>(Vector3d(0, 0, std::stod(match[1]) - 5.0), 1);
  sphere->setMaterial(test->redDiffuse());
  test->add(sphere);
}

// Snapshot the current edge-transition count so a later THEN step can
// compare it to a second render (analogous to "i should see the sphere
// with size S" / "with size larger than S").
THEN(EngineFeatureTest, "record edge count as S") {
  test->previousEdgeCount = edgeTransitionCount(test->buffer());
}

// A defocused render must produce strictly more partial-coverage pixels
// than the focused render captured by "record edge count as S".
THEN(EngineFeatureTest, "edge count should be larger than S") {
  int current = edgeTransitionCount(test->buffer());
  ASSERT_GT(current, test->previousEdgeCount) << "Defocused silhouette (" << current
                                              << " partial pixels) should "
                                                 "exceed the focused silhouette ("
                                              << test->previousEdgeCount << ")";
}
