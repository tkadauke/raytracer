#include "gmock/gmock.h"
#include "Surface.h"
#include "Ray.h"
#include "HitPointInterval.h"

class MockSurface : public Surface {
public:
  virtual ~MockSurface() { destructorCall(); }

  MOCK_METHOD2(intersect, Surface*(const Ray&, HitPointInterval&));
  MOCK_METHOD0(destructorCall, void());
};

ACTION_P(AddHitPoint, hitPoint) {
  arg1.add(hitPoint);
}
