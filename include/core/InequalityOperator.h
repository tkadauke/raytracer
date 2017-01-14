#pragma once

/**
  * Generic trait/mixin which implements the inequality operator in terms of
  * the equality operator. To use, derive from this class.
  * 
  * @tparam Derived the subclass that is being defined. This is required to
  *   access the equality operator of the subclass.
  */
template<class Derived>
class InequalityOperator {
public:
  /**
    * @returns true if this object is different from @p other, false otherwise.
    */
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
