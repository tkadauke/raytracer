#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "raytracer/cameras/PinholeCamera.h"
#include "raytracer/cameras/OrthographicCamera.h"

using namespace testing;

WHEN(RaytracerFeatureTest, "i look at the origin") {
  test->lookAtOrigin();
}

WHEN(RaytracerFeatureTest, "i look away from the origin") {
  test->lookAway();
}

WHEN(RaytracerFeatureTest, "i go far away from the origin") {
  test->goFarAway();
}

WHEN(RaytracerFeatureTest, "i zoom in") {
  auto camera = static_cast<raytracer::Camera*>(test->camera().get());
  if (auto c = dynamic_cast<raytracer::PinholeCamera*>(camera)) {
    c->setZoom(c->zoom() * 2);
  } else if (auto c = dynamic_cast<raytracer::OrthographicCamera*>(camera)) {
    c->setZoom(c->zoom() * 2);
  }
}

WHEN(RaytracerFeatureTest, "i zoom out") {
  auto camera = static_cast<raytracer::Camera*>(test->camera().get());
  if (auto c = dynamic_cast<raytracer::PinholeCamera*>(camera)) {
    c->setZoom(c->zoom() * 0.8);
  } else if (auto c = dynamic_cast<raytracer::OrthographicCamera*>(camera)) {
    c->setZoom(c->zoom() * 0.8);
  }
}
