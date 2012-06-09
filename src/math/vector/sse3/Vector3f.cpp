#include "math/Vector.h"

#ifdef __SSE__

using namespace std;

const Vector3<float>& Vector3<float>::null() {
  static Vector3<float>* v = new Vector3<float>();
  return *v;
}

const Vector3<float>& Vector3<float>::epsilon() {
  static Vector3<float>* v = new Vector3<float>(numeric_limits<float>::epsilon(),
                                                numeric_limits<float>::epsilon(),
                                                numeric_limits<float>::epsilon());
  return *v;
}

const Vector3<float>& Vector3<float>::undefined() {
  static Vector3<float>* v = new Vector3<float>(numeric_limits<float>::quiet_NaN(),
                                                numeric_limits<float>::quiet_NaN(),
                                                numeric_limits<float>::quiet_NaN());
  return *v;
}

const Vector3<float>& Vector3<float>::minusInfinity() {
  static Vector3<float>* v = new Vector3<float>(-numeric_limits<float>::infinity(),
                                                -numeric_limits<float>::infinity(),
                                                -numeric_limits<float>::infinity());
  return *v;
}

const Vector3<float>& Vector3<float>::plusInfinity() {
  static Vector3<float>* v = new Vector3<float>(numeric_limits<float>::infinity(),
                                                numeric_limits<float>::infinity(),
                                                numeric_limits<float>::infinity());
  return *v;
}

#endif


