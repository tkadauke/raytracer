#include "core/math/Vector.h"

#ifdef __SSE__

using namespace std;

const Vector4<float>& Vector4<float>::null() {
  static Vector4<float>* v = new Vector4<float>();
  return *v;
}

const Vector4<float>& Vector4<float>::epsilon() {
  static Vector4<float>* v = new Vector4<float>(numeric_limits<float>::epsilon(),
                                                numeric_limits<float>::epsilon(),
                                                numeric_limits<float>::epsilon(),
                                                numeric_limits<float>::epsilon());
  return *v;
}

const Vector4<float>& Vector4<float>::undefined() {
  static Vector4<float>* v = new Vector4<float>(numeric_limits<float>::quiet_NaN(),
                                                numeric_limits<float>::quiet_NaN(),
                                                numeric_limits<float>::quiet_NaN(),
                                                numeric_limits<float>::quiet_NaN());
  return *v;
}

const Vector4<float>& Vector4<float>::minusInfinity() {
  static Vector4<float>* v = new Vector4<float>(-numeric_limits<float>::infinity(),
                                                -numeric_limits<float>::infinity(),
                                                -numeric_limits<float>::infinity(),
                                                -numeric_limits<float>::infinity());
  return *v;
}

const Vector4<float>& Vector4<float>::plusInfinity() {
  static Vector4<float>* v = new Vector4<float>(numeric_limits<float>::infinity(),
                                                numeric_limits<float>::infinity(),
                                                numeric_limits<float>::infinity(),
                                                numeric_limits<float>::infinity());
  return *v;
}

#endif
