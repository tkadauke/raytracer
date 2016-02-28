#pragma once

template<class T>
class Singleton {
public:
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
