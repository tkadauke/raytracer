#pragma once

#include "core/math/HitPoint.h"
#include "core/math/Matrix.h"
#include <vector>
#include <iostream>

/**
  * Represents one or more intersection intervals along a ray. Intervals are
  * described by an entering and exiting HitPoint. This class also supports
  * set operations on the intervals. With that, CSG operations are implemented
  * in the Raytracer.
  */
class HitPointInterval {
public:
  /**
    * Wrapper around HitPoint, which only adds whether or not the HitPoint is
    * entering or exiting the object.
    */
  class HitPointWrapper {
  public:
    /**
      * Constructs a HitPointWrapper around p.
      * 
      * @param i describes whether or not the point is entering an object.
      */
    inline HitPointWrapper(const HitPoint& p, bool i)
      : point(p),
        in(i)
    {
    }
    
    /**
      * Wrapper around the HitPoint::operator<().
      */
    inline bool operator<(const HitPointWrapper& other) const {
      return point.distance() < other.point.distance();
    }
    
    /**
      * The point.
      */
    HitPoint point;
    
    /**
      * True if this is an entering HitPoint, false otherwise.
      */
    bool in;
  };
  
  typedef std::vector<HitPointWrapper> HitPoints;
  
  /**
    * Constructs an empty HitPointInterval.
    */
  inline HitPointInterval() {}
  
  /**
    * Constructs a HitPointInterval with a single interval described by begin
    * and end.
    */
  inline HitPointInterval(const HitPoint& begin, const HitPoint& end) {
    add(begin, end);
  }
  
  /**
    * Adds an entering HitPoint to the interval.
    */
  inline void addIn(const HitPoint& hitPoint) {
    add(hitPoint, true);
  }
  
  /**
    * Adds an exiting HitPoint to the interval.
    */
  inline void addOut(const HitPoint& hitPoint) {
    add(hitPoint, false);
  }
  
  /**
    * Adds a HitPoint to this interval.
    * 
    * @param in if true, the HitPoint is set to entering.
    */
  inline void add(const HitPoint& hitPoint, bool in) {
    m_hitPoints.push_back(HitPointWrapper(hitPoint, in));
  }
  
  /**
    * Adds a zero-width interval that starts and ends with hitPoint.
    */
  inline void add(const HitPoint& hitPoint) {
    add(hitPoint, hitPoint);
  }

  /**
    * Adds an interval specified by first and second.
    */
  inline void add(const HitPoint& first, const HitPoint& second) {
    addIn(first);
    addOut(second);
  }
  
  /**
    * Adds an already wrapped hitpoint to this interval.
    */
  inline void add(const HitPointWrapper& hpw) {
    m_hitPoints.push_back(hpw);
  }
  
  /**
    * @returns all points describing all of the intervals.
    */
  inline const HitPoints& points() const {
    return m_hitPoints;
  }
  
  /**
    * @returns true if the HitPointInterval is empty, false otherwise.
    */
  inline bool empty() const {
    return m_hitPoints.empty();
  }
  
  /**
    * @returns a new interval that is the union of this interval and other.
    */
  HitPointInterval operator|(const HitPointInterval& other) const;
  
  /**
    * @returns a new interval that is the intersection of this interval and
    *   other.
    */
  HitPointInterval operator&(const HitPointInterval& other) const;
  
  /**
    * @returns a new interval that is the difference between this interval and
    *   other.
    */
  HitPointInterval operator-(const HitPointInterval& other) const;

  /**
    * @returns a new interval that is the composite between this interval and
    *   other.
    */
  HitPointInterval operator+(const HitPointInterval& other) const;

  /**
    * @returns a new interval that has only the first and last points. This
    *   is used to build unions of solid and flat primitives.
    */
  inline HitPointInterval merged() const {
    HitPointInterval result;
    result.add(min(), max());
    return result;
  }

  /**
    * @returns a new HitPointInterval with all points transformed with
    * pointMatrix, and all normals transformed with normalMatrix.
    */
  inline HitPointInterval transform(const Matrix4d& pointMatrix, const Matrix3d& normalMatrix) const {
    HitPointInterval result;
    for (const auto& i : m_hitPoints) {
      result.add(i.point.transform(pointMatrix, normalMatrix), i.in);
    }
    return result;
  }
  
  /**
    * Sets the primitive in all hit points in this interval to prim.
    */
  inline void setPrimitive(const raytracer::Primitive* prim) {
    for (auto& i : m_hitPoints) {
      i.point.setPrimitive(prim);
    }
  }
  
  /**
    * @returns the HitPoint in this interval with the smallest distance value.
    */
  inline const HitPoint& min() const {
    if (m_hitPoints.empty())
      return HitPoint::undefined();
    return m_hitPoints.begin()->point;
  }
  
  /**
    * @returns the HitPoint in this interval with the smallest distance value
    *   that is positive.
    */
  inline const HitPoint& minWithPositiveDistance() const {
    for (const auto& i : m_hitPoints) {
      if (i.point.distance() > 0) {
        return i.point;
      }
    }
    return HitPoint::undefined();
  }
  
  /**
    * @returns the HitPoint in this interval with the largest distance value.
    */
  inline const HitPoint& max() const {
    if (m_hitPoints.empty())
      return HitPoint::undefined();
    return m_hitPoints.rbegin()->point;
  }
  
private:
  HitPoints m_hitPoints;
};

/**
  * Outputs the interval to the given std::ostream.
  * 
  * @returns os.
  */
std::ostream& operator<<(std::ostream& os, const HitPointInterval& interval);
