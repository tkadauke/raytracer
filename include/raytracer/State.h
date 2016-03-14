#pragma once

#include "core/math/HitPoint.h"

namespace raytracer {
  class Primitive;
  
  class State {
  public:
    State()
      : traceEvents(false),
        recursionDepth(0),
        maxRecursionDepth(0),
        intersectionHits(0),
        intersectionMisses(0),
        shadowIntersectionHits(0),
        shadowIntersectionMisses(0)
    {
    }
    
    inline void startTrace() {
      events = std::make_unique<std::list<std::string>>();
      traceEvents = true;
    }
    
    inline void recordEvent(const std::string& event) {
      if (traceEvents) {
        std::string indent;
        for (int i = 0; i != recursionDepth; i++)
          indent += "  ";
        events->push_back(indent + event);
      }
    }
    
    inline void recurseIn() {
      recursionDepth++;
      maxRecursionDepth = std::max(maxRecursionDepth, recursionDepth);
    }
    
    inline void recurseOut() {
      recursionDepth--;
    }
    
    inline void hit(const std::string& info) {
      intersectionHits++;
      recordEvent("Intersection hit: " + info);
    }
    
    inline void miss(const std::string& info) {
      intersectionMisses++;
      recordEvent("Intersection miss: " + info);
    }
    
    inline void shadowHit(const std::string& info) {
      shadowIntersectionHits++;
      recordEvent("Shadow intersection hit: " + info);
    }
    
    inline void shadowMiss(const std::string& info) {
      shadowIntersectionMisses++;
      recordEvent("Shadow intersection miss: " + info);
    }
    
    bool traceEvents;
    int recursionDepth;
    int maxRecursionDepth;
    int intersectionHits;
    int intersectionMisses;
    int shadowIntersectionHits;
    int shadowIntersectionMisses;
    
    HitPoint hitPoint;
    
    std::unique_ptr<std::list<std::string>> events;
  };
}
