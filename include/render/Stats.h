#pragma once

// Per-render performance counters.
//
// The whole subsystem is gated on the RAYTRACER_ENABLE_STATS preprocessor
// macro, which the build sets when the user passes -DRAYTRACER_ENABLE_STATS
// =ON to CMake. With the macro undefined, every RAYTRACER_STATS_INC call
// expands to (void)0 and the counters singleton isn't even materialised —
// zero overhead in production builds.
//
// When enabled, each RAYTRACER_STATS_INC is an atomic fetch_add with
// memory_order_relaxed, which is the cheapest atomic operation modern CPUs
// support. Counters are global and process-wide; engine::raytracer::Raytracer::render
// resets them at the start of each render and dumps the JSON snapshot to
// stderr at the end.
//
// Cache-miss counters are intentionally out of scope — they need
// perf_event_open / Apple's `kperf` and platform-specific privileges;
// if you need them, run the binary under `perf stat`.

#include <atomic>
#include <cstdint>
#include <iosfwd>

namespace render {
  namespace stats {

#ifdef RAYTRACER_ENABLE_STATS

    class Counters {
    public:
      // Meyers' singleton; thread-safe initialisation per C++11.
      static Counters& instance() {
        static Counters s_instance;
        return s_instance;
      }

      // Atomic counters. memory_order_relaxed is fine — we only care about
      // the totals, not happens-before ordering with surrounding reads.
      std::atomic<std::uint64_t> raySphereIntersect{0};
      std::atomic<std::uint64_t> raySphereIntersects{0};
      std::atomic<std::uint64_t> rayBoxIntersects{0};
      std::atomic<std::uint64_t> gridTraversalSteps{0};

      void reset() {
        raySphereIntersect.store(0, std::memory_order_relaxed);
        raySphereIntersects.store(0, std::memory_order_relaxed);
        rayBoxIntersects.store(0, std::memory_order_relaxed);
        gridTraversalSteps.store(0, std::memory_order_relaxed);
      }

      // Writes a single-line JSON object to `out`. Counter values are
      // sampled with relaxed loads — which is fine because the dump
      // happens after threadPool->waitForDone() returns from render().
      void dumpJson(std::ostream& out) const;
    };

#define RAYTRACER_STATS_INC(field)                                             \
  ::render::stats::Counters::instance().field.fetch_add(                    \
      1, std::memory_order_relaxed)

#else  // !RAYTRACER_ENABLE_STATS

#define RAYTRACER_STATS_INC(field) ((void)0)

#endif  // RAYTRACER_ENABLE_STATS

  }  // namespace stats
}  // namespace render
