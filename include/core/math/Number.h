#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

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

  inline uint64_t entropy_seed() {
    std::random_device rd;
    uint64_t hi = static_cast<uint64_t>(rd()) << 32u;
    return hi ^ static_cast<uint64_t>(rd());
  }

  inline PCG32State make_entropy_seeded_rng() {
    PCG32State rng;
    rng.reseed(entropy_seed(), entropy_seed());
    return rng;
  }

  inline PCG32State& thread_rng() {
    thread_local PCG32State rng = make_entropy_seeded_rng();
    return rng;
  }
}

/**
  * Seeds the calling thread's PRNG for deterministic output.
  *
  * Random generation is thread-local. Calling `seed()` affects only the
  * current thread and does not reseed other render worker threads. The
  * project no longer uses `std::rand`, so `std::srand` has no effect on
  * `random()` or `Range::random()`.
  */
inline void seed(uint64_t s) {
  detail::thread_rng().reseed(s);
}

/**
  * Temporarily seeds the calling thread's PRNG and restores its previous
  * state when the scope exits.
  */
class RandomSeedScope {
public:
  explicit RandomSeedScope(uint64_t s)
      : m_previous(detail::thread_rng()) {
    seed(s);
  }

  RandomSeedScope(const RandomSeedScope&) = delete;
  RandomSeedScope& operator=(const RandomSeedScope&) = delete;

  ~RandomSeedScope() {
    detail::thread_rng() = m_previous;
  }

private:
  detail::PCG32State m_previous;
};

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

/**
  * @returns the absolute value of @p value clamped to at least
  * \f$\epsilon\f$ — the smallest positive representable value for type T.
  * Used by geometric setters to prevent zero or negative extents.
  */
template<class T>
inline T positiveExtent(const T& value) {
  return std::max(std::abs(value), std::numeric_limits<T>::epsilon());
}

/**
  * @returns @p value clamped to be no less than @p minimum. Used by
  * setters that only need a floor (e.g. sample counts, biases, radii).
  */
template<class T>
inline T atLeast(T minimum, T value) {
  return std::max(minimum, value);
}

/**
  * @returns @p value clamped to be no less than @p minimum, treating a
  * non-finite @p value as if it were @p minimum. Used by setters that
  * accept externally supplied doubles (e.g. from JSON) which may be NaN
  * or infinite.
  */
inline double finiteAtLeast(double minimum, double value) {
  return std::isfinite(value) ? std::max(minimum, value) : minimum;
}

/**
  * @returns the largest power of two that is at most @p n, e.g. 1 for
  * inputs in [1, 2), 2 for [2, 4), 4 for [4, 8). Used by the interlaced
  * view-plane iterators to pick their initial pixel-block size.
  */
inline int largestPowerOfTwoAtMost(int n) {
  return 1 << int(std::log(n));
}
