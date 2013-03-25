#include "core/math/Vector.h"

#ifdef __SSE3__

using namespace std;

const Vector3<double>& Vector3<double>::null() {
  static Vector3<double>* v = new Vector3<double>();
  return *v;
}

const Vector3<double>& Vector3<double>::epsilon() {
  static Vector3<double>* v = new Vector3<double>(numeric_limits<double>::epsilon(),
                                                  numeric_limits<double>::epsilon(),
                                                  numeric_limits<double>::epsilon());
  return *v;
}

const Vector3<double>& Vector3<double>::undefined() {
  static Vector3<double>* v = new Vector3<double>(numeric_limits<double>::quiet_NaN(),
                                                  numeric_limits<double>::quiet_NaN(),
                                                  numeric_limits<double>::quiet_NaN());
  return *v;
}

const Vector3<double>& Vector3<double>::minusInfinity() {
  static Vector3<double>* v = new Vector3<double>(-numeric_limits<double>::infinity(),
                                                  -numeric_limits<double>::infinity(),
                                                  -numeric_limits<double>::infinity());
  return *v;
}

const Vector3<double>& Vector3<double>::plusInfinity() {
  static Vector3<double>* v = new Vector3<double>(numeric_limits<double>::infinity(),
                                                  numeric_limits<double>::infinity(),
                                                  numeric_limits<double>::infinity());
  return *v;
}

#endif


