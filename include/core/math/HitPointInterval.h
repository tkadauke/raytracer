#pragma once

#include "core/math/HitPoint.h"
#include "core/math/Matrix.h"
#include <utility>
#include <vector>
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
  
  typedef std::vector<HitPointWrapper> HitPoints;
  
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
    m_hitPoints.push_back(HitPointWrapper(hitPoint, in));
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
  HitPointInterval operator-(const HitPointInterval& other) const;
  
  inline HitPointInterval transform(const Matrix4d& pointMatrix, const Matrix3d& normalMatrix) {
    HitPointInterval result;
    for (const auto& i : m_hitPoints) {
      result.add(i.point.transform(pointMatrix, normalMatrix), i.in);
    }
    return result;
  }
  
  const HitPoint& min() const {
    if (m_hitPoints.empty())
      return HitPoint::undefined();
    return m_hitPoints.begin()->point;
  }
  
  const HitPoint& minWithPositiveDistance() const {
    for (const auto& i : m_hitPoints) {
      if (i.point.distance() > 0) {
        return i.point;
      }
    }
    return HitPoint::undefined();
  }
  
  const HitPoint& max() const {
    if (m_hitPoints.empty())
      return HitPoint::undefined();
    return m_hitPoints.rbegin()->point;
  }
  
private:
  HitPoints m_hitPoints;
};

std::ostream& operator<<(std::ostream& os, const HitPointInterval& interval);
