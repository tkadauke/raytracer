#ifndef MOCK_LIGHT_H
#define MOCK_LIGHT_H

#include "gmock/gmock.h"
#include "raytracer/lights/PointLight.h"

namespace testing {
  class MockLight : public raytracer::Light {
  public:
    inline MockLight()
      : raytracer::Light()
    {
    }
  
    inline virtual ~MockLight() {
      destructorCall();
    }
    
    inline virtual Vector3d direction(const Vector3d&) const {
      return Vector3d::null();
    }

    inline virtual Colord radiance() const {
      return Colord::white();
    }

    MOCK_METHOD0(destructorCall, void());
  };
}

#endif
