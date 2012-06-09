#include "Color.h"

#ifdef __SSE3__

const Color<double>& Color<double>::black() {
  Color<double>* c = new Color<double>();
  return *c;
}

const Color<double>& Color<double>::white() {
  Color<double>* c = new Color<double>(1, 1, 1);
  return *c;
}

#endif
