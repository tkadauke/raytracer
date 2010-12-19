#include "math/HitPoint.h"
#include <limits>

using namespace std;

const HitPoint HitPoint::undefined = HitPoint(numeric_limits<double>::quiet_NaN(), Vector3d::undefined, Vector3d::undefined);
