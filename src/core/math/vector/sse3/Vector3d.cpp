#include "core/math/Vector.h"

#ifdef __SSE3__

using namespace std;

const Vector3<double>& Vector3<double>::right() {
  static Vector3<double> v(1, 0, 0);
  return v;
}

const Vector3<double>& Vector3<double>::up() {
  static Vector3<double> v(0, 1, 0);
  return v;
}

const Vector3<double>& Vector3<double>::forward() {
  static Vector3<double> v(0, 0, 1);
  return v;
}

#endif
