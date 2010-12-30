#ifndef IN_PLACE_SET_OPERATORS_H
#define IN_PLACE_SET_OPERATORS_H

template<class Derived>
class InPlaceSetOperators {
public:
  inline Derived& operator|=(const Derived& other) {
    derived() = derived() | other;
    return derived();
  }
  
  inline Derived& operator&=(const Derived& other) {
    derived() = derived() & other;
    return derived();
  }

private:
  inline Derived& derived() {
    return static_cast<Derived&>(*this);
  }
};

#endif
