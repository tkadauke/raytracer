#pragma once

#include <cstddef>
#include <cstring>
#include <iterator>
#include <new>
#include <type_traits>

/**
  * A fixed-inline-capacity vector that stores up to N elements directly inside
  * the object (no heap allocation) and falls back to heap only when the count
  * exceeds N.  Requires T to be trivially copyable so that std::memcpy can be
  * used for all moves and copies, keeping hot-path construction as cheap as a
  * scalar store.
  *
  * @tparam T              Element type. Must satisfy std::is_trivially_copyable.
  * @tparam InlineCapacity Number of elements to store inline before spilling to
  *                        heap. Choosing a value that covers the common case
  *                        (e.g. 4 for HitPointInterval) eliminates per-call
  *                        heap allocations on the hot path.
  */
template<typename T, std::size_t InlineCapacity>
class SmallVector {
  static_assert(std::is_trivially_copyable<T>::value,
                "SmallVector<T,N> requires a trivially-copyable element type");
  static_assert(std::is_trivially_destructible<T>::value,
                "SmallVector<T,N> requires a trivially-destructible element type");
  static_assert(InlineCapacity > 0, "InlineCapacity must be > 0");

public:
  using value_type = T;
  using size_type = std::size_t;
  using iterator = T*;
  using const_iterator = const T*;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // ---- construction / destruction ----------------------------------------

  SmallVector() noexcept
      : m_size(0),
        m_heap(nullptr),
        m_heapCap(0) {
  }

  SmallVector(const SmallVector& o)
      : m_size(o.m_size),
        m_heap(nullptr),
        m_heapCap(0) {
    if (o.m_heap) {
      m_heapCap = o.m_heapCap;
      m_heap = allocate(m_heapCap);
      std::memcpy(m_heap, o.m_heap, m_size * sizeof(T));
    } else {
      std::memcpy(m_inline, o.m_inline, m_size * sizeof(T));
    }
  }

  SmallVector(SmallVector&& o) noexcept
      : m_size(o.m_size),
        m_heap(nullptr),
        m_heapCap(0) {
    if (o.m_heap) {
      m_heap = o.m_heap;
      m_heapCap = o.m_heapCap;
      o.m_heap = nullptr;
      o.m_heapCap = 0;
    } else {
      std::memcpy(m_inline, o.m_inline, m_size * sizeof(T));
    }
    o.m_size = 0;
  }

  SmallVector& operator=(const SmallVector& o) {
    if (this == &o)
      return *this;
    freeHeap();
    m_size = o.m_size;
    if (o.m_heap) {
      m_heapCap = o.m_heapCap;
      m_heap = allocate(m_heapCap);
      std::memcpy(m_heap, o.m_heap, m_size * sizeof(T));
    } else {
      std::memcpy(m_inline, o.m_inline, m_size * sizeof(T));
    }
    return *this;
  }

  SmallVector& operator=(SmallVector&& o) noexcept {
    if (this == &o)
      return *this;
    freeHeap();
    m_size = o.m_size;
    if (o.m_heap) {
      m_heap = o.m_heap;
      m_heapCap = o.m_heapCap;
      o.m_heap = nullptr;
      o.m_heapCap = 0;
    } else {
      std::memcpy(m_inline, o.m_inline, m_size * sizeof(T));
    }
    o.m_size = 0;
    return *this;
  }

  ~SmallVector() {
    freeHeap();
  }

  // ---- modifiers ---------------------------------------------------------

  void push_back(const T& val) {
    if (!m_heap) {
      if (m_size < InlineCapacity) {
        std::memcpy(inlineData() + m_size, &val, sizeof(T));
        ++m_size;
        return;
      }
      spillToHeap();
    } else if (m_size >= m_heapCap) {
      growHeap();
    }
    std::memcpy(m_heap + m_size, &val, sizeof(T));
    ++m_size;
  }

  // ---- queries -----------------------------------------------------------

  bool empty() const noexcept {
    return m_size == 0;
  }
  std::size_t size() const noexcept {
    return m_size;
  }

  /** True when all elements live in the inline buffer (no heap allocation). */
  bool usingInlineStorage() const noexcept {
    return m_heap == nullptr;
  }

  // ---- iteration ---------------------------------------------------------

  iterator begin() noexcept {
    return data();
  }
  iterator end() noexcept {
    return data() + m_size;
  }
  const_iterator begin() const noexcept {
    return data();
  }
  const_iterator end() const noexcept {
    return data() + m_size;
  }
  reverse_iterator rbegin() noexcept {
    return reverse_iterator(end());
  }
  reverse_iterator rend() noexcept {
    return reverse_iterator(begin());
  }
  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }
  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }

private:
  // Inline storage: raw bytes, properly aligned for T.
  alignas(T) unsigned char m_inline[InlineCapacity * sizeof(T)];
  std::size_t m_size;
  T* m_heap;
  std::size_t m_heapCap;

  T* inlineData() noexcept {
    return reinterpret_cast<T*>(m_inline);
  }
  const T* inlineData() const noexcept {
    return reinterpret_cast<const T*>(m_inline);
  }

  T* data() noexcept {
    return m_heap ? m_heap : inlineData();
  }
  const T* data() const noexcept {
    return m_heap ? m_heap : inlineData();
  }

  static T* allocate(std::size_t n) {
    return static_cast<T*>(::operator new(n * sizeof(T)));
  }

  void freeHeap() noexcept {
    if (m_heap) {
      ::operator delete(m_heap);
      m_heap = nullptr;
      m_heapCap = 0;
    }
  }

  void spillToHeap() {
    std::size_t newCap = InlineCapacity * 2;
    T* buf = allocate(newCap);
    std::memcpy(buf, inlineData(), m_size * sizeof(T));
    m_heap = buf;
    m_heapCap = newCap;
  }

  void growHeap() {
    std::size_t newCap = m_heapCap * 2;
    T* buf = allocate(newCap);
    std::memcpy(buf, m_heap, m_size * sizeof(T));
    ::operator delete(m_heap);
    m_heap = buf;
    m_heapCap = newCap;
  }
};
