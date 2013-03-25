#ifndef MOCK_VIEW_PLANE_H
#define MOCK_VIEW_PLANE_H

#include "gmock/gmock.h"
#include "raytracer/viewplanes/ViewPlane.h"

namespace testing {
  class MockViewPlane : public ViewPlane {
  public:
    virtual ~MockViewPlane() { destructorCall(); }

    MOCK_METHOD0(destructorCall, void());
  };
}

#endif
