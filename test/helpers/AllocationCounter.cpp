#include "test/helpers/AllocationCounter.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {
  std::atomic<std::size_t> g_allocationCount{0};
  thread_local bool g_countAllocations = false;

  void* allocate(std::size_t size) {
    if (g_countAllocations) {
      g_allocationCount.fetch_add(1, std::memory_order_relaxed);
    }

    if (size == 0) {
      size = 1;
    }

    if (void* ptr = std::malloc(size)) {
      return ptr;
    }

    throw std::bad_alloc();
  }
}

void* operator new(std::size_t size) {
  return allocate(size);
}

void* operator new[](std::size_t size) {
  return allocate(size);
}

void operator delete(void* ptr) noexcept {
  std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
  std::free(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
  std::free(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
  std::free(ptr);
}

namespace testing {
  namespace allocations {
    ScopedCounter::ScopedCounter()
        : m_previousEnabled(g_countAllocations),
          m_previousCount(g_allocationCount.exchange(0, std::memory_order_relaxed)) {
      g_countAllocations = true;
    }

    ScopedCounter::~ScopedCounter() {
      g_countAllocations = m_previousEnabled;
      g_allocationCount.store(m_previousCount, std::memory_order_relaxed);
    }

    std::size_t ScopedCounter::count() const {
      return g_allocationCount.load(std::memory_order_relaxed);
    }

    void ScopedCounter::reset() {
      g_allocationCount.store(0, std::memory_order_relaxed);
    }
  }
}
