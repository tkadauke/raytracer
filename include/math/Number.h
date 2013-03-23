#ifndef NUMBER_H
#define NUMBER_H

template<class T>
inline bool isAlmost(const T& what, const T& value) {
  static const T epsilon = std::numeric_limits<T>::epsilon();
  return what - epsilon < value && value < what + epsilon;
}

template<class T>
inline bool isAlmostZero(const T& value) {
  return isAlmost(T(0), value);
}

#endif
