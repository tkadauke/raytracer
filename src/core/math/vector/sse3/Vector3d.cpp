#include "core/math/Vector.h"

#ifdef __SSE3__

using namespace std;

const Vector3<double>& Vector3<double>::null() {
  static Vector3<double> v;
  return v;
}

const Vector3<double>& Vector3<double>::epsilon() {
  static Vector3<double> v(numeric_limits<double>::epsilon(),
                           numeric_limits<double>::epsilon(),
                           numeric_limits<double>::epsilon());
  return v;
}

const Vector3<double>& Vector3<double>::undefined() {
  static Vector3<double> v(numeric_limits<double>::quiet_NaN(),
                           numeric_limits<double>::quiet_NaN(),
                           numeric_limits<double>::quiet_NaN());
  return v;
}

const Vector3<double>& Vector3<double>::minusInfinity() {
  static Vector3<double> v(-numeric_limits<double>::infinity(),
                           -numeric_limits<double>::infinity(),
                           -numeric_limits<double>::infinity());
  return v;
}

const Vector3<double>& Vector3<double>::plusInfinity() {
  static Vector3<double> v(numeric_limits<double>::infinity(),
                           numeric_limits<double>::infinity(),
                           numeric_limits<double>::infinity());
  return v;
}

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
