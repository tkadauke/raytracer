#include "engine/raster/RasterVisibilitySet.h"

#include <algorithm>
#include <set>
#include <utility>

namespace engine::raster {
  bool RasterVisibilitySet::TileGrid::enabled() const {
    return width > 0 && height > 0 && tileWidth > 0 && tileHeight > 0 && columns > 0 && rows > 0;
  }

  std::size_t RasterVisibilitySet::TileGrid::tileCount() const {
    if (!enabled()) {
      return 0;
    }
    return static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows);
  }

  void RasterVisibilitySet::addVisibleLeaf(std::size_t triangleCount) {
    addVisibleLeaf(triangleCount, triangleCount);
  }

  void RasterVisibilitySet::addVisibleLeaf(std::size_t triangleCount, std::size_t faceCount) {
    LeafDecision decision;
    decision.visible = true;
    decision.triangleCount = triangleCount;
    decision.faceCount = faceCount;
    m_leaves.push_back(std::move(decision));
  }

  void RasterVisibilitySet::addRejectedLeaf(RejectionReason reason, std::size_t triangleCount) {
    addRejectedLeaf(reason, triangleCount, triangleCount);
  }

  void RasterVisibilitySet::addRejectedLeaf(RejectionReason reason, std::size_t triangleCount,
                                            std::size_t faceCount) {
    LeafDecision decision;
    decision.visible = false;
    decision.rejectionReason = reason;
    decision.triangleCount = triangleCount;
    decision.faceCount = faceCount;
    m_leaves.push_back(std::move(decision));
  }

  void RasterVisibilitySet::setVisibleLeafOrder(std::vector<std::size_t> leafIndices) {
    m_visibleLeafOrder = std::move(leafIndices);
  }

  void RasterVisibilitySet::setTileGrid(int width, int height, int tileWidth, int tileHeight) {
    if (width <= 0 || height <= 0 || tileWidth <= 0 || tileHeight <= 0) {
      m_tileGrid = {};
      return;
    }

    m_tileGrid.width = width;
    m_tileGrid.height = height;
    m_tileGrid.tileWidth = tileWidth;
    m_tileGrid.tileHeight = tileHeight;
    m_tileGrid.columns = (width + tileWidth - 1) / tileWidth;
    m_tileGrid.rows = (height + tileHeight - 1) / tileHeight;
    for (LeafDecision& leaf : m_leaves) {
      leaf.tileCoverageKnown = false;
      leaf.tileIndices.clear();
    }
  }

  void RasterVisibilitySet::setVisibleLeafTiles(std::size_t leafIndex,
                                                std::vector<std::size_t> tileIndices) {
    if (leafIndex >= m_leaves.size() || !m_leaves[leafIndex].visible || !hasTileGrid()) {
      return;
    }
    LeafDecision& leaf = m_leaves[leafIndex];
    leaf.tileCoverageKnown = false;
    leaf.tileIndices.clear();

    const std::size_t tileCount = m_tileGrid.tileCount();
    tileIndices.erase(std::remove_if(tileIndices.begin(), tileIndices.end(),
                                     [tileCount](std::size_t tile) { return tile >= tileCount; }),
                      tileIndices.end());
    std::sort(tileIndices.begin(), tileIndices.end());
    tileIndices.erase(std::unique(tileIndices.begin(), tileIndices.end()), tileIndices.end());
    if (tileIndices.empty()) {
      return;
    }

    leaf.tileCoverageKnown = true;
    leaf.tileIndices = std::move(tileIndices);
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

  bool RasterVisibilitySet::hasTileGrid() const {
    return m_tileGrid.enabled();
  }

  const RasterVisibilitySet::TileGrid& RasterVisibilitySet::tileGrid() const {
    return m_tileGrid;
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

  std::size_t RasterVisibilitySet::visibleLeafTileReferenceCount() const {
    std::size_t count = 0;
    for (const LeafDecision& leaf : m_leaves) {
      if (leaf.visible && leaf.tileCoverageKnown) {
        count += leaf.tileIndices.size();
      }
    }
    return count;
  }

  std::size_t RasterVisibilitySet::coveredTileCount() const {
    std::set<std::size_t> tiles;
    for (const LeafDecision& leaf : m_leaves) {
      if (leaf.visible && leaf.tileCoverageKnown) {
        tiles.insert(leaf.tileIndices.begin(), leaf.tileIndices.end());
      }
    }
    return tiles.size();
  }

  std::size_t RasterVisibilitySet::tileUncertainVisibleLeafCount() const {
    if (!hasTileGrid()) {
      return visibleLeafCount();
    }

    std::size_t count = 0;
    for (const LeafDecision& leaf : m_leaves) {
      if (leaf.visible && !leaf.tileCoverageKnown) {
        ++count;
      }
    }
    return count;
  }
}
