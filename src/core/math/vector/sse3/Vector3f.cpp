#include "core/math/Vector.h"

#ifdef __SSE__

using namespace std;

const Vector3<float>& Vector3<float>::null() {
  static Vector3<float> v;
  return v;
}

const Vector3<float>& Vector3<float>::one() {
  static Vector3<float> v(1, 1, 1);
  return v;
}

const Vector3<float>& Vector3<float>::epsilon() {
  static Vector3<float> v(numeric_limits<float>::epsilon(),
                          numeric_limits<float>::epsilon(),
                          numeric_limits<float>::epsilon());
  return v;
}

const Vector3<float>& Vector3<float>::undefined() {
  static Vector3<float> v(numeric_limits<float>::quiet_NaN(),
                          numeric_limits<float>::quiet_NaN(),
                          numeric_limits<float>::quiet_NaN());
  return v;
}

const Vector3<float>& Vector3<float>::minusInfinity() {
  static Vector3<float> v(-numeric_limits<float>::infinity(),
                          -numeric_limits<float>::infinity(),
                          -numeric_limits<float>::infinity());
  return v;
}

const Vector3<float>& Vector3<float>::plusInfinity() {
  static Vector3<float> v(numeric_limits<float>::infinity(),
                          numeric_limits<float>::infinity(),
                          numeric_limits<float>::infinity());
  return v;
}

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
