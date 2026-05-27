#include "engine/raster/detail/RasterPipelineTypes.h"

#include <algorithm>
#include <cmath>

namespace engine::raster::detail {
  namespace {
    void projectRasterPoints(const std::array<RasterPoint2d, 3>& points, double axisX, double axisY,
                             double& minProjection, double& maxProjection) {
      minProjection = maxProjection = points[0].x * axisX + points[0].y * axisY;
      for (std::size_t i = 1; i != points.size(); ++i) {
        const double projection = points[i].x * axisX + points[i].y * axisY;
        minProjection = std::min(minProjection, projection);
        maxProjection = std::max(maxProjection, projection);
      }
    }

    void projectRasterRect(double left, double top, double right, double bottom, double axisX,
                           double axisY, double& minProjection, double& maxProjection) {
      const std::array<RasterPoint2d, 4> corners{
        RasterPoint2d{left, top}, RasterPoint2d{right, top}, RasterPoint2d{right, bottom},
        RasterPoint2d{left, bottom}};
      minProjection = maxProjection = corners[0].x * axisX + corners[0].y * axisY;
      for (std::size_t i = 1; i != corners.size(); ++i) {
        const double projection = corners[i].x * axisX + corners[i].y * axisY;
        minProjection = std::min(minProjection, projection);
        maxProjection = std::max(maxProjection, projection);
      }
    }

    bool rasterProjectionsOverlap(double triangleMin, double triangleMax, double rectMin,
                                  double rectMax) {
      constexpr double epsilon = 1e-9;
      return triangleMax + epsilon >= rectMin && rectMax + epsilon >= triangleMin;
    }

    bool rasterTriangleIntersectsExpandedRect(const std::array<RasterPoint2d, 3>& triangle,
                                              const Recti& rect) {
      // The triangle set is reused by all MSAA samples, whose offsets stay
      // within a half-pixel envelope. Expanding tile bounds by that amount keeps
      // binning conservative while avoiding tiles that the triangle cannot
      // touch.
      constexpr double sampleEnvelope = 0.5;
      const double left = static_cast<double>(rect.left()) - sampleEnvelope;
      const double top = static_cast<double>(rect.top()) - sampleEnvelope;
      const double right = static_cast<double>(rect.right()) + sampleEnvelope;
      const double bottom = static_cast<double>(rect.bottom()) + sampleEnvelope;

      const auto separatedOnAxis = [&](double axisX, double axisY) {
        double triangleMin = 0.0;
        double triangleMax = 0.0;
        double rectMin = 0.0;
        double rectMax = 0.0;
        projectRasterPoints(triangle, axisX, axisY, triangleMin, triangleMax);
        projectRasterRect(left, top, right, bottom, axisX, axisY, rectMin, rectMax);
        return !rasterProjectionsOverlap(triangleMin, triangleMax, rectMin, rectMax);
      };

      if (separatedOnAxis(1.0, 0.0) || separatedOnAxis(0.0, 1.0)) {
        return false;
      }

      for (std::size_t i = 0; i != triangle.size(); ++i) {
        const RasterPoint2d& a = triangle[i];
        const RasterPoint2d& b = triangle[(i + 1) % triangle.size()];
        const double edgeX = b.x - a.x;
        const double edgeY = b.y - a.y;
        if (edgeX == 0.0 && edgeY == 0.0) {
          continue;
        }
        if (separatedOnAxis(-edgeY, edgeX)) {
          return false;
        }
      }

      return true;
    }
  }

  bool ClipVert::ensureScreen(const render::ViewPlane& viewPlane) {
    if (screen.isUndefined()) {
      screen = viewPlane.screenFromClip(clip);
    }
    return screen.isDefined();
  }

  InterpolatedFragment::InterpolatedFragment(const RasterVertex& v0, const RasterVertex& v1,
                                             const RasterVertex& v2, double w0b, double w1b,
                                             double w2b) {
    // Projective interpolation uses homogeneous clip.w, not blindly
    // camera-space depth. For pinhole projection w is eye-relative depth, so
    // this is the usual 1/z correction. For orthographic projection w is 1, so
    // the formula collapses to ordinary linear interpolation.
    const double wp0 = w0b * v0.invW;
    const double wp1 = w1b * v1.invW;
    const double wp2 = w2b * v2.invW;
    const double correction = 1.0 / (wp0 + wp1 + wp2);

    depth = (w0b * v0.depthOverW + w1b * v1.depthOverW + w2b * v2.depthOverW) * correction;
    worldPos = (v0.point * wp0 + v1.point * wp1 + v2.point * wp2) * correction;
    normal = (v0.normal * wp0 + v1.normal * wp1 + v2.normal * wp2) * correction;
    uv = (v0.uv * wp0 + v1.uv * wp1 + v2.uv * wp2) * correction;
  }

  Recti intersectRasterRects(const Recti& a, const Recti& b) {
    const int left = std::max(a.left(), b.left());
    const int top = std::max(a.top(), b.top());
    const int right = std::min(a.right(), b.right());
    const int bottom = std::min(a.bottom(), b.bottom());
    return Recti(left, top, std::max(0, right - left), std::max(0, bottom - top));
  }

  bool rasterRectEmpty(const Recti& rect) {
    return rect.width() <= 0 || rect.height() <= 0;
  }

  void RasterDiagnosticBufferViews::recordCoverage(int x, int y) const {
    increment(coverageCount, x, y);
  }

  void RasterDiagnosticBufferViews::recordDepthTest(int x, int y) const {
    increment(depthTestCount, x, y);
  }

  void RasterDiagnosticBufferViews::recordDepthPass(int x, int y) const {
    increment(depthPassCount, x, y);
  }

  void RasterDiagnosticBufferViews::recordShade(int x, int y) const {
    increment(shadeCount, x, y);
  }

  void RasterDiagnosticBufferViews::recordColorWrite(int x, int y) const {
    increment(colorWriteCount, x, y);
  }

  void RasterDiagnosticBufferViews::writeStencil(int x, int y, std::uint8_t value) const {
    if (stencil.isValid()) {
      stencil.at(x, y) = value;
    }
  }

  void RasterDiagnosticBufferViews::writeFragment(const RasterTriangle& triangle, int x, int y,
                                                  const InterpolatedFragment& fragment,
                                                  double committedDepth) const {
    if (depth.isValid()) {
      depth.at(x, y) = committedDepth;
    }
    if (worldPosition.isValid()) {
      worldPosition.at(x, y) = fragment.worldPos;
    }
    if (normal.isValid()) {
      normal.at(x, y) = fragment.normal.normalized();
    }
    if (primitive.isValid()) {
      primitive.at(x, y) = triangle.primitive;
    }
    if (material.isValid()) {
      material.at(x, y) = triangle.material.get();
    }
    if (face.isValid()) {
      face.at(x, y) = triangle.faceIdx;
    }
  }

  void RasterDiagnosticBufferViews::increment(RasterFullBufferView<std::uint32_t> counter, int x,
                                              int y) {
    if (counter.isValid()) {
      ++counter.at(x, y);
    }
  }

  RasterTileGrid::RasterTileGrid(const render::TilePlan& plan)
      : m_plan(plan),
        m_triangleIndices(plan.size()) {
  }

  Recti RasterTileGrid::rect(int row, int col) const {
    return m_plan.rect(row, col);
  }

  std::size_t RasterTileGrid::index(int row, int col) const {
    return m_plan.index(row, col);
  }

  const std::vector<std::size_t>& RasterTileGrid::triangleIndices(std::size_t tileIndex) const {
    return m_triangleIndices[tileIndex];
  }

  std::size_t RasterTileGrid::addBounds(int rawMinX, int rawMaxX, int rawMinY, int rawMaxY,
                                        const std::array<RasterPoint2d, 3>& triangle,
                                        std::size_t triangleIndex) {
    if (rawMaxX < 0 || rawMaxY < 0 || rawMinX >= m_plan.width() || rawMinY >= m_plan.height()) {
      return 0;
    }

    const int minX = std::clamp(rawMinX, 0, m_plan.width() - 1);
    const int maxX = std::clamp(rawMaxX, 0, m_plan.width() - 1);
    const int minY = std::clamp(rawMinY, 0, m_plan.height() - 1);
    const int maxY = std::clamp(rawMaxY, 0, m_plan.height() - 1);

    const int firstCol = m_plan.columnForX(minX);
    const int lastCol = m_plan.columnForX(maxX);
    const int firstRow = m_plan.rowForY(minY);
    const int lastRow = m_plan.rowForY(maxY);

    std::size_t added = 0;
    for (int row = firstRow; row <= lastRow; ++row) {
      for (int col = firstCol; col <= lastCol; ++col) {
        if (!rasterTriangleIntersectsExpandedRect(triangle, rect(row, col))) {
          continue;
        }
        m_triangleIndices[index(row, col)].push_back(triangleIndex);
        ++added;
      }
    }
    return added;
  }

  RasterTriangleSet::RasterTriangleSet(const render::TilePlan& tilePlan)
      : m_tileGrid(tilePlan) {
  }

  void RasterTriangleSet::add(const RasterTriangle& triangle) {
    const double minX =
      std::min({triangle.vertices[0].x, triangle.vertices[1].x, triangle.vertices[2].x});
    const double maxX =
      std::max({triangle.vertices[0].x, triangle.vertices[1].x, triangle.vertices[2].x});
    const double minY =
      std::min({triangle.vertices[0].y, triangle.vertices[1].y, triangle.vertices[2].y});
    const double maxY =
      std::max({triangle.vertices[0].y, triangle.vertices[1].y, triangle.vertices[2].y});
    // Triangle sets are reused for all MSAA samples, so bins cover the possible
    // half-pixel sample-offset envelope around fractional vertices.
    const int rawMinX = static_cast<int>(std::ceil(minX - 0.5));
    const int rawMaxX = static_cast<int>(std::floor(maxX + 0.5));
    const int rawMinY = static_cast<int>(std::ceil(minY - 0.5));
    const int rawMaxY = static_cast<int>(std::floor(maxY + 0.5));

    const std::size_t triangleIndex = m_triangles.size();
    const std::array<RasterPoint2d, 3> screenTriangle{
      RasterPoint2d{triangle.vertices[0].x, triangle.vertices[0].y},
      RasterPoint2d{triangle.vertices[1].x, triangle.vertices[1].y},
      RasterPoint2d{triangle.vertices[2].x, triangle.vertices[2].y}};
    const std::size_t added =
      m_tileGrid.addBounds(rawMinX, rawMaxX, rawMinY, rawMaxY, screenTriangle, triangleIndex);
    if (added == 0)
      return;

    m_triangles.push_back(triangle);
    m_binnedTriangleCount += added;
  }

  bool RasterTriangleSet::empty() const {
    return m_binnedTriangleCount == 0;
  }

  const std::vector<RasterTriangle>& RasterTriangleSet::triangles() const {
    return m_triangles;
  }

  const RasterTileGrid& RasterTriangleSet::tileGrid() const {
    return m_tileGrid;
  }

  std::size_t RasterTriangleSet::binnedTriangleCount() const {
    return m_binnedTriangleCount;
  }
}
