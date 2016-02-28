#pragma once

#include "core/NullType.h"

/**
  * Enables simple static conditional type calculations. If cond is true, then
  * StaticIf::Result equals Cons, otherwise it equals Alt.
  * 
  * @code
  * typedef typename StaticIf<
  *   sizeof(int) == 4,
  *   long int,
  *   int
  * >::Result int64;
  * @endcode
  */
template<bool Cond, class Cons, class Alt>
struct StaticIf {
  /**
    * The result of the static type calculation.
    */
  typedef NullType Result;
};

/**
  * Specialization for true condition.
  */
template<class Cons, class Alt>
struct StaticIf<true, Cons, Alt> {
  typedef Cons Result;
};

/**
  * Specialization for false condition.
  */
template<class Cons, class Alt>
struct StaticIf<false, Cons, Alt> {
  typedef Alt Result;
};

/**
  * Static boolean predicate that tests whether the two given type parameters
  * are equal. The member Result evaluates to true if they are equal, false
  * otherwise.
  */
template<class Left, class Right>
struct TypesEqual {
  /**
    * Evaluates to true if Left == Right, false otherwise.
    */
  static const bool Result = false;
};

/**
  * Specialization for true condition.
  */
template<class T>
struct TypesEqual<T, T> {
  static const bool Result = true;
};

/**
  * Static boolean predicate that tests whether the given type is the NullType.
  */
template<class T>
struct IsNullType {
  /**
    * Evaluates to true, if T == NullType, false otherwise.
    */
  static const bool Result = TypesEqual<T, NullType>::Result;
};
