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

/**
  * Returns a random number in the interval defined by lower and upper.
  */
template<class T>
inline T random(T lower, T upper) {
  T r = T(std::rand()) / RAND_MAX;
  return lower + r * (upper - lower);
}

/**
  * Returns a random number smaller or equal to upper.
  */
template<class T>
inline T random(T upper) {
  return random(T(), upper);
}

inline int random(int upper) {
  return std::rand() % upper;
}
