#include "render/Stats.h"

#ifdef RAYTRACER_ENABLE_STATS

#include <ostream>

namespace render {
  namespace stats {

    void Counters::dumpJson(std::ostream& out) const {
      out << "{"
          << "\"raySphereIntersect\":"
          << raySphereIntersect.load(std::memory_order_relaxed)
          << ",\"raySphereIntersects\":"
          << raySphereIntersects.load(std::memory_order_relaxed)
          << ",\"rayBoxIntersects\":"
          << rayBoxIntersects.load(std::memory_order_relaxed)
          << ",\"gridTraversalSteps\":"
          << gridTraversalSteps.load(std::memory_order_relaxed)
          << "}\n";
    }

  }  // namespace stats
}  // namespace render

#endif  // RAYTRACER_ENABLE_STATS
