#pragma once

/**
  * Wrapper class to turn a default-constructible class into a singleton.
  */
template<class T>
class Singleton {
public:
  /**
    * @returns a mutable reference to the wrapped single instance, if it already
    *   exists. Otherwise, the instance is created and returned.
    */
  inline static T& self() {
    if (!s_instance) {
      s_instance = new T;
    }
    return *s_instance;
  }

private:
  static T* s_instance;
};

template<class T>
T* Singleton<T>::s_instance = nullptr;
