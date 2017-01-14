#pragma once

/**
  * Generic trait/mixin which implements the inplace set operators |= and &= in
  * terms of the operators | and =. To use, derive from this class.
  * 
  * @tparam Derived the subclass that is being defined. This is required to
  *   access the operators of the subclass.
  */
template<class Derived>
class InPlaceSetOperators {
public:
  /**
    * Replaces this object with the set union of this and @p other.
    * 
    * @returns this object.
    */
  inline Derived& operator|=(const Derived& other) {
    derived() = derived() | other;
    return derived();
  }
  
  /**
    * Replaces this object with the set intersection of this and @p other.
    * 
    * @returns this object.
    */
  inline Derived& operator&=(const Derived& other) {
    derived() = derived() & other;
    return derived();
  }

private:
  inline Derived& derived() {
    return static_cast<Derived&>(*this);
  }
};
