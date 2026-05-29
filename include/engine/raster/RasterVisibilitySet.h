#pragma once

#include <cstddef>
#include <vector>

namespace engine::raster {
  /**
    * Per-frame raster leaf visibility produced by a graph preprocessing pass.
    *
    * Leaf indices are traversal-order indices for the render scene during the
    * same graph execution. Out-of-range queries are treated as visible so a
    * mismatched or partial set cannot accidentally hide geometry.
    */
  class RasterVisibilitySet {
  public:
    enum class RejectionReason { Frustum };

    void addVisibleLeaf(std::size_t triangleCount);
    void addRejectedLeaf(RejectionReason reason, std::size_t triangleCount);

    bool leafVisible(std::size_t leafIndex) const;
    std::size_t leafCount() const;
    std::size_t inputTriangleCount() const;
    std::size_t visibleLeafCount() const;
    std::size_t visibleTriangleCount() const;
    std::size_t rejectedLeafCount() const;
    std::size_t rejectedTriangleCount() const;
    std::size_t rejectedLeafCount(RejectionReason reason) const;
    std::size_t rejectedTriangleCount(RejectionReason reason) const;

  private:
    struct LeafDecision {
      bool visible{true};
      RejectionReason rejectionReason{RejectionReason::Frustum};
      std::size_t triangleCount{0};
    };

    std::vector<LeafDecision> m_leaves;
  };
}
