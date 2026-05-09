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
#include "render/lights/Light.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Scene.h"
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
#include <memory>
#include <vector>

using namespace engine::raster;

namespace {
  class RasterTriangleEmitter;
  class RasterTriangleSet;
}

struct Rasterizer::Private {
  Private()
      : threadPool(std::make_unique<QThreadPool>()),
        queueSize(1) {
    threadPool->setMaxThreadCount(std::max(1, QThread::idealThreadCount()));
  }

  std::unique_ptr<QThreadPool> threadPool;
  std::list<std::shared_ptr<engine::TileRenderTask>> tasks;
  int queueSize;

  struct SamplePattern {
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
                               const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

  void renderMSAAFrame(const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
                       const render::TilePlan& tilePlan, const SamplePattern& pattern,
                       const RasterTriangleEmitter& triangleEmitter,
                       const std::atomic<bool>& cancelled, Buffer<Colord>& buffer);

  void renderTriangleSetPass(const Rasterizer& rasterizer,
                             const std::shared_ptr<render::Scene>& scene,
                             const RasterTriangleSet& triangleSet,
                             const render::TilePlan& tilePlan,
                             const std::atomic<bool>& cancelled, Buffer<Colord>& buffer,
                             const Vector2d& sampleOffset);

  static RasterTriangleSet collectRasterTriangles(const RasterTriangleEmitter& triangleEmitter,
                                                  const render::TilePlan& tilePlan);
  static void accumulateSample(Buffer<Colord>& target, const Buffer<Colord>& sample);
  static void resolveMSAA(Buffer<Colord>& buffer, int sampleCount);
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

void Rasterizer::cancel() {
  m_cancelled.store(true);
}

void Rasterizer::uncancel() {
  m_cancelled.store(false);
}

std::list<Recti> Rasterizer::activeRects() const {
  std::list<Recti> result;
  for (const auto& task : p->tasks) {
    if (task->active) {
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

void Rasterizer::render(Buffer<Colord>& buffer) {
  // Caller is expected to call uncancel() between renders. Matches
  // the Wireframe / Raytracer convention.

  // Clear to the configured background before depth-tested fragments overwrite it.
  buffer.clear(m_backgroundColor);

  if (!m_scene || !m_camera)
    return;

  // Same view-plane setup the other engines perform — the camera
  // projection math depends on the cached basis vectors.
  m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());

  p->tasks.clear();
  p->renderFrame(*this, m_scene, m_camera, m_cancelled, buffer);
}

namespace {
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

  struct ProjectedVertex {
    Vector4d clip;
    Vector3d screen;
    std::uint8_t outCode;
  };

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

  struct RasterVertex {
    Vector3d point;
    Vector3d normal;
    Vector2d uv;
    double invZ;
    int x;
    int y;
  };

  struct RasterTriangle {
    std::array<RasterVertex, 3> vertices;
    const render::Primitive* primitive;
    std::shared_ptr<render::Material> material;
    std::uint64_t faceIdx;
  };

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
      if (rawMaxX < 0 || rawMaxY < 0 || rawMinX >= m_plan.width() ||
          rawMinY >= m_plan.height()) {
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

  using ClipPolygon = std::array<ClipVert, kMaxClipVertices>;

  const render::HomogeneousClipVolume& clipVolume() {
    static const render::HomogeneousClipVolume volume(kNearClipDepth);
    return volume;
  }

  inline ClipVert interpolateClipVert(const ClipVert& from, const ClipVert& to, double t) {
    return {from.point + (to.point - from.point) * t, from.normal + (to.normal - from.normal) * t,
            from.uv + (to.uv - from.uv) * t, from.clip + (to.clip - from.clip) * t,
            Vector3d::undefined()};
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

  struct InterpolatedFragment {
    InterpolatedFragment(const RasterVertex& v0, const RasterVertex& v1, const RasterVertex& v2,
                         double w0b, double w1b, double w2b) {
      // Perspective-correct depth interpolation. The screen-space
      // barycentric weights from `rasterizeTriangle` are linear in
      // screen space — but vertex *depth* is not. The standard trick:
      // 1/z IS linear in screen space, so interpolate 1/z and invert.
      // (Heckbert & Moreton 1991.)
      const double oneOverZ = w0b * v0.invZ + w1b * v1.invZ + w2b * v2.invZ;
      depth = 1.0 / oneOverZ;

      // Perspective-correct attribute interpolation: same trick as
      // depth, applied to vertex normals, world positions, and UVs:
      //   attr_pixel = (Σ_i w_i · attr_i / z_i) · pixelDepth
      const double wp0 = w0b * v0.invZ;
      const double wp1 = w1b * v1.invZ;
      const double wp2 = w2b * v2.invZ;
      worldPos = (v0.point * wp0 + v1.point * wp1 + v2.point * wp2) * depth;
      normal = (v0.normal * wp0 + v1.normal * wp1 + v2.normal * wp2) * depth;
      uv = (v0.uv * wp0 + v1.uv * wp1 + v2.uv * wp2) * depth;
    }

    double depth;
    Vector3d worldPos;
    Vector3d normal;
    Vector2d uv;
  };

  class MaterialEvaluator {
  public:
    explicit MaterialEvaluator(const render::Scene* scene)
        : m_scene(scene) {
    }

    Colord shade(const RasterTriangle& triangle, const InterpolatedFragment& fragment) const {
      return shade(triangle.material, triangle.primitive, fragment.worldPos, fragment.normal,
                   fragment.uv, triangle.faceIdx);
    }

    Colord shade(const std::shared_ptr<render::Material>& material,
                 const render::Primitive* primitive, const Vector3d& worldPos,
                 const Vector3d& normal, const Vector2d& uv, std::uint64_t faceIdx) const {
      const Vector3d n = normal.normalized();
      const Colord albedo = albedoFor(material, primitive, worldPos, n, uv, faceIdx);

      // Lambertian shading. No shadow rays (no recursive ray tracing
      // in this engine); each light contributes diffuse-cosine-
      // weighted radiance directly.
      Colord shaded = m_scene->ambient() * kAmbientCoefficient * albedo;
      for (const auto& light : m_scene->lights()) {
        const Vector3d lightDir = light->direction(worldPos);
        const double nDotL = std::max(0.0, n * lightDir);
        if (nDotL > 0.0) {
          shaded += albedo * light->radiance() * nDotL;
        }
      }
      return shaded;
    }

  private:
    static Colord faceColor(std::uint64_t index) {
      const std::uint64_t r = (index * 2654435761ULL) & 0xFFu;
      const std::uint64_t g = (index * 40503ULL + 12345) & 0xFFu;
      const std::uint64_t b = (index * 15485863ULL + 999983) & 0xFFu;
      return Colord(0.3 + (r / 255.0) * 0.7, 0.3 + (g / 255.0) * 0.7, 0.3 + (b / 255.0) * 0.7);
    }

    static Colord albedoFor(const std::shared_ptr<render::Material>& material,
                            const render::Primitive* primitive, const Vector3d& worldPos,
                            const Vector3d& normal, const Vector2d& uv, std::uint64_t faceIdx) {
      if (auto matte = std::dynamic_pointer_cast<render::MatteMaterial>(material)) {
        auto texture = matte->diffuseTexture();
        if (texture) {
          const HitPoint hp(primitive, 0.0, Vector4d(worldPos), normal, uv);
          const Rayd ray(worldPos, -normal);
          return texture->evaluate(ray, hp);
        }
      }
      return faceColor(faceIdx);
    }

    const render::Scene* m_scene;
  };

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

  struct RasterStencilPolicy {
    Buffer<std::uint8_t>& stencilBuffer;
    StencilState state;

    inline bool pass(int x, int y) const {
      return state.pass(stencilBuffer[y][x]);
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
      stencilBuffer[y][x] = state.update(op, stencilBuffer[y][x]);
    }
  };

  struct DepthWritePolicy {
    Buffer<double>& zBuffer;
    DepthState state;

    inline bool pass(int x, int y, double depth) const {
      return state.pass(depth, zBuffer[y][x]);
    }

    inline void write(int x, int y, double depth) const {
      zBuffer[y][x] = depth;
    }
  };

  struct DepthReadOnlyPolicy {
    Buffer<double>& zBuffer;
    DepthState state;

    inline bool pass(int x, int y, double depth) const {
      return state.pass(depth, zBuffer[y][x]);
    }

    inline void write(int, int, double) const {
    }
  };

  struct BuiltInFragmentPolicy {
    MaterialEvaluator materialEvaluator;

    inline Colord shade(const RasterTriangle& triangle, int, int, double, double, double,
                        const InterpolatedFragment& fragment) const {
      return materialEvaluator.shade(triangle, fragment);
    }
  };

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

  template<class Stencil, class Depth, class Fragment>
  inline void rasterizePreparedTriangleWithPolicies(const RasterTriangle& triangle,
                                                    const Recti& clipRect, Buffer<Colord>& buffer,
                                                    const Vector2d& sampleOffset,
                                                    Stencil stencil, Depth depth,
                                                    Fragment fragmentPolicy) {
    const RasterVertex& v0 = triangle.vertices[0];
    const RasterVertex& v1 = triangle.vertices[1];
    const RasterVertex& v2 = triangle.vertices[2];

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
        buffer[y][x] = shaded;
      });
  }

  template<class Stencil, class Depth, class Fragment>
  inline void rasterizeTileWithPolicies(const RasterTriangleSet& triangleSet, const Recti& rect,
                                        std::size_t tileIndex, Buffer<Colord>& buffer,
                                        const Vector2d& sampleOffset,
                                        const std::atomic<bool>& cancelled, Stencil stencil,
                                        Depth depth, Fragment fragmentPolicy) {
    const auto& triangles = triangleSet.triangles();
    const auto& triangleIndices = triangleSet.tileGrid().triangleIndices(tileIndex);
    for (const std::size_t triangleIndex : triangleIndices) {
      if (cancelled.load())
        return;
      rasterizePreparedTriangleWithPolicies(triangles[triangleIndex], rect, buffer, sampleOffset,
                                            stencil, depth, fragmentPolicy);
    }
  }

  template<class Stencil, class Depth, class Fragment>
  inline void rasterizeTriangleSetWithPolicies(
    const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan, QThreadPool& threadPool,
    std::list<std::shared_ptr<engine::TileRenderTask>>& tasks,
    const std::atomic<bool>& cancelled,
    Buffer<Colord>& buffer, const Vector2d& sampleOffset, Stencil stencil, Depth depth,
    Fragment fragmentPolicy) {
    if (tilePlan.isSingleTile()) {
      rasterizeTileWithPolicies(triangleSet, tilePlan.fullRect(), 0, buffer, sampleOffset,
                                cancelled, stencil, depth, fragmentPolicy);
      return;
    }

    engine::dispatchTileTasks(tilePlan, threadPool, tasks,
                              [&, sampleOffset, stencil, depth, fragmentPolicy](
                                const Recti& rect, std::size_t tileIndex) {
                                rasterizeTileWithPolicies(triangleSet, rect, tileIndex, buffer,
                                                          sampleOffset, cancelled, stencil, depth,
                                                          fragmentPolicy);
                              });
  }

  template<class Stencil, class Fragment, class RenderFn>
  inline void withPreparedTriangleDepthPolicy(const Rasterizer& rasterizer, Buffer<double>& zBuffer,
                                              Stencil stencil, Fragment fragmentPolicy,
                                              RenderFn&& render) {
    const DepthState depthState{rasterizer.depthFunc()};
    if (rasterizer.depthWriteEnabled()) {
      render(stencil, DepthWritePolicy{zBuffer, depthState}, fragmentPolicy);
    } else {
      render(stencil, DepthReadOnlyPolicy{zBuffer, depthState}, fragmentPolicy);
    }
  }

  template<class RenderFn>
  inline void withPreparedTrianglePolicies(const render::Scene* scene, const Rasterizer& rasterizer,
                                           Buffer<double>& zBuffer,
                                           Buffer<std::uint8_t>* stencilBuffer, RenderFn&& render) {
    const bool useStencil = rasterizer.stencilTestEnabled();
    const bool useFragmentShader = static_cast<bool>(rasterizer.fragmentShader());

    if (useStencil) {
      const StencilState stencilState{rasterizer.stencilFunc(),   rasterizer.stencilReference(),
                                      rasterizer.stencilMask(),   rasterizer.stencilWriteMask(),
                                      rasterizer.stencilFailOp(), rasterizer.stencilDepthFailOp(),
                                      rasterizer.stencilPassOp()};
      RasterStencilPolicy stencil{*stencilBuffer, stencilState};
      if (useFragmentShader) {
        withPreparedTriangleDepthPolicy(rasterizer, zBuffer, stencil,
                                        ShaderFragmentPolicy{rasterizer}, render);
      } else {
        withPreparedTriangleDepthPolicy(rasterizer, zBuffer, stencil,
                                        BuiltInFragmentPolicy{MaterialEvaluator(scene)}, render);
      }
    } else if (useFragmentShader) {
      withPreparedTriangleDepthPolicy(rasterizer, zBuffer, NoStencilPolicy{},
                                      ShaderFragmentPolicy{rasterizer}, render);
    } else {
      withPreparedTriangleDepthPolicy(rasterizer, zBuffer, NoStencilPolicy{},
                                      BuiltInFragmentPolicy{MaterialEvaluator(scene)}, render);
    }
  }

  class RasterTriangleEmitter {
  public:
    RasterTriangleEmitter(const render::Scene* scene, const std::shared_ptr<render::Camera>& camera,
                          int lod, const Rasterizer& rasterizer, const std::atomic<bool>& cancelled)
        : m_scene(scene),
          m_camera(camera),
          m_lod(lod),
          m_rasterizer(rasterizer),
          m_cullPolicy{rasterizer.cullMode()},
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

          std::vector<ProjectedVertex> projected(vertices.size());
          for (std::size_t vi = 0; vi < vertices.size(); ++vi) {
            const auto& vertex = vertices[vi];
            const Vector4d clip = m_camera->projectPointToClipSpace(vertex.point);
            const std::uint8_t outCode = clipVolume().outCode(clip);
            projected[vi] = {
              clip, outCode == 0 ? viewPlane.screenFromClipUnchecked(clip) : Vector3d::undefined(),
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
              if ((p0.outCode & p1.outCode & p2.outCode) != 0) {
                continue;
              }

              const std::uint8_t outCodeOr = p0.outCode | p1.outCode | p2.outCode;
              if (outCodeOr == 0) {
                ClipVert v0{vertices[face[0]].point, vertices[face[0]].normal, vertices[face[0]].uv,
                            Vector4d::undefined(), p0.screen};
                ClipVert v1{vertices[face[i]].point, vertices[face[i]].normal, vertices[face[i]].uv,
                            Vector4d::undefined(), p1.screen};
                ClipVert v2{vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                            vertices[face[i + 1]].uv, Vector4d::undefined(), p2.screen};

                emitPreparedTriangle(primitive, material, globalFaceIdx, v0, v1, v2, callback);
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

                emitPreparedTriangle(primitive, material, globalFaceIdx, v0, v1, v2, callback);
              }
            }
          }
        });
    }

  private:
    template<class EmitFn>
    void emitPreparedTriangle(const render::Primitive* primitive,
                              const std::shared_ptr<render::Material>& material,
                              std::uint64_t faceIdx, const ClipVert& v0, const ClipVert& v1,
                              const ClipVert& v2, EmitFn& callback) const {
      if (m_cullPolicy.shouldCull(v0, v1, v2)) {
        return;
      }

      RasterTriangle triangle;
      if (makeTriangle(v0, v1, v2, primitive, material, faceIdx, triangle)) {
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

      if (const auto& shader = m_rasterizer.vertexShader()) {
        Rasterizer::VertexInput input{vertex.point,  vertex.normal, vertex.uv, vertex.clip,
                                      vertex.screen, primitive,     material,  faceIdx};
        const Rasterizer::VertexOutput output = shader(input);
        point = output.worldPosition;
        normal = output.normal;
        uv = output.uv;
        screen = output.screenPosition;
      }

      if (screen.isUndefined() || screen.z() <= 0.0)
        return false;

      out = {point,
             normal,
             uv,
             1.0 / screen.z(),
             static_cast<int>(std::lround(screen.x())),
             static_cast<int>(std::lround(screen.y()))};
      return true;
    }

    bool makeTriangle(const ClipVert& v0, const ClipVert& v1, const ClipVert& v2,
                      const render::Primitive* primitive,
                      const std::shared_ptr<render::Material>& material, std::uint64_t faceIdx,
                      RasterTriangle& out) const {
      RasterVertex r0, r1, r2;
      const render::Material* materialPtr = material.get();
      if (!makeVertex(v0, primitive, materialPtr, faceIdx, r0) ||
          !makeVertex(v1, primitive, materialPtr, faceIdx, r1) ||
          !makeVertex(v2, primitive, materialPtr, faceIdx, r2)) {
        return false;
      }

      out = RasterTriangle{{{r0, r1, r2}}, primitive, material, faceIdx};
      return true;
    }

    const render::Scene* m_scene;
    const std::shared_ptr<render::Camera>& m_camera;
    int m_lod;
    const Rasterizer& m_rasterizer;
    TriangleCullPolicy m_cullPolicy;
    const std::atomic<bool>& m_cancelled;
  };

}

RasterTriangleSet Rasterizer::Private::collectRasterTriangles(
  const RasterTriangleEmitter& triangleEmitter, const render::TilePlan& tilePlan) {
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

void Rasterizer::Private::renderTriangleSetPass(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const RasterTriangleSet& triangleSet, const render::TilePlan& tilePlan,
  const std::atomic<bool>& cancelled, Buffer<Colord>& buffer, const Vector2d& sampleOffset) {
  PassBuffers passBuffers(rasterizer, tilePlan, buffer);
  withPreparedTrianglePolicies(scene.get(), rasterizer, passBuffers.depth(), passBuffers.stencil(),
                               [&](auto stencil, auto depth, auto fragmentPolicy) {
                                 rasterizeTriangleSetWithPolicies(
                                   triangleSet, tilePlan, *threadPool, tasks, cancelled,
                                   passBuffers.color(), sampleOffset, stencil, depth,
                                   fragmentPolicy);
                               });
}

void Rasterizer::Private::renderSingleSampleFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& tilePlan, const RasterTriangleEmitter& triangleEmitter,
  const std::atomic<bool>& cancelled, Buffer<Colord>& buffer) {
  const RasterTriangleSet triangleSet = collectRasterTriangles(triangleEmitter, tilePlan);
  if (cancelled.load() || triangleSet.empty())
    return;

  renderTriangleSetPass(rasterizer, scene, triangleSet, tilePlan, cancelled, buffer,
                        Vector2d(0.0, 0.0));
}

void Rasterizer::Private::renderMSAAFrame(
  const Rasterizer& rasterizer, const std::shared_ptr<render::Scene>& scene,
  const render::TilePlan& tilePlan, const SamplePattern& pattern,
  const RasterTriangleEmitter& triangleEmitter, const std::atomic<bool>& cancelled,
  Buffer<Colord>& buffer) {
  const RasterTriangleSet triangleSet = collectRasterTriangles(triangleEmitter, tilePlan);
  if (cancelled.load() || triangleSet.empty())
    return;

  buffer.clear(Colord::black());
  for (int sampleIndex = 0; sampleIndex != pattern.count; ++sampleIndex) {
    if (cancelled.load())
      return;

    Buffer<Colord> sampleBuffer(tilePlan.width(), tilePlan.height());
    sampleBuffer.clear(rasterizer.backgroundColor());

    const Vector2d& sampleOffset = pattern.offsets[sampleIndex];
    renderTriangleSetPass(rasterizer, scene, triangleSet, tilePlan, cancelled, sampleBuffer,
                          sampleOffset);

    if (cancelled.load())
      return;
    accumulateSample(buffer, sampleBuffer);
  }

  resolveMSAA(buffer, pattern.count);
}

void Rasterizer::Private::renderFrame(const Rasterizer& rasterizer,
                                      const std::shared_ptr<render::Scene>& scene,
                                      const std::shared_ptr<render::Camera>& camera,
                                      const std::atomic<bool>& cancelled,
                                      Buffer<Colord>& buffer) {
  const int width = buffer.width();
  const int height = buffer.height();

  if (width <= 0 || height <= 0 || cancelled.load())
    return;

  const render::TilePlan tilePlan = render::TilePlan::forBuffer(width, height, queueSize);
  const SamplePattern pattern(rasterizer.msaaSamples());
  const RasterTriangleEmitter triangleEmitter(scene.get(), camera, rasterizer.lod(), rasterizer,
                                              cancelled);
  if (pattern.count > 1) {
    renderMSAAFrame(rasterizer, scene, tilePlan, pattern, triangleEmitter, cancelled, buffer);
    return;
  }

  renderSingleSampleFrame(rasterizer, scene, tilePlan, triangleEmitter, cancelled, buffer);
}
