#include "Color.h"

#ifdef __SSE__

const Color<float>& Color<float>::black() {
  Color<float>* c = new Color<float>();
  return *c;
}

const Color<float>& Color<float>::white() {
  Color<float>* c = new Color<float>(1, 1, 1);
  return *c;
}

#endif
