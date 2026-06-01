#include "core/util/ScopedTimer.h"

namespace core::util {
  ScopedTimer::ScopedTimer(double* seconds)
      : m_seconds(seconds),
        m_start(std::chrono::steady_clock::now()),
        m_running(seconds != nullptr) {
  }

  ScopedTimer::~ScopedTimer() {
    stop();
  }

  void ScopedTimer::stop() {
    if (!m_running) {
      return;
    }

    *m_seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
    m_running = false;
  }
}
