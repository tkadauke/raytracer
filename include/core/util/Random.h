#pragma once

#include <random>
#include <algorithm>

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
 * The shuffle uses `std::rand() % (i+1)` to pick swap indices,
 * which is biased for most ranges (the modulo isn't uniform when
 * `RAND_MAX` doesn't divide evenly). The bias is small enough not
 * to matter for the existing call sites (sample-set permutation,
 * not cryptographic shuffling). If a caller needs unbiased
 * shuffling, switch to `std::shuffle` with a URBG locally.
 */
template<class RandomIt>
void random_shuffle(RandomIt first, RandomIt last)
{
    typename std::iterator_traits<RandomIt>::difference_type i, n;
    n = last - first;
    for (i = n-1; i > 0; --i) {
        using std::swap;
        swap(first[i], first[std::rand() % (i+1)]);
        // rand() % (i+1) isn't actually correct, because the generated number
        // is not uniformly distributed for most values of i. A correct implementation
        // will need to essentially reimplement C++11 std::uniform_int_distribution,
        // which is beyond the scope of this example.
    }
}
