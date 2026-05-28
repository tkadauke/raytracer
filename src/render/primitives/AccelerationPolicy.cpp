#include "render/primitives/AccelerationPolicy.h"

namespace render {
  namespace {
    AccelerationDecision manualDecision(AccelerationMode mode, SpatialIndexKind kind) {
      return AccelerationDecision{mode, kind, "manual_override"};
    }
  }

  AccelerationPolicy AccelerationPolicy::automatic() {
    return AccelerationPolicy(AccelerationMode::Automatic);
  }

  AccelerationPolicy AccelerationPolicy::manual(SpatialIndexKind kind) {
    switch (kind) {
      case SpatialIndexKind::Linear:
        return AccelerationPolicy(AccelerationMode::Linear);
      case SpatialIndexKind::Grid:
        return AccelerationPolicy(AccelerationMode::Grid);
      case SpatialIndexKind::BVH:
        return AccelerationPolicy(AccelerationMode::BVH);
    }

    return automatic();
  }

  AccelerationPolicy::AccelerationPolicy(AccelerationMode mode)
      : m_mode(mode) {
  }

  AccelerationMode AccelerationPolicy::mode() const {
    return m_mode;
  }

  AccelerationDecision AccelerationPolicy::choose(const AccelerationAnalysis&) const {
    switch (m_mode) {
      case AccelerationMode::Automatic:
        return AccelerationDecision{m_mode, SpatialIndexKind::Grid, "automatic_conservative_grid"};
      case AccelerationMode::Linear:
        return manualDecision(m_mode, SpatialIndexKind::Linear);
      case AccelerationMode::Grid:
        return manualDecision(m_mode, SpatialIndexKind::Grid);
      case AccelerationMode::BVH:
        return manualDecision(m_mode, SpatialIndexKind::BVH);
    }

    return AccelerationDecision{AccelerationMode::Automatic, SpatialIndexKind::Grid,
                                "automatic_conservative_grid"};
  }

  const char* toString(AccelerationMode mode) {
    switch (mode) {
      case AccelerationMode::Automatic:
        return "automatic";
      case AccelerationMode::Linear:
        return "linear";
      case AccelerationMode::Grid:
        return "grid";
      case AccelerationMode::BVH:
        return "bvh";
    }

    return "unknown";
  }

  const char* toString(SpatialIndexKind kind) {
    switch (kind) {
      case SpatialIndexKind::Linear:
        return "linear";
      case SpatialIndexKind::Grid:
        return "grid";
      case SpatialIndexKind::BVH:
        return "bvh";
    }

    return "unknown";
  }

  std::string diagnosticString(const AccelerationDecision& decision) {
    return std::string("requested=") + toString(decision.requestedMode) +
           " selected=" + toString(decision.spatialIndexKind) + " reason=" + decision.reason;
  }
}
