#pragma once

template<class Derived>
class InequalityOperator {
public:
  inline bool operator!=(const Derived& other) const {
    if (this == &other)
      return false;
    return !(derived() == other);
  }
  
private:
  inline const Derived& derived() const {
    return static_cast<const Derived&>(*this);
  }
};
