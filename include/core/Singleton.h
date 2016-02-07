#ifndef SINGLETON_H
#define SINGLETON_H

template<class T>
class Singleton {
public:
  static T& self() {
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

#endif
