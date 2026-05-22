#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"
#include "core/math/Rect.h"
#include "core/math/Vector.h"
#include "render/TilePlan.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Primitive.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/Texture.h"
#include "render/viewplanes/ViewPlane.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <typeinfo>
#include <utility>
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
    bool ensureScreen(const render::ViewPlane& viewPlane) {
      if (screen.isUndefined()) {
        screen = viewPlane.screenFromClip(clip);
      }
      return screen.isDefined();
    }
  };

  // Final per-vertex data consumed by the edge-function rasterizer. Coordinates
  // are currently integer pixel positions; the reciprocal terms preserve enough
  // homogeneous state for perspective-correct depth, world position, normal,
  // and UV interpolation in the fragment loop.
  struct RasterVertex {
    Vector3d point;
    Vector3d normal;
    Vector2d uv;
    double invW;
    double depthOverW;
    int x;
    int y;
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

  // Stable diagnostic color for primitives without a material the rasterizer can
  // interpret. The hash keeps missing-material output visible without requiring
  // global state or per-run randomness.
  inline Colord fallbackFaceColor(std::uint64_t index) {
    const std::uint64_t r = (index * 2654435761ULL) & 0xFFu;
    const std::uint64_t g = (index * 40503ULL + 12345) & 0xFFu;
    const std::uint64_t b = (index * 15485863ULL + 999983) & 0xFFu;
    return Colord(0.3 + (r / 255.0) * 0.7, 0.3 + (g / 255.0) * 0.7, 0.3 + (b / 255.0) * 0.7);
  }

  // Per-triangle material adapter used by the built-in fragment path. Most
  // triangles collapse to a constant albedo; texture-backed materials keep a
  // shared texture pointer and evaluate it with the interpolated fragment
  // context only when a fragment is actually shaded.
  class RasterMaterial {
  public:
    RasterMaterial()
        : m_kind(Kind::Constant),
          m_albedo(Colord::black()),
          m_texture(nullptr) {
    }

    static RasterMaterial constant(const Colord& albedo) {
      return RasterMaterial(Kind::Constant, albedo, nullptr);
    }

    static RasterMaterial texture(std::shared_ptr<render::Texturec> texture) {
      return RasterMaterial(Kind::Texture, Colord::black(), std::move(texture));
    }

    Colord albedo(const render::Primitive* primitive, const Vector3d& worldPos,
                  const Vector3d& normal, const Vector2d& uv) const {
      if (m_kind == Kind::Constant || !m_texture)
        return m_albedo;

      const HitPoint hp(primitive, 0.0, Vector4d(worldPos), normal, uv);
      const Rayd ray(worldPos, -normal);
      return m_texture->evaluate(ray, hp);
    }

  private:
    enum class Kind { Constant, Texture };

    RasterMaterial(Kind kind, const Colord& albedo, std::shared_ptr<render::Texturec> texture)
        : m_kind(kind),
          m_albedo(albedo),
          m_texture(std::move(texture)) {
    }

    Kind m_kind;
    Colord m_albedo;
    std::shared_ptr<render::Texturec> m_texture;
  };

  // Per-leaf material classifier. The emitter builds one source per primitive
  // leaf, then asks it for a concrete RasterMaterial for each emitted face so
  // expensive material/type checks stay out of the fragment loop.
  class RasterMaterialSource {
  public:
    static RasterMaterialSource from(const std::shared_ptr<render::Material>& material) {
      auto matte = std::dynamic_pointer_cast<render::MatteMaterial>(material);
      if (!matte)
        return faceColor();

      auto texture = matte->diffuseTexture();
      if (!texture)
        return faceColor();

      const render::Texturec* texturePtr = texture.get();
      if (typeid(*texturePtr) == typeid(render::ConstantColorTexture)) {
        const auto* constant = static_cast<const render::ConstantColorTexture*>(texturePtr);
        return constantAlbedo(constant->color());
      }

      return textured(std::move(texture));
    }

    RasterMaterial forFace(std::uint64_t faceIdx) const {
      switch (m_kind) {
      case Kind::FaceColor:
        return RasterMaterial::constant(fallbackFaceColor(faceIdx));
      case Kind::Constant:
        return RasterMaterial::constant(m_albedo);
      case Kind::Texture:
        return RasterMaterial::texture(m_texture);
      }
      return RasterMaterial::constant(fallbackFaceColor(faceIdx));
    }

  private:
    enum class Kind { FaceColor, Constant, Texture };

    static RasterMaterialSource faceColor() {
      return RasterMaterialSource(Kind::FaceColor, Colord::black(), nullptr);
    }

    static RasterMaterialSource constantAlbedo(const Colord& albedo) {
      return RasterMaterialSource(Kind::Constant, albedo, nullptr);
    }

    static RasterMaterialSource textured(std::shared_ptr<render::Texturec> texture) {
      return RasterMaterialSource(Kind::Texture, Colord::black(), std::move(texture));
    }

    RasterMaterialSource(Kind kind, const Colord& albedo, std::shared_ptr<render::Texturec> texture)
        : m_kind(kind),
          m_albedo(albedo),
          m_texture(std::move(texture)) {
    }

    Kind m_kind;
    Colord m_albedo;
    std::shared_ptr<render::Texturec> m_texture;
  };

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
      const int rawMinX =
        std::min({triangle.vertices[0].x, triangle.vertices[1].x, triangle.vertices[2].x});
      const int rawMaxX =
        std::max({triangle.vertices[0].x, triangle.vertices[1].x, triangle.vertices[2].x});
      const int rawMinY =
        std::min({triangle.vertices[0].y, triangle.vertices[1].y, triangle.vertices[2].y});
      const int rawMaxY =
        std::max({triangle.vertices[0].y, triangle.vertices[1].y, triangle.vertices[2].y});

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
