#include "core/math/Vector.h"

#ifdef __SSE3__

using namespace std;

const Vector4<double>& Vector4<double>::null() {
  static Vector4<double>* v = new Vector4<double>();
  return *v;
}

const Vector4<double>& Vector4<double>::epsilon() {
  static Vector4<double>* v = new Vector4<double>(numeric_limits<double>::epsilon(),
                                                  numeric_limits<double>::epsilon(),
                                                  numeric_limits<double>::epsilon(),
                                                  numeric_limits<double>::epsilon());
  return *v;
}

const Vector4<double>& Vector4<double>::undefined() {
  static Vector4<double>* v = new Vector4<double>(numeric_limits<double>::quiet_NaN(),
                                                  numeric_limits<double>::quiet_NaN(),
                                                  numeric_limits<double>::quiet_NaN(),
                                                  numeric_limits<double>::quiet_NaN());
  return *v;
}

const Vector4<double>& Vector4<double>::minusInfinity() {
  static Vector4<double>* v = new Vector4<double>(-numeric_limits<double>::infinity(),
                                                  -numeric_limits<double>::infinity(),
                                                  -numeric_limits<double>::infinity(),
                                                  -numeric_limits<double>::infinity());
  return *v;
}

const Vector4<double>& Vector4<double>::plusInfinity() {
  static Vector4<double>* v = new Vector4<double>(numeric_limits<double>::infinity(),
                                                  numeric_limits<double>::infinity(),
                                                  numeric_limits<double>::infinity(),
                                                  numeric_limits<double>::infinity());
  return *v;
}

#endif
