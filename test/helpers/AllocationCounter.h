#pragma once

#include <cstddef>

namespace testing {
  namespace allocations {
    class ScopedCounter {
    public:
      ScopedCounter();
      ScopedCounter(const ScopedCounter&) = delete;
      ScopedCounter& operator=(const ScopedCounter&) = delete;
      ~ScopedCounter();

      std::size_t count() const;
      void reset();

    private:
      bool m_previousEnabled;
      std::size_t m_previousCount;
    };
  }
}
