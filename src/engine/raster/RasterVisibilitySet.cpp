#include "engine/raster/RasterVisibilitySet.h"

namespace engine::raster {
  void RasterVisibilitySet::addVisibleLeaf(std::size_t triangleCount) {
    m_leaves.push_back(LeafDecision{true, RejectionReason::Frustum, triangleCount});
  }

  void RasterVisibilitySet::addRejectedLeaf(RejectionReason reason, std::size_t triangleCount) {
    m_leaves.push_back(LeafDecision{false, reason, triangleCount});
  }

  bool RasterVisibilitySet::leafVisible(std::size_t leafIndex) const {
    if (leafIndex >= m_leaves.size()) {
      return true;
    }
    return m_leaves[leafIndex].visible;
  }

  std::size_t RasterVisibilitySet::leafCount() const {
    return m_leaves.size();
  }

  std::size_t RasterVisibilitySet::inputTriangleCount() const {
    std::size_t count = 0;
    for (const LeafDecision& leaf : m_leaves) {
      count += leaf.triangleCount;
    }
    return count;
  }

  std::size_t RasterVisibilitySet::visibleLeafCount() const {
    std::size_t count = 0;
    for (const LeafDecision& leaf : m_leaves) {
      if (leaf.visible) {
        ++count;
      }
    }
    return count;
  }

  std::size_t RasterVisibilitySet::visibleTriangleCount() const {
    std::size_t count = 0;
    for (const LeafDecision& leaf : m_leaves) {
      if (leaf.visible) {
        count += leaf.triangleCount;
      }
    }
    return count;
  }

  std::size_t RasterVisibilitySet::rejectedLeafCount() const {
    return leafCount() - visibleLeafCount();
  }

  std::size_t RasterVisibilitySet::rejectedTriangleCount() const {
    return inputTriangleCount() - visibleTriangleCount();
  }

  std::size_t RasterVisibilitySet::rejectedLeafCount(RejectionReason reason) const {
    std::size_t count = 0;
    for (const LeafDecision& leaf : m_leaves) {
      if (!leaf.visible && leaf.rejectionReason == reason) {
        ++count;
      }
    }
    return count;
  }

  std::size_t RasterVisibilitySet::rejectedTriangleCount(RejectionReason reason) const {
    std::size_t count = 0;
    for (const LeafDecision& leaf : m_leaves) {
      if (!leaf.visible && leaf.rejectionReason == reason) {
        count += leaf.triangleCount;
      }
    }
    return count;
  }
}
