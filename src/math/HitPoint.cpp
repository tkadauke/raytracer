#include "math/HitPoint.h"
#include <limits>

using namespace std;

const HitPoint& HitPoint::undefined() {
  static HitPoint* h = new HitPoint(numeric_limits<double>::quiet_NaN(), Vector4d::undefined(), Vector3d::undefined());
  return *h;
}
