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

#endif
