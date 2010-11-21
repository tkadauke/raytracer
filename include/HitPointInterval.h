#ifndef HIT_POINT_INTERVAL_H
#define HIT_POINT_INTERVAL_H

#include "HitPoint.h"
#include <utility>
#include <list>
#include <limits>

class HitPointInterval {
public:
  class Pair : public std::pair<HitPoint, HitPoint> {
    typedef std::pair<HitPoint, HitPoint> Base;
  public:
    Pair(const HitPoint& first, const HitPoint& second)
      : Base(first.distance() < second.distance() ? first : second, first.distance() < second.distance() ? second : first)
    {
    }
  };
  
  typedef std::list<Pair> HitPointList;
  
  inline HitPointInterval() {}
  
  inline HitPointInterval(const Pair& section) {
    add(section);
  }
  
  inline HitPointInterval(const HitPoint& begin, const HitPoint& end) {
    add(Pair(begin, end));
  }
  
  inline void add(const Pair& section) {
    m_intervals.push_back(section);
  }
  
  inline void add(const HitPoint& hitPoint) {
    add(hitPoint, hitPoint);
  }
  
  inline void add(const HitPoint& first, const HitPoint& second) {
    add(Pair(first, second));
  }
  
  inline const HitPointList& intervals() const {
    return m_intervals;
  }
  
  inline HitPointInterval operator+(const HitPointInterval& other) const {
    HitPointInterval result;
    for (HitPointList::const_iterator i = intervals().begin(); i != intervals().end(); ++i) {
      result.add(*i);
    }
    
    for (HitPointList::const_iterator i = other.intervals().begin(); i != other.intervals().end(); ++i) {
      result.add(*i);
    }
    return result;
  }
  
  inline HitPointInterval operator|(const HitPointInterval& other) const {
    HitPointInterval result;
    HitPointInterval temp = *this + other;
    result.add(temp.min(), temp.max());
    return result;
  }
  
  const HitPoint& min() const {
    double distance = std::numeric_limits<double>::infinity();
    HitPoint const* result = &HitPoint::undefined;
    for (HitPointList::const_iterator i = m_intervals.begin(); i != m_intervals.end(); ++i) {
      if (i->first.distance() < distance) {
        distance = i->first.distance();
        result = &i->first;
      }
    }
    return *result;
  }
  
  const HitPoint& minWithPositiveDistance() const {
    double distance = std::numeric_limits<double>::infinity();
    HitPoint const* result = &HitPoint::undefined;
    for (HitPointList::const_iterator i = m_intervals.begin(); i != m_intervals.end(); ++i) {
      if (i->second.distance() > 0 && i->second.distance() < distance) {
        distance = i->second.distance();
        result = &i->second;
      }
      if (i->first.distance() > 0 && i->first.distance() < distance) {
        distance = i->first.distance();
        result = &i->first;
      }
    }
    return *result;
  }
  
  const HitPoint& max() const {
    double distance = - std::numeric_limits<double>::infinity();
    HitPoint const* result = &HitPoint::undefined;
    for (HitPointList::const_iterator i = m_intervals.begin(); i != m_intervals.end(); ++i) {
      if (i->second.distance() > distance) {
        distance = i->second.distance();
        result = &i->second;
      }
    }
    return *result;
  }
  
private:
  HitPointList m_intervals;
};

#endif
