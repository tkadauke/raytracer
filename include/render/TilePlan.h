#pragma once

#include "core/math/IntegerDecomposition.h"
#include "core/math/Rect.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace render {
  class TilePlan {
  public:
    static TilePlan forBuffer(int width, int height, int requestedTiles) {
      if (width <= 0 || height <= 0) {
        return TilePlan(width, height, 0, 0);
      }

      const int tileCount = std::max(1, std::min(requestedTiles, width * height));
      IntegerDecomposition decomposition(tileCount);
      return TilePlan(width, height, std::max(1, std::min(decomposition.first(), height)),
                      std::max(1, std::min(decomposition.second(), width)));
    }

    int width() const {
      return m_width;
    }

    int height() const {
      return m_height;
    }

    int rows() const {
      return m_rows;
    }

    int cols() const {
      return m_cols;
    }

    std::size_t size() const {
      return static_cast<std::size_t>(m_rows * m_cols);
    }

    bool empty() const {
      return m_rows == 0 || m_cols == 0;
    }

    bool isSingleTile() const {
      return size() == 1;
    }

    int maxTileWidth() const {
      if (empty())
        return 0;
      return (m_width + m_cols - 1) / m_cols;
    }

    int maxTileHeight() const {
      if (empty())
        return 0;
      return (m_height + m_rows - 1) / m_rows;
    }

    int maxTilePixels() const {
      return maxTileWidth() * maxTileHeight();
    }

    double averageTilePixels() const {
      if (empty())
        return 0.0;
      return static_cast<double>(m_width) * static_cast<double>(m_height) /
             static_cast<double>(size());
    }

    Recti fullRect() const {
      return Recti(0, 0, m_width, m_height);
    }

    Recti rect(int row, int col) const {
      if (empty())
        return Recti();
      const int left = static_cast<int>(std::floor(double(m_width) * col / m_cols));
      const int right = static_cast<int>(std::floor(double(m_width) * (col + 1) / m_cols));
      const int top = static_cast<int>(std::floor(double(m_height) * row / m_rows));
      const int bottom = static_cast<int>(std::floor(double(m_height) * (row + 1) / m_rows));
      return Recti(left, top, right - left, bottom - top);
    }

    int columnForX(int x) const {
      if (empty())
        return 0;
      const int clamped = std::clamp(x, 0, m_width - 1);
      return static_cast<int>(((static_cast<long long>(clamped) + 1) * m_cols - 1) / m_width);
    }

    int rowForY(int y) const {
      if (empty())
        return 0;
      const int clamped = std::clamp(y, 0, m_height - 1);
      return static_cast<int>(((static_cast<long long>(clamped) + 1) * m_rows - 1) / m_height);
    }

    std::size_t index(int row, int col) const {
      return static_cast<std::size_t>(row * m_cols + col);
    }

  private:
    TilePlan(int width, int height, int rows, int cols)
        : m_width(width),
          m_height(height),
          m_rows(rows),
          m_cols(cols) {
    }

    int m_width;
    int m_height;
    int m_rows;
    int m_cols;
  };
}
