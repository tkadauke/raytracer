#include "core/Color.h"

#ifdef __SSE__

const Color<float>& Color<float>::black() {
  static Color<float> c(0, 0, 0);
  return c;
}

const Color<float>& Color<float>::white() {
  static Color<float> c(1, 1, 1);
  return c;
}

const Color<float>& Color<float>::red() {
  static Color<float> c(1, 0, 0);
  return c;
}

const Color<float>& Color<float>::green() {
  static Color<float> c(0, 1, 0);
  return c;
}

const Color<float>& Color<float>::blue() {
  static Color<float> c(0, 0, 1);
  return c;
}

#endif
