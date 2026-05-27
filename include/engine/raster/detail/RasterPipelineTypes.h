#pragma once

#include "engine/raster/detail/RasterMaterial.h"

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/math/Rect.h"
#include "core/math/Vector.h"
#include "render/TilePlan.h"
#include "render/primitives/Primitive.h"
#include "render/viewplanes/ViewPlane.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace engine::raster::detail {

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
    bool ensureScreen(const render::ViewPlane& viewPlane);
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
                         double w0b, double w1b, double w2b);

    double depth;
    Vector3d worldPos;
    Vector3d normal;
    Vector2d uv;
  };

  struct RasterFragment {
    Colord color;
    double alpha{1.0};
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

  Recti intersectRasterRects(const Recti& a, const Recti& b);
  bool rasterRectEmpty(const Recti& rect);

  struct RasterPoint2d {
    double x;
    double y;
  };

  // Prepared triangle handed from the emitter to the raster pass. It stores the
  // three raster vertices plus enough scene identity to support material
  // evaluation, fragment shader inputs, and face-indexed diagnostic colors.
  struct RasterTriangle {
    std::array<RasterVertex, 3> vertices;
    const render::Primitive* primitive;
    std::shared_ptr<render::Material> material;
    RasterMaterial rasterMaterial;
    RasterTangentFrame tangentFrame;
    Vector2d uvDx;
    Vector2d uvDy;
    std::uint64_t faceIdx;
  };

  // Borrowed full-frame outputs used by diagnostics and picking experiments.
  // They mirror the committed raster pass results without owning storage or
  // changing the color/depth/stencil buffer lifetime of the normal pass.
  struct RasterDiagnosticBufferViews {
    RasterFullBufferView<double> depth;
    RasterFullBufferView<Vector3d> worldPosition;
    RasterFullBufferView<Vector3d> normal;
    RasterFullBufferView<const render::Primitive*> primitive;
    RasterFullBufferView<const render::Material*> material;
    RasterFullBufferView<std::uint64_t> face;
    RasterFullBufferView<std::uint8_t> stencil;
    RasterFullBufferView<std::uint32_t> coverageCount;
    RasterFullBufferView<std::uint32_t> depthTestCount;
    RasterFullBufferView<std::uint32_t> depthPassCount;
    RasterFullBufferView<std::uint32_t> shadeCount;
    RasterFullBufferView<std::uint32_t> colorWriteCount;

    void recordCoverage(int x, int y) const;

    void recordDepthTest(int x, int y) const;

    void recordDepthPass(int x, int y) const;

    void recordShade(int x, int y) const;

    void recordColorWrite(int x, int y) const;

    void writeStencil(int x, int y, std::uint8_t value) const;

    void writeFragment(const RasterTriangle& triangle, int x, int y,
                       const InterpolatedFragment& fragment, double committedDepth) const;

  private:
    static void increment(RasterFullBufferView<std::uint32_t> counter, int x, int y);
  };

  // Per-frame tile binning structure. The tile plan defines disjoint pixel
  // rectangles; this grid records which prepared triangle indices may touch
  // each rectangle so worker tasks can render without locking shared pixels.
  class RasterTileGrid {
  public:
    explicit RasterTileGrid(const render::TilePlan& plan);

    Recti rect(int row, int col) const;

    std::size_t index(int row, int col) const;

    const std::vector<std::size_t>& triangleIndices(std::size_t tileIndex) const;

    std::size_t addBounds(int rawMinX, int rawMaxX, int rawMinY, int rawMaxY,
                          const std::array<RasterPoint2d, 3>& triangle, std::size_t triangleIndex);

  private:
    render::TilePlan m_plan;
    std::vector<std::vector<std::size_t>> m_triangleIndices;
  };

  // Retained triangle batch for paths that need to reuse emitted triangles:
  // queued tiled rendering and MSAA. The ordinary single-tile 1x path bypasses
  // this and streams triangles directly to avoid allocation and binning cost.
  class RasterTriangleSet {
  public:
    explicit RasterTriangleSet(const render::TilePlan& tilePlan);

    void add(const RasterTriangle& triangle);

    bool empty() const;

    const std::vector<RasterTriangle>& triangles() const;

    const RasterTileGrid& tileGrid() const;

    std::size_t binnedTriangleCount() const;

  private:
    std::vector<RasterTriangle> m_triangles;
    RasterTileGrid m_tileGrid;
    std::size_t m_binnedTriangleCount{0};
  };

}
