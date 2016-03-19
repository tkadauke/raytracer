#include "core/math/Ray.h"

template<>
const double Ray<double>::epsilon = 0.0000001;

template<>
const float Ray<float>::epsilon = 0.0001;
