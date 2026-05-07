#pragma once

#include "gmock/gmock.h"
#include "render/viewplanes/ViewPlane.h"
#include "test/mocks/MockDestructor.h"

namespace testing {
  class MockViewPlane : public render::ViewPlane, public MockDestructor {
  public:
  };
}
