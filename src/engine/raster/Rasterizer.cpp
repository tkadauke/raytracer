#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"
#include "core/geometry/Mesh.h"
#include "core/geometry/Rasterize.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"
#include "core/math/Vector.h"
#include "render/HomogeneousClipVolume.h"
#include "render/TilePlan.h"
#include "render/cameras/Camera.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/Light.h"
#include "render/materials/MatteMaterial.h"
#include "render/postprocess/Fxaa.h"
#include "render/primitives/Scene.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/Texture.h"
#include "render/viewplanes/ViewPlane.h"

#include "../TileRenderTask.h"

#include <QThread>
#include <QThreadPool>

#include <algorithm>
#include <array>
#include <list>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <typeinfo>
#include <vector>

using namespace engine::raster;

// Rasterizer.cpp is organized as a software graphics pipeline:
//
//   Rasterizer::render()
//     sets up the public engine state, clears the output, and hands
//     the frame to Rasterizer::Private.
//
//   Rasterizer::Private
//     owns frame-level orchestration: tile dispatch, pass-local
//     depth/stencil buffers, tile-local MSAA storage, and the active task
//     list used by the UI progress overlay.
//
//   anonymous namespace
//     defines the local vocabulary of the pipeline: projected vertices,
//     clip vertices, raster vertices, triangle batches, material
//     evaluation, and the policy objects that specialize the hot loop.
//
//   RasterTriangleEmitter
//     is the front end. It walks leaf primitives, tessellates them,
//     projects each mesh vertex once, clips polygons in homogeneous
//     space, applies culling/vertex-shader hooks, and streams prepared
//     RasterTriangle objects.
//
//   RasterTriangleSet / RasterTileGrid
//     is the batch. It stores the emitted triangles and bins each one
//     into the tiles whose pixel rectangles it may touch.
//
//   rasterizeTriangleSetWithPolicies()
//     is the draw step. It chooses direct single-tile rendering or
//     QRunnable tile dispatch, then calls the core edge-function
//     triangle rasterizer for each prepared triangle.
//
//   Depth/Stencil/Fragment policy objects
//     are the C++ implementation trick: runtime state such as
//     "stencil enabled?" or "custom fragment shader?" is resolved once
//     per pass into concrete policy objects, so the per-pixel loop can
//     inline pass/fail/write/shade behavior.
//
// Example path for a normal 1x single-tile render:
//   render -> Private::renderFrame -> RasterTriangleEmitter
//   -> Private::renderTriangleStreamPass -> withPreparedTrianglePolicies
//   -> rasterizePreparedTriangleWithPolicies.
//
// Example path for queued 4x MSAA:
//   renderFrame builds the same triangle set once, then renderMSAAFrame
//   renders each tile's four subpixel offsets into tile-local buffers
//   and resolves the tile directly into the output framebuffer.
//
// If you are reading for performance, start at
// rasterizePreparedTriangleWithPolicies(): that is the fragment loop.
// If you are reading for correctness of projection/clipping, start at
// RasterTriangleEmitter::forEachTriangle(). If you are reading for
// threading, start at Rasterizer::Private::renderTriangleSetPass().

namespace {
  class RasterTriangleEmitter;
  class RasterTriangleSet;
  class ShadowMaps;
}

// Pimpl: hides Qt threading and render-pass orchestration from the
// public header, and gives the .cpp room for local pipeline types.
struct Rasterizer::Private {
  Private()
      : threadPool(std::make_unique<QThreadPool>()),
        queueSize(1) {
    threadPool->setMaxThreadCount(std::max(1, QThread::idealThreadCount()));
  }

  std::unique_ptr<QThreadPool> threadPool;
  std::list<std::shared_ptr<engine::TileRenderTask>> tasks;
  int queueSize;

  // The pimpl owns frame-level orchestration: tiling, pass buffers,
  // MSAA resolve, and task lifetime. Keeping this here makes the
  // public Rasterizer surface describe engine state, while the .cpp
  // can still talk in terms of local pipeline objects.
  struct SamplePattern {
    // Fixed rotated-ish subpixel patterns. These are not random
    // samples; every render repeats the same offsets so tests and
    // docs stay deterministic.
    explicit SamplePattern(int sampleCount) {
      switch (sampleCount) {
      case 2:
        offsets[0] = {-0.25, -0.25};
        offsets[1] = {0.25, 0.25};
        count = 2;
        break;
      case 4:
        offsets[0] = {-0.125, -0.375};
        offsets[1] = {0.375, -0.125};
        offsets[2] = {-0.375, 0.125};
        offsets[3] = {0.125, 0.375};
        count = 4;
        break;
      case 8:
        offsets[0] = {0.0625, -0.1875};
        offsets[1] = {-0.0625, 0.1875};
        offsets[2] = {0.3125, 0.0625};
        offsets[3] = {-0.1875, -0.3125};
        offsets[4] = {-0.3125, 0.3125};
        offsets[5] = {-0.4375, -0.0625};
        offsets[6] = {0.1875, 0.4375};
        offsets[7] = {0.4375, -0.4375};
        count = 8;
        break;
      default:
        offsets[0] = {0.0, 0.0};
        count = 1;
        break;
      }
    }

    std::array<Vector2d, 8> offsets{};
    int count{1};
  };

  class PassBuffers {
  public:
    // A render pass owns depth and optional stencil, but borrows the
    // color target. Single-sample rendering writes straight to the
    // final buffer; full-frame MSAA borrows temporary sample buffers
    // through this wrapper, while queued MSAA uses tile-local buffers.
    PassBuffers(const Rasterizer& rasterizer, const render::TilePlan& tilePlan,
                Buffer<Colord>& colorBuffer)
        : m_colorBuffer(colorBuffer),
          m_depthBuffer(tilePlan.width(), tilePlan.height()) {
      m_depthBuffer.clear(rasterizer.depthClearValue());
      if (rasterizer.stencilTestEnabled()) {
        m_stencilBuffer =
          std::make_unique<Buffer<std::uint8_t>>(tilePlan.width(), tilePlan.height());
        m_stencilBuffer->clear(rasterizer.stencilClearValue());
      }
    }

    Buffer<Colord>& color() {
      return m_colorBuffer;
    }

    Buffer<double>& depth() {
      return m_depthBuffer;
    }

    Buffer<std::uint8_t>* stencil() {
      return m_stencilBuffer.get();
    }

  private:
    Buffer<Colord>& m_colorBuffer;
    Buffer<double> m_depthBuffer;
    std::unique_ptr<Buffer<std::uint8_t>> m_stencilBuffer;
  };

  void renderFrame(const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
                   const std::shared_ptr<render::Camera>& camera,
                   const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

  void renderSingleSampleFrame(const Rasterizer& rasterizer,
                               const std::shared_ptr<render::Scene>& scene,
                               const render::TilePlan& tilePlan,
                               const RasterTriangleEmitter& triangleEmitter,
                               const ShadowMaps& shadowMaps, const std::atomic<bool>& cancelled,
                               Buffer<Colord>& buffer);

  void renderMSAAFrame(const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
                       const render::TilePlan& tilePlan, const SamplePattern& pattern,
                       const RasterTriangleEmitter& triangleEmitter, const ShadowMaps& shadowMaps,
                       const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

  void renderMSAAFullFrame(const Rasterizer& rasterizer,
                           const std::shared_ptr<render::Scene>& scene,
                           const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
                           const ShadowMaps& shadowMaps, const SamplePattern& pattern,
                           const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

  void renderMSAATile(const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
                      const RasterTriangleSet& triangleSet, const ShadowMaps& shadowMaps,
                      const SamplePattern& pattern, const Recti& rect, std::size_t tileIndex,
                      const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

  void renderTriangleSetPass(const Rasterizer& rasterizer,
                             const std::shared_ptr<render::Scene>& scene,
                             const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
                             const ShadowMaps& shadowMaps, const std::atomic<bool>& cancelled,
                             Buffer<Colord>& buffer, const Vector2d& sampleOffset);
  void renderTriangleStreamPass(const Rasterizer& rasterizer,
                                const std::shared_ptr<render::Scene>& scene,
                                const RasterTriangleEmitter& triangleEmitter,
                                const render::TilePlan& tilePlan, const ShadowMaps& shadowMaps,
                                const std::atomic<bool>& cancelled, Buffer<Colord>& buffer,
                                const Vector2d& sampleOffset);

  ShadowMaps buildShadowMaps(const Rasterizer& rasterizer,
                             const std::shared_ptr<render::Scene>& scene,
                             const std::atomic<bool>& cancelled);

  static RasterTriangleSet collectRasterTriangles(const RasterTriangleEmitter& triangleEmitter,
                                                  const render::TilePlan& tilePlan);
  static void accumulateSample(Buffer<Colord>& target, const Buffer<Colord>& sample);
  static void resolveMSAA(Buffer<Colord>& buffer, int sampleCount);
  static void resolveMSAATile(Buffer<Colord>& buffer, const Buffer<Colord>& accumulated,
                              const Recti& rect, int sampleCount);
};

Rasterizer::Rasterizer(std::shared_ptr<render::Scene> scene)
    : RenderEngine(std::move(scene)),
      p(std::make_unique<Private>()) {
}

Rasterizer::Rasterizer(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene)
    : RenderEngine(std::move(camera), std::move(scene)),
      p(std::make_unique<Private>()) {
}

Rasterizer::~Rasterizer() = default;

std::shared_ptr<render::RenderEngine> Rasterizer::cloneForRender() const {
  auto result = std::make_shared<Rasterizer>(
    m_camera ? m_camera->clone() : nullptr,
    m_scene
  );
  result->setTonemap(tonemap());
  result->setLod(m_lod);
  result->setMaximumThreads(p->threadPool->maxThreadCount());
  result->setQueueSize(p->queueSize);
  result->setMSAASamples(m_msaaSamples);
  result->setPostProcessAA(m_postProcessAA);
  result->setShadowMapsEnabled(m_shadowMapsEnabled);
  result->setShadowMapSize(m_shadowMapSize);
  result->setShadowBias(m_shadowBias);
  result->setShadowFilterRadius(m_shadowFilterRadius);
  result->setCullMode(m_cullMode);
  result->setDepthFunc(m_depthFunc);
  result->setDepthClearValue(m_depthClearValue);
  result->setDepthWriteEnabled(m_depthWriteEnabled);
  result->setStencilTestEnabled(m_stencilTestEnabled);
  result->setStencilFunc(m_stencilFunc, m_stencilReference, m_stencilMask);
  result->setStencilClearValue(m_stencilClearValue);
  result->setStencilWriteMask(m_stencilWriteMask);
  result->setStencilOps(m_stencilFailOp, m_stencilDepthFailOp, m_stencilPassOp);
  result->setVertexShader(m_vertexShader);
  result->setFragmentShader(m_fragmentShader);
  if (hasBackgroundColorOverride()) {
    result->setBackgroundColor(backgroundColor());
  }
  return result;
}

void Rasterizer::cancel() {
  m_cancelled.store(true);
}

void Rasterizer::uncancel() {
  m_cancelled.store(false);
}

std::list<Recti> Rasterizer::activeTiles() const {
  std::list<Recti> result;
  for (const auto& task : p->tasks) {
    if (task->active.load(std::memory_order_acquire)) {
      result.push_back(task->rect);
    }
  }
  return result;
}

void Rasterizer::setMaximumThreads(int threads) {
  p->threadPool->setMaxThreadCount(std::max(1, threads));
}

void Rasterizer::setQueueSize(int queue) {
  p->queueSize = std::max(1, queue);
}

void Rasterizer::setMSAASamples(int samples) {
  if (samples <= 1) {
    m_msaaSamples = 1;
  } else if (samples <= 2) {
    m_msaaSamples = 2;
  } else if (samples <= 4) {
    m_msaaSamples = 4;
  } else {
    m_msaaSamples = 8;
  }
}

void Rasterizer::setShadowMapSize(int size) {
  m_shadowMapSize = std::max(1, size);
}

void Rasterizer::render(Buffer<Colord>& buffer) {
  // Caller is expected to call uncancel() between renders. Matches
  // the Wireframe / Raytracer convention.

  // Clear to the configured background before depth-tested fragments overwrite it.
  buffer.clear(backgroundColor());

  if (!m_scene || !m_camera)
    return;

  // Same view-plane setup the other engines perform — the camera
  // projection math depends on the cached basis vectors.
  m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());

  // From here down the render is expressed in pipeline terms. The
  // Rasterizer object contributes configuration; Private drives the
  // concrete passes and keeps task state available for activeTiles().
  p->tasks.clear();
  p->renderFrame(*this, m_scene, m_camera, m_cancelled, buffer);
}

namespace {
  // The anonymous namespace is the raster pipeline vocabulary:
  // transient vertices, policy objects, and hot-path template helpers.
  // None of these types are part of the engine API.

  // Ambient coefficient — same role as MatteMaterial's
  // `ambientCoefficient`. Multiplies the scene's ambient term so
  // the unlit side of an object is visible at its full ambient
  // contribution rather than darkened.
  constexpr double kAmbientCoefficient = 1.0;

  // Near-plane depth used by the rasterizer's homogeneous clipper. It
  // sits just in front of the eye to keep perspective divides bounded
  // for clipped vertices.
  constexpr double kNearClipDepth = 0.1;
  constexpr std::size_t kMaxClipVertices = 32;

  // Projection produces clip-space data first. Screen coordinates are
  // cached only when the vertex is already inside the clip volume.
  struct ProjectedVertex {
    Vector4d clip;
    Vector3d screen;
    std::uint8_t outCode;
  };

  // A clip vertex carries every attribute that clipping may need to
  // synthesize: world position, normal, UV, homogeneous clip coords,
  // and eventually screen position.
  struct ClipVert {
    Vector3d point;
    Vector3d normal;
    Vector2d uv;
    Vector4d clip;
    Vector3d screen;

    bool ensureScreen(const render::ViewPlane& viewPlane) {
      if (screen.isUndefined()) {
        screen = viewPlane.screenFromClip(clip);
      }
      return screen.isDefined();
    }
  };

  // A raster vertex is after clipping and optional vertex shading:
  // integer screen coordinates plus the homogeneous reciprocal used
  // by perspective-correct interpolation. Perspective cameras emit
  // clip.w = camera depth, while orthographic cameras emit clip.w = 1;
  // carrying 1/w keeps both projections correct.
  struct RasterVertex {
    Vector3d point;
    Vector3d normal;
    Vector2d uv;
    double invW;
    double depthOverW;
    int x;
    int y;
  };

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

  inline Colord fallbackFaceColor(std::uint64_t index) {
    const std::uint64_t r = (index * 2654435761ULL) & 0xFFu;
    const std::uint64_t g = (index * 40503ULL + 12345) & 0xFFu;
    const std::uint64_t b = (index * 15485863ULL + 999983) & 0xFFu;
    return Colord(0.3 + (r / 255.0) * 0.7, 0.3 + (g / 255.0) * 0.7, 0.3 + (b / 255.0) * 0.7);
  }

  // Per-triangle material evaluator prepared during emission. Most
  // raster fragments use either a cached fallback face color or a
  // cached constant matte albedo; only arbitrary textures need the
  // heavier HitPoint/Ray context.
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

  // Per-primitive source for RasterMaterial. The expensive material
  // type checks happen once per leaf primitive, while face-indexed
  // fallback colors are still baked per emitted triangle.
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

  // Material and primitive pointers stay attached to triangles so
  // the fragment stage can use the same scene/material vocabulary as
  // the raytracer, even though no rays are traced here.
  struct RasterTriangle {
    std::array<RasterVertex, 3> vertices;
    const render::Primitive* primitive;
    std::shared_ptr<render::Material> material;
    RasterMaterial rasterMaterial;
    std::uint64_t faceIdx;
  };

  // Tile-local triangle bins. The tile plan owns pixel rectangles;
  // this grid owns "which triangles might touch this rectangle".
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

  // Renderable triangle batch for one frame. It stores prepared
  // triangles once, plus per-tile index lists for parallel drawing.
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

      // Bin by conservative screen-space bounds. Tiles own disjoint
      // pixel rectangles, so color/depth/stencil writes stay lock-free
      // once a triangle has been copied into each overlapping tile.
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

  using ClipPolygon = std::array<ClipVert, kMaxClipVertices>;

  const render::HomogeneousClipVolume& clipVolume() {
    static const render::HomogeneousClipVolume volume(kNearClipDepth);
    return volume;
  }

  // Clipping creates new vertices on plane intersections. The
  // geometry is homogeneous, but attributes are interpolated in the
  // original primitive domain so the later perspective-correct stage
  // still has meaningful per-vertex values.
  inline ClipVert interpolateClipVert(const ClipVert& from, const ClipVert& to, double t) {
    return {from.point + (to.point - from.point) * t, from.normal + (to.normal - from.normal) * t,
            from.uv + (to.uv - from.uv) * t, from.clip + (to.clip - from.clip) * t,
            Vector3d::undefined};
  }

  const Vector4d& clipOf(const ClipVert& vertex) {
    return vertex.clip;
  }

  inline std::size_t clipTriangleToView(const std::array<ClipVert, 3>& input,
                                        ClipPolygon& clipped) {
    return clipVolume().clipTriangle(input, clipped, clipOf, interpolateClipVert);
  }

  inline double signedScreenArea(const ClipVert& v0, const ClipVert& v1, const ClipVert& v2) {
    return (v1.screen.x() - v0.screen.x()) * (v2.screen.y() - v0.screen.y()) -
           (v1.screen.y() - v0.screen.y()) * (v2.screen.x() - v0.screen.x());
  }

  // Culling is a tiny policy too: it turns the public cull mode into
  // a single projected-area decision used before rasterization.
  struct TriangleCullPolicy {
    Rasterizer::CullMode mode;

    bool shouldCull(const ClipVert& v0, const ClipVert& v1, const ClipVert& v2) const {
      if (mode == Rasterizer::CullMode::Both)
        return false;

      // Tessellated primitives use CCW winding when viewed from the
      // outside. With the current camera projection, front-facing
      // triangles have negative projected area and back-facing
      // triangles have positive projected area.
      const double area = signedScreenArea(v0, v1, v2);
      if (area == 0.0)
        return false;

      return mode == Rasterizer::CullMode::Back ? area > 0.0 : area < 0.0;
    }
  };

  // Fragment payload after barycentric interpolation. Constructing it
  // is the handoff from coverage math to shading/depth tests.
  struct InterpolatedFragment {
    InterpolatedFragment(const RasterVertex& v0, const RasterVertex& v1, const RasterVertex& v2,
                         double w0b, double w1b, double w2b) {
      // Projective interpolation uses homogeneous clip.w, not
      // blindly camera-space depth. For pinhole projection w is the
      // eye-relative depth, so this is the usual 1/z correction. For
      // orthographic projection w is 1, so the same formula collapses
      // to ordinary linear interpolation. Shadow-map passes rely on
      // that second case because the light camera is orthographic.
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

  // Directional-light shadow maps use their own orthographic camera
  // math instead of render::OrthographicCamera so top-down lights do
  // not inherit Camera's fixed world-up degeneracy. The interface is
  // still Camera-shaped because the raster front end only needs
  // projectPointToClipSpace() plus a ViewPlane for clip->screen.
  class DirectionalShadowCamera : public render::Camera {
  public:
    DirectionalShadowCamera(const Vector3d& center, const Vector3d& lightDirection,
                            double halfExtent)
        : m_origin(center + lightDirection * (halfExtent * 2.0)),
          m_forward((-lightDirection).normalized()),
          m_halfExtent(halfExtent) {
      const Vector3d upCandidate =
        std::abs(m_forward * Vector3d::up()) > 0.95 ? Vector3d::forward() : Vector3d::up();
      m_right = (upCandidate ^ m_forward).normalized();
      m_up = (m_right ^ -m_forward).normalized();
    }

    Rayd rayForPixel(double, double, render::SampleStream&) const override {
      return Rayd::undefined;
    }

    std::shared_ptr<render::Camera> clone() const override {
      auto result =
        std::shared_ptr<DirectionalShadowCamera>(new DirectionalShadowCamera(
          m_origin, m_forward, m_right, m_up, m_halfExtent
        ));
      copyBaseStateTo(*result);
      return result;
    }

    Vector3d projectPointWithDepth(const Vector3d& worldPoint) const override {
      const Vector3d cameraPoint = toCameraSpace(worldPoint);
      if (cameraPoint.z() < 0.0)
        return Vector3d::undefined;

      const auto plane = viewPlane();
      return Vector3d((cameraPoint.x() / m_halfExtent + 1.0) * plane->width() / 2.0,
                      (cameraPoint.y() / m_halfExtent + 1.0) * plane->height() / 2.0,
                      cameraPoint.z());
    }

    Vector4d projectPointToClipSpace(const Vector3d& worldPoint) const override {
      const Vector3d cameraPoint = toCameraSpace(worldPoint);
      return Vector4d(cameraPoint.x() / m_halfExtent, cameraPoint.y() / m_halfExtent,
                      cameraPoint.z(), 1.0);
    }

  private:
    DirectionalShadowCamera(const Vector3d& origin, const Vector3d& forward,
                            const Vector3d& right, const Vector3d& up, double halfExtent)
        : m_origin(origin),
          m_forward(forward),
          m_right(right),
          m_up(up),
          m_halfExtent(halfExtent) {
    }

    Vector3d toCameraSpace(const Vector3d& worldPoint) const {
      const Vector3d rel = worldPoint - m_origin;
      return Vector3d(rel * m_right, rel * m_up, rel * m_forward);
    }

    Vector3d m_origin;
    Vector3d m_forward;
    Vector3d m_right;
    Vector3d m_up;
    double m_halfExtent;
  };

  class DirectionalShadowMap {
  public:
    DirectionalShadowMap(const render::Light* light,
                         std::shared_ptr<DirectionalShadowCamera> camera,
                         std::unique_ptr<Buffer<double>> depthBuffer, double bias, int filterRadius)
        : m_light(light),
          m_camera(std::move(camera)),
          m_depthBuffer(std::move(depthBuffer)),
          m_bias(bias),
          m_filterRadius(filterRadius) {
    }

    const render::Light* light() const {
      return m_light;
    }

    double visibility(const Vector3d& worldPos) const {
      const Vector3d shadowPixel = m_camera->projectPointWithDepth(worldPos);
      if (shadowPixel.isUndefined())
        return 1.0;

      const int x = static_cast<int>(std::lround(shadowPixel.x()));
      const int y = static_cast<int>(std::lround(shadowPixel.y()));

      if (m_filterRadius == 0)
        return sampleVisibility(x, y, shadowPixel.z());

      double litSamples = 0.0;
      int samples = 0;
      for (int dy = -m_filterRadius; dy <= m_filterRadius; ++dy) {
        for (int dx = -m_filterRadius; dx <= m_filterRadius; ++dx) {
          litSamples += sampleVisibility(x + dx, y + dy, shadowPixel.z());
          ++samples;
        }
      }
      return litSamples / static_cast<double>(samples);
    }

  private:
    double sampleVisibility(int x, int y, double receiverDepth) const {
      if (x < 0 || y < 0 || x >= m_depthBuffer->width() || y >= m_depthBuffer->height())
        return 1.0;

      const double occluderDepth = (*m_depthBuffer)[y][x];
      if (!std::isfinite(occluderDepth))
        return 1.0;

      return receiverDepth <= occluderDepth + m_bias ? 1.0 : 0.0;
    }

    const render::Light* m_light;
    std::shared_ptr<DirectionalShadowCamera> m_camera;
    std::unique_ptr<Buffer<double>> m_depthBuffer;
    double m_bias;
    int m_filterRadius;
  };

  class ShadowMaps {
  public:
    void add(DirectionalShadowMap shadowMap) {
      m_directional.push_back(std::move(shadowMap));
    }

    bool empty() const {
      return m_directional.empty();
    }

    double visibility(const render::Light* light, const Vector3d& worldPos) const {
      for (const auto& shadowMap : m_directional) {
        if (shadowMap.light() == light)
          return shadowMap.visibility(worldPos);
      }
      return 1.0;
    }

  private:
    std::vector<DirectionalShadowMap> m_directional;
  };

  // The default fragment path is intentionally modest: material
  // albedo plus direct Lambertian lights. More advanced visibility
  // effects belong to later render passes or to the ray/path tracers.
  class MaterialEvaluator {
  public:
    explicit MaterialEvaluator(const render::Scene* scene, const ShadowMaps* shadowMaps)
        : m_scene(scene),
          m_shadowMaps(shadowMaps) {
    }

    Colord shade(const RasterTriangle& triangle, const InterpolatedFragment& fragment) const {
      return shade(triangle.rasterMaterial, triangle.primitive, fragment.worldPos, fragment.normal,
                   fragment.uv);
    }

    Colord shade(const RasterMaterial& rasterMaterial, const render::Primitive* primitive,
                 const Vector3d& worldPos, const Vector3d& normal, const Vector2d& uv) const {
      const Vector3d n = normal.normalized();
      const Colord albedo = rasterMaterial.albedo(primitive, worldPos, n, uv);

      // Lambertian shading. Raster shadow maps, when enabled, only
      // mask direct diffuse light. Ambient remains visible because
      // it models light not explained by the direct-light pass.
      Colord shaded = m_scene->ambient() * kAmbientCoefficient * albedo;
      for (const auto& light : m_scene->lights()) {
        const Vector3d lightDir = light->direction(worldPos);
        const double nDotL = std::max(0.0, n * lightDir);
        if (nDotL > 0.0) {
          const double visibility =
            m_shadowMaps ? m_shadowMaps->visibility(light.get(), worldPos) : 1.0;
          if (visibility > 0.0)
            shaded += albedo * light->radiance() * nDotL * visibility;
        }
      }
      return shaded;
    }

  private:
    const render::Scene* m_scene;
    const ShadowMaps* m_shadowMaps;
  };

  // Small value objects capture fixed-function state at pass setup
  // time. The inner fragment loop then receives concrete policy
  // objects instead of asking the Rasterizer about booleans per pixel.
  // Pure depth comparison state. Kept separate from depth-buffer
  // ownership so write/read-only policies can share the comparison.
  struct DepthState {
    Rasterizer::DepthFunc func;

    inline bool pass(double incoming, double stored) const {
      switch (func) {
      case Rasterizer::DepthFunc::Never:
        return false;
      case Rasterizer::DepthFunc::Less:
        return incoming < stored;
      case Rasterizer::DepthFunc::Equal:
        return incoming == stored;
      case Rasterizer::DepthFunc::LessEqual:
        return incoming <= stored;
      case Rasterizer::DepthFunc::Greater:
        return incoming > stored;
      case Rasterizer::DepthFunc::GreaterEqual:
        return incoming >= stored;
      case Rasterizer::DepthFunc::NotEqual:
        return incoming != stored;
      case Rasterizer::DepthFunc::Always:
        return true;
      }
      return false;
    }
  };

  // Pure stencil state: compare function plus the update operations
  // for stencil-fail, depth-fail, and pass outcomes.
  struct StencilState {
    Rasterizer::StencilFunc func;
    std::uint8_t reference;
    std::uint8_t mask;
    std::uint8_t writeMask;
    Rasterizer::StencilOp failOp;
    Rasterizer::StencilOp depthFailOp;
    Rasterizer::StencilOp passOp;

    inline bool pass(std::uint8_t stored) const {
      const std::uint8_t lhs = reference & mask;
      const std::uint8_t rhs = stored & mask;
      switch (func) {
      case Rasterizer::StencilFunc::Never:
        return false;
      case Rasterizer::StencilFunc::Less:
        return lhs < rhs;
      case Rasterizer::StencilFunc::Equal:
        return lhs == rhs;
      case Rasterizer::StencilFunc::LessEqual:
        return lhs <= rhs;
      case Rasterizer::StencilFunc::Greater:
        return lhs > rhs;
      case Rasterizer::StencilFunc::GreaterEqual:
        return lhs >= rhs;
      case Rasterizer::StencilFunc::NotEqual:
        return lhs != rhs;
      case Rasterizer::StencilFunc::Always:
        return true;
      }
      return false;
    }

    inline std::uint8_t apply(Rasterizer::StencilOp op, std::uint8_t current) const {
      switch (op) {
      case Rasterizer::StencilOp::Keep:
        return current;
      case Rasterizer::StencilOp::Zero:
        return 0;
      case Rasterizer::StencilOp::Replace:
        return reference;
      case Rasterizer::StencilOp::IncrementClamp:
        return current == 0xFF ? current : static_cast<std::uint8_t>(current + 1);
      case Rasterizer::StencilOp::DecrementClamp:
        return current == 0 ? current : static_cast<std::uint8_t>(current - 1);
      case Rasterizer::StencilOp::Invert:
        return static_cast<std::uint8_t>(~current);
      }
      return current;
    }

    inline std::uint8_t update(Rasterizer::StencilOp op, std::uint8_t current) const {
      const std::uint8_t updated = apply(op, current);
      return static_cast<std::uint8_t>((current & ~writeMask) | (updated & writeMask));
    }
  };

  // Policy objects: C++ templates select "no stencil" vs "stencil",
  // "write depth" vs "read-only depth", and "built-in" vs "shader"
  // before entering the tile loops. The generated inner loop has
  // direct calls and can inline the chosen behavior.
  // Null object for disabled stencil. Same interface as the real
  // policy, so the inner loop does not branch on "is stencil enabled".
  struct NoStencilPolicy {
    inline bool pass(int, int) const {
      return true;
    }
    inline void onStencilFail(int, int) const {
    }
    inline void onDepthFail(int, int) const {
    }
    inline void onPass(int, int) const {
    }
  };

  // Real stencil policy: owns access to the pass stencil buffer and
  // applies the configured operation at each fragment outcome.
  template<class BufferView>
  struct RasterStencilPolicy {
    BufferView stencilBuffer;
    StencilState state;

    inline bool pass(int x, int y) const {
      return state.pass(stencilBuffer.at(x, y));
    }

    inline void onStencilFail(int x, int y) const {
      update(x, y, state.failOp);
    }

    inline void onDepthFail(int x, int y) const {
      update(x, y, state.depthFailOp);
    }

    inline void onPass(int x, int y) const {
      update(x, y, state.passOp);
    }

  private:
    inline void update(int x, int y, Rasterizer::StencilOp op) const {
      stencilBuffer.at(x, y) = state.update(op, stencilBuffer.at(x, y));
    }
  };

  // Normal depth policy: compare incoming depth, then commit passing
  // fragments back into the z-buffer.
  template<class BufferView>
  struct DepthWritePolicy {
    BufferView zBuffer;
    DepthState state;

    inline bool pass(int x, int y, double depth) const {
      return state.pass(depth, zBuffer.at(x, y));
    }

    inline void write(int x, int y, double depth) const {
      zBuffer.at(x, y) = depth;
    }
  };

  // Depth-test-only policy. Useful for passes that should respect
  // existing depth without modifying it.
  template<class BufferView>
  struct DepthReadOnlyPolicy {
    BufferView zBuffer;
    DepthState state;

    inline bool pass(int x, int y, double depth) const {
      return state.pass(depth, zBuffer.at(x, y));
    }

    inline void write(int, int, double) const {
    }
  };

  // Built-in shader policy: material lookup and direct Lambertian
  // shading. This is the default fixed-function fragment stage.
  struct BuiltInFragmentPolicy {
    MaterialEvaluator materialEvaluator;

    inline Colord shade(const RasterTriangle& triangle, int, int, double, double, double,
                        const InterpolatedFragment& fragment) const {
      return materialEvaluator.shade(triangle, fragment);
    }
  };

  // User fragment-shader policy. It adapts the internal fragment
  // payload to Rasterizer::FragmentInput and calls the callback.
  struct ShaderFragmentPolicy {
    const Rasterizer& rasterizer;

    inline Colord shade(const RasterTriangle& triangle, int x, int y, double w0b, double w1b,
                        double w2b, const InterpolatedFragment& fragment) const {
      const auto& shader = rasterizer.fragmentShader();
      const Vector3d n = fragment.normal.normalized();
      const Rasterizer::FragmentInput input{
        x, y,           fragment.depth,     Vector3d(w0b, w1b, w2b), fragment.worldPos,
        n, fragment.uv, triangle.primitive, triangle.material.get(), triangle.faceIdx};
      return shader(input);
    }
  };

  // Shadow-map pass fragment policy. It exists only to satisfy the
  // shared raster loop's "shade then write color" contract; the
  // useful output of the pass is the depth policy's z-buffer write.
  struct DepthOnlyFragmentPolicy {
    inline Colord shade(const RasterTriangle&, int, int, double, double, double,
                        const InterpolatedFragment&) const {
      return Colord::black();
    }
  };

  template<class ColorBuffer, class Stencil, class Depth, class Fragment>
  inline void rasterizePreparedTriangleWithPolicies(const RasterTriangle& triangle,
                                                    const Recti& clipRect, ColorBuffer colorBuffer,
                                                    const Vector2d& sampleOffset, Stencil stencil,
                                                    Depth depth, Fragment fragmentPolicy) {
    const RasterVertex& v0 = triangle.vertices[0];
    const RasterVertex& v1 = triangle.vertices[1];
    const RasterVertex& v2 = triangle.vertices[2];

    // Hot loop boundary: core::rasterizeTriangleSampled supplies
    // covered pixels and barycentric weights; the policies decide
    // stencil/depth/shading without virtual dispatch.
    core::rasterizeTriangleSampled(
      v0.x, v0.y, v1.x, v1.y, v2.x, v2.y, clipRect.left(), clipRect.top(), clipRect.right(),
      clipRect.bottom(), sampleOffset.x(), sampleOffset.y(),
      [&](int x, int y, double w0b, double w1b, double w2b) {
        if (!stencil.pass(x, y)) {
          stencil.onStencilFail(x, y);
          return;
        }

        const InterpolatedFragment fragment(v0, v1, v2, w0b, w1b, w2b);
        if (!depth.pass(x, y, fragment.depth)) {
          stencil.onDepthFail(x, y);
          return;
        }

        stencil.onPass(x, y);
        const Colord shaded = fragmentPolicy.shade(triangle, x, y, w0b, w1b, w2b, fragment);
        depth.write(x, y, fragment.depth);
        colorBuffer.at(x, y) = shaded;
      });
  }

  template<class ColorBuffer, class Stencil, class Depth, class Fragment>
  inline void rasterizeTileWithPolicies(const RasterTriangleSet& triangleSet, const Recti& rect,
                                        std::size_t tileIndex, ColorBuffer colorBuffer,
                                        const Vector2d& sampleOffset,
                                        const std::atomic<bool>& cancelled, Stencil stencil,
                                        Depth depth, Fragment fragmentPolicy) {
    const auto& triangles = triangleSet.triangles();
    const auto& triangleIndices = triangleSet.tileGrid().triangleIndices(tileIndex);
    for (const std::size_t triangleIndex : triangleIndices) {
      if (cancelled.load())
        return;
      rasterizePreparedTriangleWithPolicies(triangles[triangleIndex], rect, colorBuffer,
                                            sampleOffset, stencil, depth, fragmentPolicy);
    }
  }

  template<class ColorBuffer, class Stencil, class Depth, class Fragment>
  inline void rasterizeTriangleSetWithPolicies(
    const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan, QThreadPool& threadPool,
    std::list<std::shared_ptr<engine::TileRenderTask>>& tasks, const std::atomic<bool>& cancelled,
    ColorBuffer colorBuffer, const Vector2d& sampleOffset, Stencil stencil, Depth depth,
    Fragment fragmentPolicy) {
    if (tilePlan.isSingleTile()) {
      // Avoid QRunnable overhead for the common single-tile path.
      rasterizeTileWithPolicies(triangleSet, tilePlan.fullRect(), 0, colorBuffer, sampleOffset,
                                cancelled, stencil, depth, fragmentPolicy);
      return;
    }

    engine::dispatchTileTasks(
      tilePlan, threadPool, tasks,
      [&, sampleOffset, stencil, depth, fragmentPolicy](const Recti& rect, std::size_t tileIndex) {
        rasterizeTileWithPolicies(triangleSet, rect, tileIndex, colorBuffer, sampleOffset,
                                  cancelled, stencil, depth, fragmentPolicy);
      });
  }

  template<class DepthBuffer, class Stencil, class Fragment, class RenderFn>
  inline void withPreparedTriangleDepthPolicy(const Rasterizer& rasterizer, DepthBuffer zBuffer,
                                              Stencil stencil, Fragment fragmentPolicy,
                                              RenderFn&& render) {
    const DepthState depthState{rasterizer.depthFunc()};
    if (rasterizer.depthWriteEnabled()) {
      render(stencil, DepthWritePolicy<DepthBuffer>{zBuffer, depthState}, fragmentPolicy);
    } else {
      render(stencil, DepthReadOnlyPolicy<DepthBuffer>{zBuffer, depthState}, fragmentPolicy);
    }
  }

  template<class DepthBuffer, class StencilBuffer, class RenderFn>
  inline void withPreparedTrianglePolicies(const render::Scene* scene, const Rasterizer& rasterizer,
                                           const ShadowMaps& shadowMaps, DepthBuffer zBuffer,
                                           StencilBuffer stencilBuffer, RenderFn&& render) {
    const bool useStencil = rasterizer.stencilTestEnabled();
    const bool useFragmentShader = static_cast<bool>(rasterizer.fragmentShader());

    // One dispatch tree per pass, not per pixel. This is the bridge
    // from runtime engine state to compile-time policy objects.
    if (useStencil) {
      const StencilState stencilState{rasterizer.stencilFunc(),   rasterizer.stencilReference(),
                                      rasterizer.stencilMask(),   rasterizer.stencilWriteMask(),
                                      rasterizer.stencilFailOp(), rasterizer.stencilDepthFailOp(),
                                      rasterizer.stencilPassOp()};
      RasterStencilPolicy<StencilBuffer> stencil{stencilBuffer, stencilState};
      if (useFragmentShader) {
        withPreparedTriangleDepthPolicy(rasterizer, zBuffer, stencil,
                                        ShaderFragmentPolicy{rasterizer}, render);
      } else {
        withPreparedTriangleDepthPolicy(rasterizer, zBuffer, stencil,
                                        BuiltInFragmentPolicy{MaterialEvaluator(
                                          scene, shadowMaps.empty() ? nullptr : &shadowMaps)},
                                        render);
      }
    } else if (useFragmentShader) {
      withPreparedTriangleDepthPolicy(rasterizer, zBuffer, NoStencilPolicy{},
                                      ShaderFragmentPolicy{rasterizer}, render);
    } else {
      withPreparedTriangleDepthPolicy(
        rasterizer, zBuffer, NoStencilPolicy{},
        BuiltInFragmentPolicy{MaterialEvaluator(scene, shadowMaps.empty() ? nullptr : &shadowMaps)},
        render);
    }
  }

  // Scene traversal + primitive tessellation + clipping. This is the
  // "front end" of the software raster pipeline; it streams prepared
  // triangles without owning the final batch.
  class RasterTriangleEmitter {
  public:
    RasterTriangleEmitter(const render::Scene* scene, const std::shared_ptr<render::Camera>& camera,
                          int lod, const Rasterizer& rasterizer, const std::atomic<bool>& cancelled,
                          Rasterizer::CullMode cullMode, bool applyVertexShader)
        : m_scene(scene),
          m_camera(camera),
          m_lod(lod),
          m_rasterizer(rasterizer),
          m_cullPolicy{cullMode},
          m_applyVertexShader(applyVertexShader),
          m_cancelled(cancelled) {
    }

    template<class EmitFn>
    void forEachTriangle(EmitFn&& callback) const {
      std::uint64_t globalFaceIdx = 0;
      m_scene->forEachLeaf(
        [&](const render::Primitive* primitive, std::shared_ptr<render::Material> material) {
          if (m_cancelled.load())
            return;

          auto mesh = primitive->tessellate(m_lod);
          if (!mesh)
            return;

          const auto& vertices = mesh->vertices();
          const auto& faces = mesh->faces();
          const auto& viewPlane = *m_camera->viewPlane();
          const RasterMaterialSource materialSource = RasterMaterialSource::from(material);

          // Project each mesh vertex once per primitive. Faces then
          // reuse clip/screen data while fan-triangulating polygons.
          std::vector<ProjectedVertex> projected(vertices.size());
          for (std::size_t vi = 0; vi < vertices.size(); ++vi) {
            const auto& vertex = vertices[vi];
            const Vector4d clip = m_camera->projectPointToClipSpace(vertex.point);
            const std::uint8_t outCode = clipVolume().outCode(clip);
            projected[vi] = {
              clip, outCode == 0 ? viewPlane.screenFromClipUnchecked(clip) : Vector3d::undefined,
              outCode};
          }

          for (std::size_t fi = 0; fi < faces.size(); ++fi, ++globalFaceIdx) {
            if (m_cancelled.load())
              return;

            const auto& face = faces[fi];
            if (face.size() < 3)
              continue;

            for (std::size_t i = 1; i + 1 < face.size(); ++i) {
              const ProjectedVertex& p0 = projected[face[0]];
              const ProjectedVertex& p1 = projected[face[i]];
              const ProjectedVertex& p2 = projected[face[i + 1]];
              // Cohen-Sutherland-style bit tests: shared outside
              // bits reject a triangle before invoking the clipper.
              if ((p0.outCode & p1.outCode & p2.outCode) != 0) {
                continue;
              }

              const std::uint8_t outCodeOr = p0.outCode | p1.outCode | p2.outCode;
              if (outCodeOr == 0) {
                ClipVert v0{vertices[face[0]].point, vertices[face[0]].normal, vertices[face[0]].uv,
                            p0.clip, p0.screen};
                ClipVert v1{vertices[face[i]].point, vertices[face[i]].normal, vertices[face[i]].uv,
                            p1.clip, p1.screen};
                ClipVert v2{vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                            vertices[face[i + 1]].uv, p2.clip, p2.screen};

                emitPreparedTriangle(primitive, material, materialSource, globalFaceIdx, v0, v1, v2,
                                     callback);
                continue;
              }

              // Sutherland-Hodgman homogeneous clipping. The camera
              // gives us un-divided clip coordinates, so the same
              // polygon clipper handles the near plane and the four
              // viewport edges before any perspective divide can blow
              // up screen coordinates.
              const std::array<ClipVert, 3> input = {{
                {vertices[face[0]].point, vertices[face[0]].normal, vertices[face[0]].uv, p0.clip,
                 p0.screen},
                {vertices[face[i]].point, vertices[face[i]].normal, vertices[face[i]].uv, p1.clip,
                 p1.screen},
                {vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                 vertices[face[i + 1]].uv, p2.clip, p2.screen},
              }};

              ClipPolygon clipped;
              const std::size_t clippedCount = clipTriangleToView(input, clipped);
              if (clippedCount < 3)
                continue;

              for (std::size_t t = 1; t + 1 < clippedCount; ++t) {
                ClipVert v0 = clipped[0];
                ClipVert v1 = clipped[t];
                ClipVert v2 = clipped[t + 1];

                if (!v0.ensureScreen(viewPlane) || !v1.ensureScreen(viewPlane) ||
                    !v2.ensureScreen(viewPlane)) {
                  continue;
                }

                emitPreparedTriangle(primitive, material, materialSource, globalFaceIdx, v0, v1, v2,
                                     callback);
              }
            }
          }
        });
    }

  private:
    template<class EmitFn>
    void emitPreparedTriangle(const render::Primitive* primitive,
                              const std::shared_ptr<render::Material>& material,
                              const RasterMaterialSource& materialSource, std::uint64_t faceIdx,
                              const ClipVert& v0, const ClipVert& v1, const ClipVert& v2,
                              EmitFn& callback) const {
      if (m_cullPolicy.shouldCull(v0, v1, v2)) {
        return;
      }

      RasterTriangle triangle;
      if (makeTriangle(v0, v1, v2, primitive, material, materialSource, faceIdx, triangle)) {
        callback(triangle);
      }
    }

    bool makeVertex(const ClipVert& vertex, const render::Primitive* primitive,
                    const render::Material* material, std::uint64_t faceIdx,
                    RasterVertex& out) const {
      Vector3d point = vertex.point;
      Vector3d normal = vertex.normal;
      Vector2d uv = vertex.uv;
      Vector3d screen = vertex.screen;

      // The optional vertex shader is deliberately late: it sees
      // already-clipped vertices and may adjust the screen-space
      // result used by the teaching/debug shader path.
      if (m_applyVertexShader) {
        if (const auto& shader = m_rasterizer.vertexShader()) {
          Rasterizer::VertexInput input{vertex.point,  vertex.normal, vertex.uv, vertex.clip,
                                        vertex.screen, primitive,     material,  faceIdx};
          const Rasterizer::VertexOutput output = shader(input);
          point = output.worldPosition;
          normal = output.normal;
          uv = output.uv;
          screen = output.screenPosition;
        }
      }

      if (screen.isUndefined() || screen.z() <= 0.0)
        return false;

      const double clipW = vertex.clip.isDefined() ? vertex.clip.w() : screen.z();
      if (clipW <= 0.0)
        return false;

      const double invW = 1.0 / clipW;
      out = {point,
             normal,
             uv,
             invW,
             screen.z() * invW,
             static_cast<int>(std::lround(screen.x())),
             static_cast<int>(std::lround(screen.y()))};
      return true;
    }

    bool makeTriangle(const ClipVert& v0, const ClipVert& v1, const ClipVert& v2,
                      const render::Primitive* primitive,
                      const std::shared_ptr<render::Material>& material,
                      const RasterMaterialSource& materialSource, std::uint64_t faceIdx,
                      RasterTriangle& out) const {
      RasterVertex r0, r1, r2;
      const render::Material* materialPtr = material.get();
      if (!makeVertex(v0, primitive, materialPtr, faceIdx, r0) ||
          !makeVertex(v1, primitive, materialPtr, faceIdx, r1) ||
          !makeVertex(v2, primitive, materialPtr, faceIdx, r2)) {
        return false;
      }

      out = RasterTriangle{
        {{r0, r1, r2}}, primitive, material, materialSource.forFace(faceIdx), faceIdx};
      return true;
    }

    const render::Scene* m_scene;
    const std::shared_ptr<render::Camera>& m_camera;
    int m_lod;
    const Rasterizer& m_rasterizer;
    TriangleCullPolicy m_cullPolicy;
    bool m_applyVertexShader;
    const std::atomic<bool>& m_cancelled;
  };

}

RasterTriangleSet
Rasterizer::Private::collectRasterTriangles(const RasterTriangleEmitter& triangleEmitter,
                                            const render::TilePlan& tilePlan) {
  // The emitter streams triangles, the set owns them and their tile
  // bins. Keeping those roles separate makes the later tile raster
  // pass independent of scene traversal and tessellation.
  RasterTriangleSet triangleSet(tilePlan);
  triangleEmitter.forEachTriangle(
    [&](const RasterTriangle& triangle) { triangleSet.add(triangle); });
  return triangleSet;
}

void Rasterizer::Private::accumulateSample(Buffer<Colord>& target, const Buffer<Colord>& sample) {
  for (int y = 0; y < target.height(); ++y)
    for (int x = 0; x < target.width(); ++x)
      target[y][x] += sample[y][x];
}

void Rasterizer::Private::resolveMSAA(Buffer<Colord>& buffer, int sampleCount) {
  const double resolveScale = 1.0 / static_cast<double>(sampleCount);
  for (int y = 0; y < buffer.height(); ++y)
    for (int x = 0; x < buffer.width(); ++x)
      buffer[y][x] = buffer[y][x] * resolveScale;
}

void Rasterizer::Private::resolveMSAATile(Buffer<Colord>& buffer, const Buffer<Colord>& accumulated,
                                          const Recti& rect, int sampleCount) {
  const double resolveScale = 1.0 / static_cast<double>(sampleCount);
  for (int y = 0; y < accumulated.height(); ++y)
    for (int x = 0; x < accumulated.width(); ++x)
      buffer[rect.top() + y][rect.left() + x] = accumulated[y][x] * resolveScale;
}

ShadowMaps Rasterizer::Private::buildShadowMaps(const Rasterizer& rasterizer,
                                                const std::shared_ptr<render::Scene>& scene,
                                                const std::atomic<bool>& cancelled) {
  ShadowMaps shadowMaps;
  if (!rasterizer.shadowMapsEnabled() || rasterizer.fragmentShader() || cancelled.load())
    return shadowMaps;

  const BoundingBoxd bounds = scene->boundingBox();
  if (!bounds.isValid() || bounds.isUndefined() || bounds.isInfinite())
    return shadowMaps;

  const int size = rasterizer.shadowMapSize();
  const double halfExtent = std::max(1.0, bounds.size().length() * 0.5) * 1.05;
  const Vector3d center = bounds.center();

  for (const auto& light : scene->lights()) {
    if (cancelled.load())
      break;

    auto directional = std::dynamic_pointer_cast<render::DirectionalLight>(light);
    if (!directional)
      continue;

    auto shadowCamera =
      std::make_shared<DirectionalShadowCamera>(center, directional->direction(), halfExtent);
    shadowCamera->setViewPlane(std::make_shared<render::ViewPlane>());
    shadowCamera->viewPlane()->setup(Matrix4d(), Recti(size, size));

    auto depthBuffer = std::make_unique<Buffer<double>>(size, size);
    depthBuffer->clear(std::numeric_limits<double>::infinity());

    Buffer<Colord> scratch(size, size);
    scratch.clear(Colord::black());
    const render::TilePlan shadowTilePlan = render::TilePlan::forBuffer(size, size, 1);
    RasterTriangleEmitter shadowEmitter(scene.get(), shadowCamera, rasterizer.lod(), rasterizer,
                                        cancelled, Rasterizer::CullMode::Both, false);
    const RasterTriangleSet shadowTriangles = collectRasterTriangles(shadowEmitter, shadowTilePlan);
    if (!shadowTriangles.empty()) {
      std::list<std::shared_ptr<engine::TileRenderTask>> shadowTasks;
      rasterizeTriangleSetWithPolicies(
        shadowTriangles, shadowTilePlan, *threadPool, shadowTasks, cancelled,
        fullBufferView(scratch), Vector2d(0.0, 0.0), NoStencilPolicy{},
        DepthWritePolicy<RasterFullBufferView<double>>{fullBufferView(*depthBuffer),
                                                       DepthState{Rasterizer::DepthFunc::Less}},
        DepthOnlyFragmentPolicy{});
    }

    shadowMaps.add(DirectionalShadowMap(light.get(), shadowCamera, std::move(depthBuffer),
                                        rasterizer.shadowBias(), rasterizer.shadowFilterRadius()));
  }

  return shadowMaps;
}

void Rasterizer::Private::renderTriangleSetPass(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
  const ShadowMaps& shadowMaps, const std::atomic<bool>& cancelled, Buffer<Colord>& buffer,
  const Vector2d& sampleOffset) {
  // A pass freezes current depth/stencil/shader state into policies,
  // then hands the triangle set to either the direct or tiled path.
  PassBuffers passBuffers(rasterizer, tilePlan, buffer);
  RasterFullBufferView<std::uint8_t> stencilView;
  if (passBuffers.stencil()) {
    stencilView = fullBufferView(*passBuffers.stencil());
  }
  withPreparedTrianglePolicies(
    scene.get(), rasterizer, shadowMaps, fullBufferView(passBuffers.depth()), stencilView,
    [&](auto stencil, auto depth, auto fragmentPolicy) {
      rasterizeTriangleSetWithPolicies(triangleSet, tilePlan, *threadPool, tasks, cancelled,
                                       fullBufferView(passBuffers.color()), sampleOffset, stencil,
                                       depth, fragmentPolicy);
    });
}

void Rasterizer::Private::renderTriangleStreamPass(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const RasterTriangleEmitter& triangleEmitter, const render::TilePlan& tilePlan,
  const ShadowMaps& shadowMaps, const std::atomic<bool>& cancelled, Buffer<Colord>& buffer,
  const Vector2d& sampleOffset) {
  // The ordinary 1x single-tile path does not need a retained triangle
  // batch or tile bins. Freeze render state once, then draw each emitted
  // triangle immediately into the full-frame pass buffers.
  PassBuffers passBuffers(rasterizer, tilePlan, buffer);
  auto colorView = fullBufferView(passBuffers.color());
  auto depthView = fullBufferView(passBuffers.depth());
  RasterFullBufferView<std::uint8_t> stencilView;
  if (passBuffers.stencil()) {
    stencilView = fullBufferView(*passBuffers.stencil());
  }

  const Recti clipRect = tilePlan.fullRect();
  withPreparedTrianglePolicies(
    scene.get(), rasterizer, shadowMaps, depthView, stencilView,
    [&](auto stencil, auto depth, auto fragmentPolicy) {
      triangleEmitter.forEachTriangle([&](const RasterTriangle& triangle) {
        if (cancelled.load())
          return;
        rasterizePreparedTriangleWithPolicies(triangle, clipRect, colorView, sampleOffset, stencil,
                                              depth, fragmentPolicy);
      });
    });
}

void Rasterizer::Private::renderSingleSampleFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& tilePlan, const RasterTriangleEmitter& triangleEmitter,
  const ShadowMaps& shadowMaps, const std::atomic<bool>& cancelled, Buffer<Colord>& buffer) {
  if (tilePlan.isSingleTile()) {
    renderTriangleStreamPass(rasterizer, scene, triangleEmitter, tilePlan, shadowMaps, cancelled,
                             buffer, Vector2d(0.0, 0.0));
    return;
  }

  const RasterTriangleSet triangleSet = collectRasterTriangles(triangleEmitter, tilePlan);
  if (cancelled.load() || triangleSet.empty())
    return;

  renderTriangleSetPass(rasterizer, scene, triangleSet, tilePlan, shadowMaps, cancelled, buffer,
                        Vector2d(0.0, 0.0));
}

void Rasterizer::Private::renderMSAAFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& tilePlan, const SamplePattern& pattern,
  const RasterTriangleEmitter& triangleEmitter, const ShadowMaps& shadowMaps,
  const std::atomic<bool>& cancelled, Buffer<Colord>& buffer) {
  const RasterTriangleSet triangleSet = collectRasterTriangles(triangleEmitter, tilePlan);
  if (cancelled.load() || triangleSet.empty())
    return;

  if (tilePlan.isSingleTile()) {
    renderMSAAFullFrame(rasterizer, scene, triangleSet, tilePlan, shadowMaps, pattern, cancelled,
                        buffer);
    return;
  }

  engine::dispatchTileTasks(tilePlan, *threadPool, tasks,
                            [&](const Recti& rect, std::size_t tileIndex) {
                              renderMSAATile(rasterizer, scene, triangleSet, shadowMaps, pattern,
                                             rect, tileIndex, cancelled, buffer);
                            });
}

void Rasterizer::Private::renderMSAAFullFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
  const ShadowMaps& shadowMaps, const SamplePattern& pattern, const std::atomic<bool>& cancelled,
  Buffer<Colord>& buffer) {
  buffer.clear(Colord::black());
  for (int sampleIndex = 0; sampleIndex != pattern.count; ++sampleIndex) {
    if (cancelled.load())
      return;

    Buffer<Colord> sampleBuffer(tilePlan.width(), tilePlan.height());
    sampleBuffer.clear(rasterizer.backgroundColor());

    renderTriangleSetPass(rasterizer, scene, triangleSet, tilePlan, shadowMaps, cancelled,
                          sampleBuffer, pattern.offsets[sampleIndex]);

    if (cancelled.load())
      return;
    accumulateSample(buffer, sampleBuffer);
  }

  resolveMSAA(buffer, pattern.count);
}

void Rasterizer::Private::renderMSAATile(const Rasterizer& rasterizer,
                                         const std::shared_ptr<render::Scene>& scene,
                                         const RasterTriangleSet& triangleSet,
                                         const ShadowMaps& shadowMaps, const SamplePattern& pattern,
                                         const Recti& rect, std::size_t tileIndex,
                                         const std::atomic<bool>& cancelled,
                                         Buffer<Colord>& buffer) {
  if (rect.width() <= 0 || rect.height() <= 0)
    return;

  Buffer<Colord> accumulated(rect.width(), rect.height());
  accumulated.clear(Colord::black());

  for (int sampleIndex = 0; sampleIndex != pattern.count; ++sampleIndex) {
    if (cancelled.load())
      return;

    // This is simple supersampling scoped to one tile: rerun
    // coverage/depth at a fixed subpixel offset, accumulate local
    // colors, and resolve the tile into the output framebuffer.
    Buffer<Colord> sampleBuffer(rect.width(), rect.height());
    sampleBuffer.clear(rasterizer.backgroundColor());

    Buffer<double> depthBuffer(rect.width(), rect.height());
    depthBuffer.clear(rasterizer.depthClearValue());

    std::unique_ptr<Buffer<std::uint8_t>> stencilBuffer;
    RasterTileBufferView<std::uint8_t> stencilView;
    if (rasterizer.stencilTestEnabled()) {
      stencilBuffer = std::make_unique<Buffer<std::uint8_t>>(rect.width(), rect.height());
      stencilBuffer->clear(rasterizer.stencilClearValue());
      stencilView = tileBufferView(*stencilBuffer, rect);
    }

    withPreparedTrianglePolicies(
      scene.get(), rasterizer, shadowMaps, tileBufferView(depthBuffer, rect), stencilView,
      [&](auto stencil, auto depth, auto fragmentPolicy) {
        rasterizeTileWithPolicies(triangleSet, rect, tileIndex, tileBufferView(sampleBuffer, rect),
                                  pattern.offsets[sampleIndex], cancelled, stencil, depth,
                                  fragmentPolicy);
      });

    if (cancelled.load())
      return;
    accumulateSample(accumulated, sampleBuffer);
  }

  resolveMSAATile(buffer, accumulated, rect, pattern.count);
}

void Rasterizer::Private::renderFrame(const Rasterizer& rasterizer,
                                      const std::shared_ptr<render::Scene>& scene,
                                      const std::shared_ptr<render::Camera>& camera,
                                      const std::atomic<bool>& cancelled, Buffer<Colord>& buffer) {
  const int width = buffer.width();
  const int height = buffer.height();

  if (width <= 0 || height <= 0 || cancelled.load())
    return;

  const render::TilePlan tilePlan = render::TilePlan::forBuffer(width, height, queueSize);
  const SamplePattern pattern(rasterizer.msaaSamples());
  const RasterTriangleEmitter triangleEmitter(scene.get(), camera, rasterizer.lod(), rasterizer,
                                              cancelled, rasterizer.cullMode(), true);
  const ShadowMaps shadowMaps = buildShadowMaps(rasterizer, scene, cancelled);
  if (pattern.count > 1) {
    renderMSAAFrame(rasterizer, scene, tilePlan, pattern, triangleEmitter, shadowMaps, cancelled,
                    buffer);
  } else {
    renderSingleSampleFrame(rasterizer, scene, tilePlan, triangleEmitter, shadowMaps, cancelled,
                            buffer);
  }

  if (!cancelled.load() && rasterizer.postProcessAA() == Rasterizer::PostProcessAA::FXAA) {
    render::postprocess::applyFxaa(buffer);
  }
}
