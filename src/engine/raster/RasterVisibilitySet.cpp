#include "engine/raster/RasterVisibilitySet.h"

#include <utility>

namespace engine::raster {
  void RasterVisibilitySet::addVisibleLeaf(std::size_t triangleCount) {
    addVisibleLeaf(triangleCount, triangleCount);
  }

  void RasterVisibilitySet::addVisibleLeaf(std::size_t triangleCount, std::size_t faceCount) {
    m_leaves.push_back(LeafDecision{true, RejectionReason::Frustum, triangleCount, faceCount});
  }

  void RasterVisibilitySet::addRejectedLeaf(RejectionReason reason, std::size_t triangleCount) {
    addRejectedLeaf(reason, triangleCount, triangleCount);
  }

  void RasterVisibilitySet::addRejectedLeaf(RejectionReason reason, std::size_t triangleCount,
                                            std::size_t faceCount) {
    m_leaves.push_back(LeafDecision{false, reason, triangleCount, faceCount});
  }

  void RasterVisibilitySet::setVisibleLeafOrder(std::vector<std::size_t> leafIndices) {
    m_visibleLeafOrder = std::move(leafIndices);
  }

  bool RasterVisibilitySet::leafVisible(std::size_t leafIndex) const {
    if (leafIndex >= m_leaves.size()) {
      return true;
    }
    return m_leaves[leafIndex].visible;
  }

  bool RasterVisibilitySet::hasVisibleLeafOrder() const {
    return !m_visibleLeafOrder.empty();
  }

  const std::vector<std::size_t>& RasterVisibilitySet::visibleLeafOrder() const {
    return m_visibleLeafOrder;
  }

  std::size_t RasterVisibilitySet::leafCount() const {
    return m_leaves.size();
  }

  std::size_t RasterVisibilitySet::leafFaceCount(std::size_t leafIndex) const {
    if (leafIndex >= m_leaves.size()) {
      return 0;
    }
    return m_leaves[leafIndex].faceCount;
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
