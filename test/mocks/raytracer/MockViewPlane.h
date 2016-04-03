#pragma once

#include "gmock/gmock.h"
#include "raytracer/viewplanes/ViewPlane.h"
#include "test/mocks/MockDestructor.h"

namespace testing {
  class MockViewPlane : public raytracer::ViewPlane, public MockDestructor {
  public:
  };
}
