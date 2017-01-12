#include "core/Color.h"

#ifdef __SSE3__

const Color<double>& Color<double>::black() {
  static Color<double> c(0, 0, 0);
  return c;
}

const Color<double>& Color<double>::white() {
  static Color<double> c(1, 1, 1);
  return c;
}

const Color<double>& Color<double>::red() {
  static Color<double> c(1, 0, 0);
  return c;
}

const Color<double>& Color<double>::green() {
  static Color<double> c(0, 1, 0);
  return c;
}

const Color<double>& Color<double>::blue() {
  static Color<double> c(0, 0, 1);
  return c;
}

#endif
