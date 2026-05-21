#include "core/math/Vector.h"

#ifdef __SSE__

using namespace std;

const Vector3<float>& Vector3<float>::right() {
  static Vector3<float> v(1, 0, 0);
  return v;
}

const Vector3<float>& Vector3<float>::up() {
  static Vector3<float> v(0, 1, 0);
  return v;
}

const Vector3<float>& Vector3<float>::forward() {
  static Vector3<float> v(0, 0, 1);
  return v;
}

#endif
