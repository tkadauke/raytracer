#pragma once

/**
  * Wrapper class to turn a default-constructible class into a singleton.
  *
  * Uses the Meyers' singleton pattern — a function-local static — which the
  * C++11 standard guarantees is initialized exactly once even when multiple
  * threads race the first call. The previous heap-allocated implementation
  * had a TOCTOU race on the unsynchronised null-check (two threads could
  * both pass `if (!s_instance)` and create two instances, leaking one) and
  * a permanent memory leak at program termination.
  */
template<class T>
class Singleton {
public:
  /**
    * @returns a mutable reference to the wrapped single instance, creating
    *   it on the first call.
    */
  inline static T& self() {
    static T s_instance;
    return s_instance;
  }
};
