#pragma once

template<class Derived>
class InequalityOperator {
public:
  inline bool operator!=(const Derived& other) {
    if (this == &other)
      return false;
    return !(derived() == other);
  }
  
private:
  inline Derived& derived() {
    return static_cast<Derived&>(*this);
  }
};
