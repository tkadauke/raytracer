#pragma once

#include "RasterMaterial.h"

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/math/Rect.h"
#include "core/math/Vector.h"
#include "render/TilePlan.h"
#include "render/primitives/Primitive.h"
#include "render/viewplanes/ViewPlane.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace engine::raster::detail {

  // Current homogeneous near-plane depth for the raster front end. Shadow
  // cascade range selection shares the value so light-space depth maps cover the
  // same visible depth interval as camera-space clipping. The v2 plan tracks
  // making this explicit rasterizer state.
  inline constexpr double kNearClipDepth = 0.1;

  // Result of projecting one mesh vertex before triangle assembly. The emitter
  // caches these per source mesh vertex so every face that references the vertex
  // can reuse the clip coordinate, outcode, and already-valid screen point.
  struct ProjectedVertex {
    Vector4d clip;
    Vector3d screen;
    std::uint8_t outCode;
  };

  // Vertex payload used while clipping in homogeneous space. It keeps world
  // attributes beside clip coordinates so the clipper can synthesize new
  // vertices without losing the data later needed for perspective-correct
  // interpolation and material evaluation.
  struct ClipVert {
    Vector3d point;
    Vector3d normal;
    Vector2d uv;
    Vector4d clip;
    Vector3d screen;

    // Screen coordinates are only valid for vertices already inside the clip
    // volume. Clipped vertices compute them lazily after the polygon is known
    // to survive.
    bool ensureScreen(const render::ViewPlane& viewPlane) {
      if (screen.isUndefined()) {
        screen = viewPlane.screenFromClip(clip);
      }
      return screen.isDefined();
    }
  };

  // Final per-vertex data consumed by the edge-function rasterizer. Coordinates
  // remain fractional screen positions until the rasterizer prepares fixed-point
  // edge equations; the reciprocal terms preserve enough homogeneous state for
  // perspective-correct depth, world position, normal, and UV interpolation in
  // the fragment loop.
  struct RasterVertex {
    Vector3d point;
    Vector3d normal;
    Vector2d uv;
    double invW;
    double depthOverW;
    double x;
    double y;
  };

  // Fragment payload after barycentric interpolation. Constructing it is the
  // handoff from coverage math to shading/depth tests.
  struct InterpolatedFragment {
    InterpolatedFragment(const RasterVertex& v0, const RasterVertex& v1, const RasterVertex& v2,
                         double w0b, double w1b, double w2b) {
      // Projective interpolation uses homogeneous clip.w, not blindly
      // camera-space depth. For pinhole projection w is eye-relative depth, so
      // this is the usual 1/z correction. For orthographic projection w is 1,
      // so the formula collapses to ordinary linear interpolation.
      const double wp0 = w0b * v0.invW;
      const double wp1 = w1b * v1.invW;
      const double wp2 = w2b * v2.invW;
      const double correction = 1.0 / (wp0 + wp1 + wp2);

      depth = (w0b * v0.depthOverW + w1b * v1.depthOverW + w2b * v2.depthOverW) * correction;
      worldPos = (v0.point * wp0 + v1.point * wp1 + v2.point * wp2) * correction;
      normal = (v0.normal * wp0 + v1.normal * wp1 + v2.normal * wp2) * correction;
      uv = (v0.uv * wp0 + v1.uv * wp1 + v2.uv * wp2) * correction;
    }

    double depth;
    Vector3d worldPos;
    Vector3d normal;
    Vector2d uv;
  };

  // Lightweight adapter for a full-frame pass buffer. The hot raster loop only
  // needs at(x, y); using this wrapper lets the same templated loop address
  // full-frame and tile-local storage without branching per fragment.
  template<class T>
  class RasterFullBufferView {
  public:
    RasterFullBufferView()
        : m_buffer(nullptr) {
    }

    explicit RasterFullBufferView(Buffer<T>& buffer)
        : m_buffer(&buffer) {
    }

    T& at(int x, int y) const {
      return (*m_buffer)[y][x];
    }

    bool isValid() const {
      return m_buffer != nullptr;
    }

  private:
    Buffer<T>* m_buffer;
  };

  // Lightweight adapter for tile-local pass buffers. Rasterization still uses
  // global pixel coordinates, while this view translates them into the local
  // tile buffer owned by one worker task.
  template<class T>
  class RasterTileBufferView {
  public:
    RasterTileBufferView()
        : m_buffer(nullptr),
          m_originX(0),
          m_originY(0) {
    }

    RasterTileBufferView(Buffer<T>& buffer, int originX, int originY)
        : m_buffer(&buffer),
          m_originX(originX),
          m_originY(originY) {
    }

    T& at(int x, int y) const {
      return (*m_buffer)[y - m_originY][x - m_originX];
    }

    bool isValid() const {
      return m_buffer != nullptr;
    }

  private:
    Buffer<T>* m_buffer;
    int m_originX;
    int m_originY;
  };

  template<class T>
  RasterFullBufferView<T> fullBufferView(Buffer<T>& buffer) {
    return RasterFullBufferView<T>(buffer);
  }

  template<class T>
  RasterTileBufferView<T> tileBufferView(Buffer<T>& buffer, const Recti& rect) {
    return RasterTileBufferView<T>(buffer, rect.left(), rect.top());
  }

  // Prepared triangle handed from the emitter to the raster pass. It stores the
  // three raster vertices plus enough scene identity to support material
  // evaluation, fragment shader inputs, and face-indexed diagnostic colors.
  struct RasterTriangle {
    std::array<RasterVertex, 3> vertices;
    const render::Primitive* primitive;
    std::shared_ptr<render::Material> material;
    RasterMaterial rasterMaterial;
    std::uint64_t faceIdx;
  };

  // Per-frame tile binning structure. The tile plan defines disjoint pixel
  // rectangles; this grid records which prepared triangle indices may touch
  // each rectangle so worker tasks can render without locking shared pixels.
  class RasterTileGrid {
  public:
    explicit RasterTileGrid(const render::TilePlan& plan)
        : m_plan(plan),
          m_triangleIndices(plan.size()) {
    }

    Recti rect(int row, int col) const {
      return m_plan.rect(row, col);
    }

    std::size_t index(int row, int col) const {
      return m_plan.index(row, col);
    }

    const std::vector<std::size_t>& triangleIndices(std::size_t tileIndex) const {
      return m_triangleIndices[tileIndex];
    }

    std::size_t addBounds(int rawMinX, int rawMaxX, int rawMinY, int rawMaxY,
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
          m_triangleIndices[index(row, col)].push_back(triangleIndex);
          ++added;
        }
      }
      return added;
    }

  private:
    render::TilePlan m_plan;
    std::vector<std::vector<std::size_t>> m_triangleIndices;
  };

  // Retained triangle batch for paths that need to reuse emitted triangles:
  // queued tiled rendering and MSAA. The ordinary single-tile 1x path bypasses
  // this and streams triangles directly to avoid allocation and binning cost.
  class RasterTriangleSet {
  public:
    explicit RasterTriangleSet(const render::TilePlan& tilePlan)
        : m_tileGrid(tilePlan) {
    }

    void add(const RasterTriangle& triangle) {
      const double minX =
        std::min({triangle.vertices[0].x, triangle.vertices[1].x, triangle.vertices[2].x});
      const double maxX =
        std::max({triangle.vertices[0].x, triangle.vertices[1].x, triangle.vertices[2].x});
      const double minY =
        std::min({triangle.vertices[0].y, triangle.vertices[1].y, triangle.vertices[2].y});
      const double maxY =
        std::max({triangle.vertices[0].y, triangle.vertices[1].y, triangle.vertices[2].y});
      // Triangle sets are reused for all MSAA samples, so bins cover the
      // possible half-pixel sample-offset envelope around fractional vertices.
      const int rawMinX = static_cast<int>(std::ceil(minX - 0.5));
      const int rawMaxX = static_cast<int>(std::floor(maxX + 0.5));
      const int rawMinY = static_cast<int>(std::ceil(minY - 0.5));
      const int rawMaxY = static_cast<int>(std::floor(maxY + 0.5));

      const std::size_t triangleIndex = m_triangles.size();
      const std::size_t added =
        m_tileGrid.addBounds(rawMinX, rawMaxX, rawMinY, rawMaxY, triangleIndex);
      if (added == 0)
        return;

      m_triangles.push_back(triangle);
      m_binnedTriangleCount += added;
    }

    bool empty() const {
      return m_binnedTriangleCount == 0;
    }

    const std::vector<RasterTriangle>& triangles() const {
      return m_triangles;
    }

    const RasterTileGrid& tileGrid() const {
      return m_tileGrid;
    }

  private:
    std::vector<RasterTriangle> m_triangles;
    RasterTileGrid m_tileGrid;
    std::size_t m_binnedTriangleCount{0};
  };

}
