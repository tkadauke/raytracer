#include "core/math/HitPointInterval.h"

HitPointInterval HitPointInterval::operator|(const HitPointInterval& other) const {
  HitPointInterval result;
  
  int depth = 0;
  HitPoints::const_iterator first = m_hitPoints.begin(), second = other.points().begin();
  while (first != m_hitPoints.end() || second != other.points().end()) {
    bool useFirst;
    if (first != m_hitPoints.end() && second != other.points().end()) {
      useFirst = *first < *second;
    } else {
      useFirst = first != m_hitPoints.end();
    }
    
    if (useFirst) {
      if (first->in) {
        if (depth == 0)
          result.addIn(first->point);
        depth++;
      } else {
        if (depth == 1)
          result.addOut(first->point);
        depth--;
      }
      ++first;
    } else {
      if (second->in) {
        if (depth == 0)
          result.addIn(second->point);
        depth++;
      } else {
        if (depth == 1)
          result.addOut(second->point);
        depth--;
      }
      ++second;
    }
  }
  
  return result;
}

HitPointInterval HitPointInterval::operator&(const HitPointInterval& other) const {
  HitPointInterval result;
  
  bool inFirst = false, inSecond = false;
  HitPoints::const_iterator first = m_hitPoints.begin(), second = other.points().begin();
  while (first != m_hitPoints.end() && second != other.points().end()) {
    if (*first < *second) {
      if (first->in) {
        if (inSecond)
          result.addIn(first->point);
        inFirst = true;
      } else {
        if (inSecond)
          result.addOut(first->point);
        inFirst = false;
      }
      ++first;
    } else {
      if (second->in) {
        if (inFirst)
          result.addIn(second->point);
        inSecond = true;
      } else {
        if (inFirst)
          result.addOut(second->point);
        inSecond = false;
      }
      ++second;
    }
  }
  
  return result;
}

HitPointInterval HitPointInterval::operator-(const HitPointInterval& other) const {
  HitPointInterval result;
  
  bool inFirst = false, inSecond = false;
  HitPoints::const_iterator first = m_hitPoints.begin(), second = other.points().begin();
  while (first != m_hitPoints.end() || second != other.points().end()) {
    bool useFirst;
    if (first != m_hitPoints.end() && second != other.points().end()) {
      useFirst = *first < *second;
    } else {
      useFirst = first != m_hitPoints.end();
    }
    
    if (useFirst) {
      if (first->in) {
        if (!inSecond)
          result.addIn(first->point);
        inFirst = true;
      } else {
        if (!inSecond)
          result.addOut(first->point);
        inFirst = false;
      }
      ++first;
    } else {
      if (second->in) {
        if (inFirst)
          result.addOut(second->point.swappedNormal());
        inSecond = true;
      } else {
        if (inFirst)
          result.addIn(second->point.swappedNormal());
        inSecond = false;
      }
      ++second;
    }
  }
  
  return result;
}

std::ostream& operator<<(std::ostream& os, const HitPointInterval& interval) {
  os << '[';
  for (const auto& i : interval.points()) {
    if (i.in)
      os << "(I)";
    else
      os << "(O)";
    os << i.point.distance();
  }
  os << ']';
  return os;
}
