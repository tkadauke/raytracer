#pragma once

#include "core/math/HitPoint.h"
#include "raytracer/Object.h"

namespace raytracer {
  class Primitive;

  class State {
  public:
    inline State()
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

    inline void recordEvent(const Object* obj, const std::string& event) {
      if (traceEvents) {
        std::string indent;
        for (int i = 0; i != recursionDepth; i++)
          indent += "  ";

        if (obj) {
          events->push_back(indent + obj->name() + ": " + event);
        } else {
          events->push_back(indent + event);
        }
      }
    }

    inline void recurseIn() {
      recursionDepth++;
      maxRecursionDepth = std::max(maxRecursionDepth, recursionDepth);
    }

    inline void recurseOut() {
      recursionDepth--;
    }

    inline void hit(const Object* obj, const std::string& info) {
      intersectionHits++;
      recordEvent(obj, "Intersection hit: " + info);
    }

    inline void miss(const Object* obj, const std::string& info) {
      intersectionMisses++;
      recordEvent(obj, "Intersection miss: " + info);
    }

    inline void shadowHit(const Object* obj, const std::string& info) {
      shadowIntersectionHits++;
      recordEvent(obj, "Shadow intersection hit: " + info);
    }

    inline void shadowMiss(const Object* obj, const std::string& info) {
      shadowIntersectionMisses++;
      recordEvent(obj, "Shadow intersection miss: " + info);
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
