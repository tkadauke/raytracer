#include "test/functional/support/RaytracerFeatureTest.h"
#include "test/functional/support/GivenWhenThen.h"

#include "render/cameras/PinholeCamera.h"
#include "render/cameras/OrthographicCamera.h"

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

WHEN(RaytracerFeatureTest, "i zoom by a factor of ([\\d.]+)") {
  double factor = std::stod(match[1]);
  auto camera = static_cast<render::Camera*>(test->camera().get());
  if (auto c = dynamic_cast<render::PinholeCamera*>(camera)) {
    c->setZoom(c->zoom() * factor);
  } else if (auto c = dynamic_cast<render::OrthographicCamera*>(camera)) {
    c->setZoom(c->zoom() * factor);
  }
}
