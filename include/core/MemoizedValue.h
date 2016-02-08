#pragma once

template<class T>
class MemoizedValue {
public:
  inline MemoizedValue()
    : m_initialized(false) {}
  inline MemoizedValue(const T& value)
    : m_initialized(true), m_value(value) {}
  
  inline bool operator!() const { return !m_initialized; }
  inline operator const T&() const { return m_value; }
  
  inline bool isInitialized() const { return m_initialized; }
  inline const T& value() const { return m_value; }
  inline T& value() { return m_value; }
  
  inline const T& operator=(const T& value) {
    m_value = value;
    m_initialized = true;
    return value;
  }
  
  inline void reset() {
    m_initialized = false;
  }
  
private:
  bool m_initialized;
  T m_value;
};
