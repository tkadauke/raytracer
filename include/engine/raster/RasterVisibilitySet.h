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
    enum class RejectionReason { Frustum, Backface };

    struct TileGrid {
      int width{0};
      int height{0};
      int tileWidth{0};
      int tileHeight{0};
      int columns{0};
      int rows{0};

      bool enabled() const;
      std::size_t tileCount() const;
    };

    void addVisibleLeaf(std::size_t triangleCount);
    void addVisibleLeaf(std::size_t triangleCount, std::size_t faceCount);
    void addRejectedLeaf(RejectionReason reason, std::size_t triangleCount);
    void addRejectedLeaf(RejectionReason reason, std::size_t triangleCount, std::size_t faceCount);
    void setVisibleLeafOrder(std::vector<std::size_t> leafIndices);
    void setTileGrid(int width, int height, int tileWidth, int tileHeight);
    void setVisibleLeafTiles(std::size_t leafIndex, std::vector<std::size_t> tileIndices);
    void setVisibleLeafTiles(std::size_t leafIndex, std::vector<std::size_t> tileIndices,
                             double nearestDepth);

    bool leafVisible(std::size_t leafIndex) const;
    bool hasVisibleLeafOrder() const;
    const std::vector<std::size_t>& visibleLeafOrder() const;
    bool hasTileGrid() const;
    const TileGrid& tileGrid() const;
    std::size_t leafCount() const;
    std::size_t leafFaceCount(std::size_t leafIndex) const;
    std::size_t inputTriangleCount() const;
    std::size_t visibleLeafCount() const;
    std::size_t visibleTriangleCount() const;
    std::size_t rejectedLeafCount() const;
    std::size_t rejectedTriangleCount() const;
    std::size_t rejectedLeafCount(RejectionReason reason) const;
    std::size_t rejectedTriangleCount(RejectionReason reason) const;
    std::size_t visibleLeafTileReferenceCount() const;
    std::size_t coveredTileCount() const;
    bool tileCovered(std::size_t tileIndex) const;
    std::size_t tileUncertainVisibleLeafCount() const;
    std::size_t tileDepthSummarizedTileCount() const;
    std::size_t tileDepthReferenceCount() const;
    double nearestTileDepth(std::size_t tileIndex) const;

  private:
    struct LeafDecision {
      bool visible{true};
      RejectionReason rejectionReason{RejectionReason::Frustum};
      std::size_t triangleCount{0};
      std::size_t faceCount{0};
      bool tileCoverageKnown{false};
      std::vector<std::size_t> tileIndices;
      bool tileDepthKnown{false};
      double nearestTileDepth{0.0};
    };

    void rebuildTileDepthSummaries();

    std::vector<LeafDecision> m_leaves;
    std::vector<std::size_t> m_visibleLeafOrder;
    TileGrid m_tileGrid;
    std::vector<double> m_nearestTileDepths;
    std::size_t m_tileDepthReferenceCount{0};
  };
}
