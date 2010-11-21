#ifndef HIT_POINT_INTERVAL_H
#define HIT_POINT_INTERVAL_H

#include "HitPoint.h"
#include <utility>
#include <set>
#include <limits>
#include <iostream>

class HitPointInterval {
public:
  class HitPointWrapper {
  public:
    inline HitPointWrapper(const HitPoint& p, bool i)
      : point(p), in(i) {}
    
    inline bool operator<(const HitPointWrapper& other) const { return point.distance() < other.point.distance(); }
    
    HitPoint point;
    bool in;
  };
  
  typedef std::multiset<HitPointWrapper> HitPoints;
  
  inline HitPointInterval() {}
  
  inline HitPointInterval(const HitPoint& begin, const HitPoint& end) {
    add(begin, end);
  }
  
  inline void addIn(const HitPoint& hitPoint) {
    add(hitPoint, true);
  }
  
  inline void addOut(const HitPoint& hitPoint) {
    add(hitPoint, false);
  }
  
  inline void add(const HitPoint& hitPoint, bool in) {
    m_hitPoints.insert(HitPointWrapper(hitPoint, in));
  }
  
  inline void add(const HitPoint& hitPoint) {
    add(hitPoint, hitPoint);
  }

  inline void add(const HitPoint& first, const HitPoint& second) {
    addIn(first);
    addOut(second);
  }
  
  inline const HitPoints& points() const {
    return m_hitPoints;
  }
  
  inline bool empty() const {
    return m_hitPoints.empty();
  }
  
  HitPointInterval operator|(const HitPointInterval& other) const;
  HitPointInterval operator&(const HitPointInterval& other) const;
  
  const HitPoint& min() const {
    double distance = std::numeric_limits<double>::infinity();
    HitPoint const* result = &HitPoint::undefined;
    for (HitPoints::const_iterator i = m_hitPoints.begin(); i != m_hitPoints.end(); ++i) {
      if (i->point.distance() < distance) {
        distance = i->point.distance();
        result = &i->point;
      }
    }
    return *result;
  }
  
  const HitPoint& minWithPositiveDistance() const {
    double distance = std::numeric_limits<double>::infinity();
    HitPoint const* result = &HitPoint::undefined;
    for (HitPoints::const_iterator i = m_hitPoints.begin(); i != m_hitPoints.end(); ++i) {
      if (i->point.distance() > 0 && i->point.distance() < distance) {
        distance = i->point.distance();
        result = &i->point;
      }
    }
    return *result;
  }
  
  const HitPoint& max() const {
    double distance = - std::numeric_limits<double>::infinity();
    HitPoint const* result = &HitPoint::undefined;
    for (HitPoints::const_iterator i = m_hitPoints.begin(); i != m_hitPoints.end(); ++i) {
      if (i->point.distance() > distance) {
        distance = i->point.distance();
        result = &i->point;
      }
    }
    return *result;
  }
  
private:
  HitPoints m_hitPoints;
};

std::ostream& operator<<(std::ostream& os, const HitPointInterval& interval);

#endif
