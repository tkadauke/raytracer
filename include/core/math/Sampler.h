#pragma once

#include "core/math/Number.h"

/**
  * Provides a coin flip mechanism that returns true or false, according to a
  * given probability rate.
  */
class Sampler {
public:
  /**
    * Constructor.
    * 
    * @param invSampleRate is the inverse of the sample rate \f$r\f$, i.e.
    *   \f$\frac{1}{r}\f$. It is inverse, because then it can be expressed as an
    *   int. Also, it is more intuitive to specify "1 in 1000" vs. "0.001".
    */
  explicit Sampler(int invSampleRate)
    : m_invSampleRate(invSampleRate)
  {
  }

  /**
    * @returns true with a probability of one divided by the sample rate, false
    *   otherwise.
    */
  bool coinFlip() const {
    return random(m_invSampleRate) == 0;
  }
  
  /**
    * Executes func, if coinFlip() returns true, otherwise does nothing.
    */
  void coinFlip(const std::function<void()>& func) const {
    if (coinFlip()) {
      func();
    }
  }

private:
  int m_invSampleRate;
};
