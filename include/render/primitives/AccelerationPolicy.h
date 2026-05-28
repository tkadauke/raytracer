#pragma once

#include <cstddef>
#include <string>

#include "render/primitives/SpatialIndexFactory.h"

namespace render {
  /**
    * User-facing acceleration request. Automatic is deliberately a policy
    * choice rather than an alias for a concrete index.
    */
  enum class AccelerationMode {
    Automatic,
    Linear,
    Grid,
    BVH,
  };

  /**
    * Scene facts that may feed acceleration selection. Keep this small and
    * testable; benchmark-backed heuristics can add fields as needed.
    */
  struct AccelerationAnalysis {
    std::size_t boundedPrimitiveCount{0};
  };

  /**
    * Observable result of applying an AccelerationPolicy.
    */
  struct AccelerationDecision {
    AccelerationMode requestedMode{AccelerationMode::Automatic};
    SpatialIndexKind spatialIndexKind{SpatialIndexKind::Grid};
    const char* reason{"automatic_conservative_grid"};
  };

  class AccelerationPolicy {
  public:
    static AccelerationPolicy automatic();
    static AccelerationPolicy manual(SpatialIndexKind kind);

    explicit AccelerationPolicy(AccelerationMode mode = AccelerationMode::Automatic);

    [[nodiscard]] AccelerationMode mode() const;
    [[nodiscard]] AccelerationDecision choose(const AccelerationAnalysis& analysis) const;

  private:
    AccelerationMode m_mode;
  };

  [[nodiscard]] const char* toString(AccelerationMode mode);
  [[nodiscard]] const char* toString(SpatialIndexKind kind);
  [[nodiscard]] std::string diagnosticString(const AccelerationDecision& decision);
}
