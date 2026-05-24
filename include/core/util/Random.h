#pragma once

#include <algorithm>

#include "core/math/Number.h"

/**
 * In-place Fisher–Yates shuffle of `[first, last)`.
 *
 * Provided as a project-local alternative to `std::random_shuffle`,
 * which was deprecated in C++14 and removed in C++17. The standard
 * replacement (`std::shuffle`) requires a caller-supplied URBG —
 * this template wraps a simpler interface for the existing call
 * sites that just want "randomise this range" without picking a
 * generator.
 *
 * The swap-index is chosen via modulo on a 32-bit uniform sample,
 * which is biased for most ranges (the modulo isn't uniform when
 * 2^32 doesn't divide evenly by i+1). The bias is small enough not
 * to matter for the existing call sites (sample-set permutation,
 * not cryptographic shuffling). If a caller needs unbiased
 * shuffling, switch to `std::shuffle` with a URBG locally.
 */
template<class RandomIt>
void random_shuffle(RandomIt first, RandomIt last) {
  typename std::iterator_traits<RandomIt>::difference_type i, n;
  n = last - first;
  for (i = n - 1; i > 0; --i) {
    using std::swap;
    swap(first[i], first[::random(int(i + 1))]);
  }
}
