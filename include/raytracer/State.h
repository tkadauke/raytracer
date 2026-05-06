#pragma once
#include <algorithm>
#include <list>
#include <memory>
#include <string>

#include "core/math/HitPoint.h"
#include "raytracer/Object.h"

namespace raytracer {
  class Primitive;

  /**
    * @brief Per-ray state passed through the recursive shading
    *        pipeline.
    *
    * One `State` is constructed per primary ray. It travels by
    * mutable reference through `Raytracer::rayColor` and every
    * `Material::shade` / BRDF / BTDF call below it, accumulating:
    *
    *  - **Recursion bookkeeping**: `recursionDepth` / `maxRecursionDepth`,
    *    incremented on entry to `rayColor` and decremented on exit.
    *    The renderer uses this to short-circuit at
    *    `Raytracer::setMaximumRecursionDepth(N)`.
    *  - **Intersection counters**: `numRays`,
    *    `intersectionHits`/`Misses`, `shadowIntersectionHits`/`Misses`.
    *    Used by the per-primitive intersect calls (e.g.
    *    `Sphere::intersect`) to record bookkeeping that surfaces in
    *    the GUI's "Render Stats" pane and in performance benchmarks.
    *  - **Final hit point**: `hitPoint`. After a top-level call to
    *    `Raytracer::rayState`, this is what the click-to-identify
    *    path in the example apps reads to surface "you clicked on
    *    this primitive at this 3D position."
    *  - **Optional event log** (`events`): when `startTrace()` is
    *    called, every `recordEvent`/`hit`/`miss`/`shadowHit`/
    *    `shadowMiss` call appends an indented string. Used by the
    *    `RefractingRayTracer` example and the `ShouldIncludeDirect-
    *    LightingOnTIRBranch` regression test in
    *    `TransparentMaterialTest.cpp` to assert that a particular
    *    branch of the shading pipeline executed.
    *
    * The default-constructed state has tracing disabled and all
    * counters at zero — fine for production renders. Tests that
    * need to introspect the trace must call `startTrace()` before
    * passing the state into `rayColor`.
    */
  class State {
  public:
    inline State()
      : traceEvents(false),
        numRays(0),
        recursionDepth(0),
        maxRecursionDepth(0),
        intersectionHits(0),
        intersectionMisses(0),
        shadowIntersectionHits(0),
        shadowIntersectionMisses(0),
        timeSample(0.0)
    {
    }

    /**
      * Enable event recording for this state. Allocates the
      * `events` list lazily so production states (which never call
      * this) carry no per-string overhead.
      */
    inline void startTrace() {
      events = std::make_unique<std::list<std::string>>();
      traceEvents = true;
    }

    /**
      * Append a one-line event to the trace, indented by the
      * current `recursionDepth` so a printed log mirrors the call
      * tree. Pass `obj == nullptr` for non-`Object`-attributed
      * events (e.g. the `Raytracer` itself recording recursion
      * truncation); otherwise prefixes with `obj->name() + ": "`.
      *
      * No-op when `traceEvents` is false.
      */
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

    /// Increment recursion depth + ray count. Pairs with
    /// `recurseOut`; called by `Raytracer::rayColor` on entry.
    inline void recurseIn() {
      recursionDepth++;
      numRays++;
      maxRecursionDepth = std::max(maxRecursionDepth, recursionDepth);
    }

    /// Decrement recursion depth. Pairs with `recurseIn`; called by
    /// `Raytracer::rayColor` on exit (via a `ScopeExit`).
    inline void recurseOut() {
      recursionDepth--;
    }

    /// Record an intersection hit: bumps `intersectionHits` and
    /// emits an event. Called from primitive `intersect` methods.
    inline void hit(const Object* obj, const std::string& info) {
      intersectionHits++;
      recordEvent(obj, "Intersection hit: " + info);
    }

    /// Record an intersection miss. Counterpart to `hit`.
    inline void miss(const Object* obj, const std::string& info) {
      intersectionMisses++;
      recordEvent(obj, "Intersection miss: " + info);
    }

    /// Record a shadow-ray hit. Tracked separately so cost analyses
    /// can break out direct-lighting visibility cost from primary
    /// + reflection / refraction cost.
    inline void shadowHit(const Object* obj, const std::string& info) {
      shadowIntersectionHits++;
      recordEvent(obj, "Shadow intersection hit: " + info);
    }

    /// Record a shadow-ray miss. Counterpart to `shadowHit`.
    inline void shadowMiss(const Object* obj, const std::string& info) {
      shadowIntersectionMisses++;
      recordEvent(obj, "Shadow intersection miss: " + info);
    }

    /// Whether `events` is being populated. Set by `startTrace()`.
    bool traceEvents;

    /// Total `rayColor` invocations along this path so far
    /// (primary + recursive).
    int numRays;

    /// Current recursion depth — bumped by `recurseIn`, dropped by
    /// `recurseOut`. Compared against the renderer's max.
    int recursionDepth;

    /// Peak `recursionDepth` seen during this trace. Useful for
    /// per-pixel "how deep did we go" diagnostics.
    int maxRecursionDepth;

    /// Aggregate intersection-test counters.
    int intersectionHits;
    int intersectionMisses;
    int shadowIntersectionHits;
    int shadowIntersectionMisses;

    /// The most recent (or final, after a top-level
    /// `Raytracer::rayState`) hit point along this trace.
    HitPoint hitPoint;

    /// Time sample for this primary ray's tree, in `[0, 1)`. Set
    /// once by `Camera::render` (drawn from the sample stream's
    /// 1D dimension) and inherited by every recursive sub-ray —
    /// the world is in a fixed configuration during a single
    /// primary ray's tree, so reflections / refractions share the
    /// time. Animatable primitives (`Instance` with non-zero
    /// `velocity`) read this to interpolate between configurations
    /// at intersect time.
    double timeSample;

    /// Optional indent-formatted event log. Allocated lazily by
    /// `startTrace()`; null when tracing is off.
    std::unique_ptr<std::list<std::string>> events;
  };
}
