#pragma once

#include <cstdint>
#include <limits>

// Thread-local PCG32 PRNG — each thread owns its own state so concurrent
// render threads never share state or contend on a lock.
// Algorithm: O'Neill 2014, "PCG: A Family of Simple Fast Space-Efficient
// Statistically Good Algorithms for Random Number Generation."
namespace detail {
  struct PCG32State {
    uint64_t state = 0x853c49e6748fea9bULL;
    uint64_t inc = 0xda3e39cb94b95bdbULL;

    uint32_t next() noexcept {
      uint64_t old = state;
      state = old * 6364136223846793005ULL + inc;
      uint32_t xs = uint32_t(((old >> 18u) ^ old) >> 27u);
      uint32_t rot = uint32_t(old >> 59u);
      return (xs >> rot) | (xs << ((-rot) & 31u));
    }

    void reseed(uint64_t initstate, uint64_t initseq = 1u) noexcept {
      state = 0u;
      inc = (initseq << 1u) | 1u;
      next();
      state += initstate;
      next();
    }
  };

  inline PCG32State& thread_rng() noexcept {
    thread_local PCG32State rng;
    return rng;
  }
}

/**
  * Seeds the calling thread's PRNG for deterministic output.
  */
inline void seed(uint64_t s) noexcept {
  detail::thread_rng().reseed(s);
}

/**
  * @returns true if @p what is within @p epsilon of @p value, false otherwise.
  */
template<class T>
inline bool isAlmost(const T& what, const T& value,
                     const T& epsilon = std::numeric_limits<T>::epsilon() * 10.0) {
  return what - epsilon <= value && value <= what + epsilon;
}

/**
  * @returns true if @p what is within @p epsilon of 0, false otherwise.
  */
template<class T>
inline bool isAlmostZero(const T& value,
                         const T& epsilon = std::numeric_limits<T>::epsilon() * 10.0) {
  return isAlmost(T(0), value, epsilon);
}

/**
  * @returns a random number in the interval [lower, upper).
  */
template<class T>
inline T random(T lower, T upper) {
  T r = T(detail::thread_rng().next()) * T(1.0 / 4294967296.0);
  return lower + r * (upper - lower);
}

/**
  * @returns a random number in [0, upper).
  */
template<class T>
inline T random(T upper) {
  return random(T(), upper);
}

/**
  * @returns a random integer in [0, upper).
  */
inline int random(int upper) {
  return int(detail::thread_rng().next() % uint32_t(upper));
}
