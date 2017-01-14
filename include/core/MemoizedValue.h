#pragma once

/**
  * Generic class that wraps any default-constructible type, so it can be
  * initialized lazily.
  */
template<class T>
class MemoizedValue {
public:
  /**
    * Default constructor. Default constructs the value and sets it to
    * uninitialized.
    */
  inline MemoizedValue()
    : m_initialized(false)
  {
  }
  
  /**
    * Value constructor. Initializes the value with the given @p value and sets
    * the object state to initialized.
    */
  inline explicit MemoizedValue(const T& value)
    : m_initialized(true),
      m_value(value)
  {
  }
  
  /**
    * @returns true if the object is uninitialized, false otherwise. This makes
    *   it easy to guard against uninitialized values.
    * 
    * @code
    * MemoizedValue<int> mv;
    * //...
    * if (!mv) {
    *   mv = 42;
    * }
    * @endcode
    */
  inline bool operator!() const {
    return !m_initialized;
  }
  
  /**
    * Value conversion operator. This allows a MemoizedValue<T> instance to be
    * used where a T instance is expected.
    * 
    * Warning: there is no protection against uninitialized values here. If you
    * use this method, you have to guard against it yourself.
    * 
    * @see operator!().
    * 
    * @returns a const-reference to the memoized value.
    */
  inline operator const T&() const {
    return m_value;
  }
  
  /**
    * @returns true if the value is initialized, false otherwise.
    */
  inline bool isInitialized() const {
    return m_initialized;
  }
  
  /**
    * Gets the value out of the MemoizedValue container.
    * 
    * Warning: there is no protection against uninitialized values here. If you
    * use this method, you have to guard against it yourself.
    * 
    * @see operator!().
    * 
    * @returns a const-reference to the memoized value.
    */
  inline const T& value() const {
    return m_value;
  }
  
  /**
    * Gets the value out of the MemoizedValue container.
    * 
    * Warning: there is no protection against uninitialized values here. If you
    * use this method, you have to guard against it yourself.
    * 
    * @see operator!().
    * 
    * @returns a mutable reference to the memoized value.
    */
  inline T& value() {
    return m_value;
  }
  
  /**
    * Sets the memoized value to @p value. This method can be called multiple
    * times.
    * 
    * @returns a const-reference to the memoized value.
    */
  inline const T& operator=(const T& value) {
    m_value = value;
    m_initialized = true;
    return value;
  }
  
  /**
    * Resets the initialized indicator, but leaves the value alone.
    */
  inline void reset() {
    m_initialized = false;
  }
  
private:
  bool m_initialized;
  T m_value;
};
