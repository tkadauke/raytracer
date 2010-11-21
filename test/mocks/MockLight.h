#include "gmock/gmock.h"
#include "Light.h"

class MockLight : public Light {
public:
  MockLight(const Vector3d& position, const Colord& color)
    : Light(position, color) {}
  
  virtual ~MockLight() { destructorCall(); }

  MOCK_METHOD0(destructorCall, void());
};
