#pragma once

#include <limits>
#include <random>

template<class T>
inline bool isAlmost(const T& what, const T& value, const T& epsilon = std::numeric_limits<T>::epsilon()) {
  return what - epsilon < value && value < what + epsilon;
}

template<class T>
inline bool isAlmostZero(const T& value, const T& epsilon = std::numeric_limits<T>::epsilon()) {
  return isAlmost(T(0), value, epsilon);
}

inline int randomInt(int limit) {
  return std::rand() % limit;
}
