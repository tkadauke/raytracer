#pragma once

#include <chrono>

namespace core::util {
  class ScopedTimer {
  public:
    explicit ScopedTimer(double* seconds);
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

    void stop();

  private:
    double* m_seconds;
    std::chrono::steady_clock::time_point m_start;
    bool m_running;
  };
}
