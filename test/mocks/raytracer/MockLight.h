#pragma once

#include "gmock/gmock.h"
#include "raytracer/lights/PointLight.h"
#include "test/mocks/MockDestructor.h"

namespace testing {
  class MockLight : public raytracer::Light, public MockDestructor {
  public:
    inline virtual Vector3d direction(const Vector3d&) const {
      return Vector3d::null();
    }

    inline virtual Colord radiance() const {
      return Colord::white();
    }
  };
}
