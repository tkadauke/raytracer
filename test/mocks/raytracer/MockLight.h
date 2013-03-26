#ifndef MOCK_LIGHT_H
#define MOCK_LIGHT_H

#include "gmock/gmock.h"
#include "raytracer/Light.h"

namespace testing {
  class MockLight : public raytracer::Light {
  public:
    MockLight(const Vector3d& position, const Colord& color)
      : raytracer::Light(position, color) {}
  
    virtual ~MockLight() { destructorCall(); }

    MOCK_METHOD0(destructorCall, void());
  };
}

#endif
