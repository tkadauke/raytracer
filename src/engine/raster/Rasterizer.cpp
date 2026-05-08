#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"
#include "core/Exception.h"
#include "core/geometry/Mesh.h"
#include "core/geometry/Rasterize.h"
#include "core/math/IntegerDecomposition.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"
#include "core/math/Vector.h"
#include "render/cameras/Camera.h"
#include "render/lights/Light.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Scene.h"
#include "render/textures/Texture.h"
#include "render/viewplanes/ViewPlane.h"

#include <QThread>
#include <QThreadPool>
#include <QRunnable>

#include <algorithm>
#include <array>
#include <functional>
#include <list>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

using namespace engine::raster;

namespace {
  class RasterTileTask : public QRunnable {
  public:
    RasterTileTask(const Recti& rect, std::function<void()> work)
      : active(false),
        rect(rect),
        m_work(std::move(work))
    {
      setAutoDelete(false);
    }

    void run() override {
      try {
        active = true;
        m_work();
      } catch (Exception& e) {
        e.printBacktrace();
      }
      active = false;
    }

    std::atomic<bool> active;
    Recti rect;

  private:
    std::function<void()> m_work;
  };
}

struct Rasterizer::Private {
  Private()
    : threadPool(std::make_unique<QThreadPool>()),
      queueSize(1)
  {
    threadPool->setMaxThreadCount(std::max(1, QThread::idealThreadCount()));
  }

  std::unique_ptr<QThreadPool> threadPool;
  std::list<std::shared_ptr<RasterTileTask>> tasks;
  int queueSize;
};

Rasterizer::Rasterizer(std::shared_ptr<render::Scene> scene)
  : RenderEngine(std::move(scene)),
    p(std::make_unique<Private>())
{
}

Rasterizer::Rasterizer(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene)
  : RenderEngine(std::move(camera), std::move(scene)),
    p(std::make_unique<Private>())
{
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

  inline Recti tileRect(int width, int height, int rows, int cols, int rowIdx, int colIdx) {
    const int left = static_cast<int>(std::floor(double(width) * colIdx / cols));
    const int right = static_cast<int>(std::floor(double(width) * (colIdx + 1) / cols));
    const int top = static_cast<int>(std::floor(double(height) * rowIdx / rows));
    const int bottom = static_cast<int>(std::floor(double(height) * (rowIdx + 1) / rows));
    return Recti(left, top, right - left, bottom - top);
  }

  enum class ClipPlane {
    Near,
    Left,
    Right,
    Top,
    Bottom
  };

  constexpr std::array<ClipPlane, 5> kClipPlanes = {{
    ClipPlane::Near,
    ClipPlane::Left,
    ClipPlane::Right,
    ClipPlane::Top,
    ClipPlane::Bottom
  }};

  using ClipPolygon = std::array<ClipVert, kMaxClipVertices>;

  inline std::uint8_t clipBit(ClipPlane plane) {
    return static_cast<std::uint8_t>(1u << static_cast<int>(plane));
  }

  inline std::uint8_t clipOutCode(const Vector4d& clip) {
    if (clip.isUndefined()) {
      std::uint8_t all = 0;
      for (ClipPlane plane : kClipPlanes) {
        all |= clipBit(plane);
      }
      return all;
    }

    std::uint8_t outCode = 0;
    if (clip.z() < kNearClipDepth) outCode |= clipBit(ClipPlane::Near);
    if (clip.x() < -clip.w()) outCode |= clipBit(ClipPlane::Left);
    if (clip.x() >  clip.w()) outCode |= clipBit(ClipPlane::Right);
    if (clip.y() < -clip.w()) outCode |= clipBit(ClipPlane::Top);
    if (clip.y() >  clip.w()) outCode |= clipBit(ClipPlane::Bottom);
    return outCode;
  }

  inline double clipDistance(const ClipVert& vertex, ClipPlane plane) {
    switch (plane) {
    case ClipPlane::Near:
      return vertex.clip.z() - kNearClipDepth;
    case ClipPlane::Left:
      return vertex.clip.x() + vertex.clip.w();
    case ClipPlane::Right:
      return vertex.clip.w() - vertex.clip.x();
    case ClipPlane::Top:
      return vertex.clip.y() + vertex.clip.w();
    case ClipPlane::Bottom:
      return vertex.clip.w() - vertex.clip.y();
    }
    return -1.0;
  }

  inline ClipVert interpolateClipVert(const ClipVert& from,
                                      const ClipVert& to,
                                      double t) {
    return {
      from.point + (to.point - from.point) * t,
      from.normal + (to.normal - from.normal) * t,
      from.uv + (to.uv - from.uv) * t,
      from.clip + (to.clip - from.clip) * t,
      Vector3d::undefined()
    };
  }

  inline std::size_t clipPolygonAgainstPlane(const ClipPolygon& input,
                                             std::size_t inputCount,
                                             ClipPlane plane,
                                             ClipPolygon& output) {
    if (inputCount == 0) return 0;

    std::size_t outputCount = 0;
    ClipVert prev = input[inputCount - 1];
    double prevDistance = clipDistance(prev, plane);
    bool prevInside = prevDistance >= 0.0;

    for (std::size_t i = 0; i < inputCount; ++i) {
      const ClipVert& curr = input[i];
      const double currDistance = clipDistance(curr, plane);
      const bool currInside = currDistance >= 0.0;

      if (currInside != prevInside) {
        const double t = prevDistance / (prevDistance - currDistance);
        output[outputCount++] = interpolateClipVert(prev, curr, t);
      }
      if (currInside) {
        output[outputCount++] = curr;
      }

      prev = curr;
      prevDistance = currDistance;
      prevInside = currInside;
    }

    return outputCount;
  }

  inline std::size_t clipTriangleToView(const std::array<ClipVert, 3>& input,
                                        ClipPolygon& clipped) {
    bool allInside = true;
    for (ClipPlane plane : kClipPlanes) {
      std::size_t insideCount = 0;
      for (const ClipVert& vertex : input) {
        if (clipDistance(vertex, plane) >= 0.0) {
          ++insideCount;
        }
      }
      if (insideCount == 0) return 0;
      if (insideCount != input.size()) {
        allInside = false;
      }
    }

    if (allInside) {
      for (std::size_t i = 0; i < input.size(); ++i) {
        clipped[i] = input[i];
      }
      return input.size();
    }

    ClipPolygon polygonA;
    ClipPolygon polygonB;
    for (std::size_t i = 0; i < input.size(); ++i) {
      polygonA[i] = input[i];
    }

    std::size_t count = input.size();
    ClipPolygon* in = &polygonA;
    ClipPolygon* out = &polygonB;

    for (ClipPlane plane : kClipPlanes) {
      count = clipPolygonAgainstPlane(*in, count, plane, *out);
      if (count < 3) return 0;
      std::swap(in, out);
    }

    for (std::size_t i = 0; i < count; ++i) {
      clipped[i] = (*in)[i];
    }
    return count;
  }

  inline Vector3d screenFromClipUnchecked(const Vector4d& clip, const render::ViewPlane& viewPlane) {
    const double invW = 1.0 / clip.w();
    const double ndcX = clip.x() * invW;
    const double ndcY = clip.y() * invW;
    return Vector3d(
      (ndcX + 1.0) * viewPlane.width() / 2.0,
      (ndcY + 1.0) * viewPlane.height() / 2.0,
      clip.z()
    );
  }

  inline Vector3d screenFromClip(const Vector4d& clip, const render::ViewPlane& viewPlane) {
    if (clip.isUndefined() || clip.w() <= 0.0) return Vector3d::undefined();
    return screenFromClipUnchecked(clip, viewPlane);
  }

  inline bool ensureScreen(ClipVert& vertex, const render::ViewPlane& viewPlane) {
    if (vertex.screen.isUndefined()) {
      vertex.screen = screenFromClip(vertex.clip, viewPlane);
    }
    return vertex.screen.isDefined();
  }

  inline double signedScreenArea(const ClipVert& v0, const ClipVert& v1, const ClipVert& v2) {
    return (v1.screen.x() - v0.screen.x()) * (v2.screen.y() - v0.screen.y())
         - (v1.screen.y() - v0.screen.y()) * (v2.screen.x() - v0.screen.x());
  }

  inline bool shouldCullTriangle(Rasterizer::CullMode mode,
                                 const ClipVert& v0,
                                 const ClipVert& v1,
                                 const ClipVert& v2) {
    if (mode == Rasterizer::CullMode::Both) return false;

    // Tessellated primitives use CCW winding when viewed from the
    // outside. With the current camera projection, front-facing
    // triangles have negative projected area and back-facing
    // triangles have positive projected area.
    const double area = signedScreenArea(v0, v1, v2);
    if (area == 0.0) return false;

    return mode == Rasterizer::CullMode::Back
      ? area > 0.0
      : area < 0.0;
  }

  inline bool makeRasterVertex(const ClipVert& vertex,
                               const Rasterizer& rasterizer,
                               const render::Primitive* primitive,
                               const render::Material* material,
                               std::uint64_t faceIdx,
                               RasterVertex& out) {
    Vector3d point = vertex.point;
    Vector3d normal = vertex.normal;
    Vector2d uv = vertex.uv;
    Vector3d screen = vertex.screen;

    if (const auto& shader = rasterizer.vertexShader()) {
      Rasterizer::VertexInput input{
        vertex.point,
        vertex.normal,
        vertex.uv,
        vertex.clip,
        vertex.screen,
        primitive,
        material,
        faceIdx
      };
      const Rasterizer::VertexOutput output = shader(input);
      point = output.worldPosition;
      normal = output.normal;
      uv = output.uv;
      screen = output.screenPosition;
    }

    if (screen.isUndefined() || screen.z() <= 0.0) return false;

    out = {
      point,
      normal,
      uv,
      1.0 / screen.z(),
      static_cast<int>(std::lround(screen.x())),
      static_cast<int>(std::lround(screen.y()))
    };
    return true;
  }

  inline bool makeRasterTriangle(const ClipVert& v0,
                                 const ClipVert& v1,
                                 const ClipVert& v2,
                                 const Rasterizer& rasterizer,
                                 const render::Primitive* primitive,
                                 const std::shared_ptr<render::Material>& material,
                                 std::uint64_t faceIdx,
                                 RasterTriangle& out) {
    RasterVertex r0, r1, r2;
    const render::Material* materialPtr = material.get();
    if (!makeRasterVertex(v0, rasterizer, primitive, materialPtr, faceIdx, r0)
        || !makeRasterVertex(v1, rasterizer, primitive, materialPtr, faceIdx, r1)
        || !makeRasterVertex(v2, rasterizer, primitive, materialPtr, faceIdx, r2)) {
      return false;
    }

    out = RasterTriangle{
      {{ r0, r1, r2 }},
      primitive,
      material,
      faceIdx
    };
    return true;
  }

  inline std::size_t addTriangleToTiles(
    const RasterTriangle& triangle,
    std::size_t triangleIndex,
    int width,
    int height,
    int rows,
    int cols,
    std::vector<std::vector<std::size_t>>& tileTriangles) {
    const int rawMinX = std::min({ triangle.vertices[0].x, triangle.vertices[1].x, triangle.vertices[2].x });
    const int rawMaxX = std::max({ triangle.vertices[0].x, triangle.vertices[1].x, triangle.vertices[2].x });
    const int rawMinY = std::min({ triangle.vertices[0].y, triangle.vertices[1].y, triangle.vertices[2].y });
    const int rawMaxY = std::max({ triangle.vertices[0].y, triangle.vertices[1].y, triangle.vertices[2].y });

    if (rawMaxX < 0 || rawMaxY < 0 || rawMinX >= width || rawMinY >= height) {
      return 0;
    }

    const int minX = std::clamp(rawMinX, 0, width - 1);
    const int maxX = std::clamp(rawMaxX, 0, width - 1);
    const int minY = std::clamp(rawMinY, 0, height - 1);
    const int maxY = std::clamp(rawMaxY, 0, height - 1);

    const int firstCol = std::clamp(minX * cols / width, 0, cols - 1);
    const int lastCol = std::clamp(maxX * cols / width, 0, cols - 1);
    const int firstRow = std::clamp(minY * rows / height, 0, rows - 1);
    const int lastRow = std::clamp(maxY * rows / height, 0, rows - 1);

    std::size_t added = 0;
    for (int row = firstRow; row <= lastRow; ++row) {
      for (int col = firstCol; col <= lastCol; ++col) {
        tileTriangles[row * cols + col].push_back(triangleIndex);
        ++added;
      }
    }
    return added;
  }

  // A reasonably colour-spread hash from a uint64 face index → RGB
  // in [0, 1]³. Fallback when a primitive has no material from which
  // an albedo can be recovered.
  Colord faceColor(std::uint64_t index) {
    const std::uint64_t r = (index * 2654435761ULL)        & 0xFFu;
    const std::uint64_t g = (index * 40503ULL + 12345)     & 0xFFu;
    const std::uint64_t b = (index * 15485863ULL + 999983) & 0xFFu;
    return Colord(0.3 + (r / 255.0) * 0.7,
                  0.3 + (g / 255.0) * 0.7,
                  0.3 + (b / 255.0) * 0.7);
  }

  // Recursive scene walker — visits every leaf primitive and emits
  // (primitive, effective material) pairs to the callback. Composite
  // children inherit their parent's material when they don't have
  // one of their own (matching `Composite::intersect`'s material
  // fallback semantics).
  template<class Fn>
  void walkLeaves(const render::Primitive* prim,
                  std::shared_ptr<render::Material> inherited,
                  Fn&& callback) {
    if (!prim) return;

    auto own = prim->material();
    auto effective = own ? own : inherited;

    if (auto composite = dynamic_cast<const render::Composite*>(prim)) {
      for (const auto& child : composite->primitives()) {
        walkLeaves(child.get(), effective, callback);
      }
    } else {
      callback(prim, effective);
    }
  }

  // Recover a per-pixel albedo (diffuse colour) from the primitive's
  // material at the given hit context, or fall back to the per-face
  // hash when the material has no diffuse texture (PortalMaterial,
  // null material, …).
  Colord materialAlbedo(const std::shared_ptr<render::Material>& material,
                        const render::Primitive* primitive,
                        const Vector3d& worldPos,
                        const Vector3d& normal,
                        const Vector2d& uv,
                        std::uint64_t faceIdx) {
    if (auto matte = std::dynamic_pointer_cast<render::MatteMaterial>(material)) {
      auto texture = matte->diffuseTexture();
      if (texture) {
        // Synthesize a HitPoint at the interpolated surface position
        // normal, and UVs so position- and UV-dependent textures
        // sample at the right place. The ray and distance fields
        // aren't used by the texture eval path; pass placeholders.
        const HitPoint hp(primitive, 0.0, Vector4d(worldPos), normal, uv);
        const Rayd ray(worldPos, -normal);
        return texture->evaluate(ray, hp);
      }
    }
    // Fall back to a per-face hash colour when no material is set
    // — a primitive with `material() == nullptr` would otherwise
    // render as an indistinct black silhouette.
    return faceColor(faceIdx);
  }

  struct InterpolatedFragment {
    double depth;
    Vector3d worldPos;
    Vector3d normal;
    Vector2d uv;
  };

  inline InterpolatedFragment interpolateFragment(const RasterVertex& v0,
                                                 const RasterVertex& v1,
                                                 const RasterVertex& v2,
                                                 double w0b,
                                                 double w1b,
                                                 double w2b) {
    // Perspective-correct depth interpolation. The screen-space
    // barycentric weights from `rasterizeTriangle` are linear in
    // screen space — but vertex *depth* is not. The standard trick:
    // 1/z IS linear in screen space, so interpolate 1/z and invert.
    // (Heckbert & Moreton 1991.)
    const double oneOverZ = w0b * v0.invZ + w1b * v1.invZ + w2b * v2.invZ;
    const double pixelDepth = 1.0 / oneOverZ;

    // Perspective-correct attribute interpolation: same trick as
    // depth, applied to vertex normals, world positions, and UVs:
    //   attr_pixel = (Σ_i w_i · attr_i / z_i) · pixelDepth
    const double wp0 = w0b * v0.invZ;
    const double wp1 = w1b * v1.invZ;
    const double wp2 = w2b * v2.invZ;
    return {
      pixelDepth,
      (v0.point  * wp0 + v1.point  * wp1 + v2.point  * wp2) * pixelDepth,
      (v0.normal * wp0 + v1.normal * wp1 + v2.normal * wp2) * pixelDepth,
      (v0.uv     * wp0 + v1.uv     * wp1 + v2.uv     * wp2) * pixelDepth
    };
  }

  inline bool depthTestPass(Rasterizer::DepthFunc func, double incoming, double stored) {
    switch (func) {
    case Rasterizer::DepthFunc::Never:        return false;
    case Rasterizer::DepthFunc::Less:         return incoming <  stored;
    case Rasterizer::DepthFunc::Equal:        return incoming == stored;
    case Rasterizer::DepthFunc::LessEqual:    return incoming <= stored;
    case Rasterizer::DepthFunc::Greater:      return incoming >  stored;
    case Rasterizer::DepthFunc::GreaterEqual: return incoming >= stored;
    case Rasterizer::DepthFunc::NotEqual:     return incoming != stored;
    case Rasterizer::DepthFunc::Always:       return true;
    }
    return false;
  }

  inline bool stencilTestPass(Rasterizer::StencilFunc func,
                              std::uint8_t reference,
                              std::uint8_t stored,
                              std::uint8_t mask) {
    const std::uint8_t lhs = reference & mask;
    const std::uint8_t rhs = stored & mask;
    switch (func) {
    case Rasterizer::StencilFunc::Never:        return false;
    case Rasterizer::StencilFunc::Less:         return lhs <  rhs;
    case Rasterizer::StencilFunc::Equal:        return lhs == rhs;
    case Rasterizer::StencilFunc::LessEqual:    return lhs <= rhs;
    case Rasterizer::StencilFunc::Greater:      return lhs >  rhs;
    case Rasterizer::StencilFunc::GreaterEqual: return lhs >= rhs;
    case Rasterizer::StencilFunc::NotEqual:     return lhs != rhs;
    case Rasterizer::StencilFunc::Always:       return true;
    }
    return false;
  }

  inline std::uint8_t applyStencilOp(Rasterizer::StencilOp op,
                                     std::uint8_t current,
                                     std::uint8_t reference) {
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

  inline void updateStencil(Buffer<std::uint8_t>* stencilBuffer,
                            const Rasterizer& rasterizer,
                            int x,
                            int y,
                            Rasterizer::StencilOp op) {
    if (!stencilBuffer) return;

    const std::uint8_t current = (*stencilBuffer)[y][x];
    const std::uint8_t updated = applyStencilOp(op, current, rasterizer.stencilReference());
    const std::uint8_t writeMask = rasterizer.stencilWriteMask();
    (*stencilBuffer)[y][x] = static_cast<std::uint8_t>(
      (current & ~writeMask) | (updated & writeMask));
  }

  inline Colord shadeBuiltInFragment(const RasterTriangle& triangle,
                                     const render::Scene* scene,
                                     const Vector3d& worldPos,
                                     const Vector3d& normal,
                                     const Vector2d& uv) {
    const Vector3d n = normal.normalized();
    const Colord albedo = materialAlbedo(
      triangle.material, triangle.primitive, worldPos, n, uv, triangle.faceIdx);

    // Lambertian shading. No shadow rays (no recursive ray tracing
    // in this engine); each light contributes diffuse-cosine-
    // weighted radiance directly.
    Colord shaded = scene->ambient() * kAmbientCoefficient * albedo;
    for (const auto& light : scene->lights()) {
      const Vector3d lightDir = light->direction(worldPos);
      const double nDotL = std::max(0.0, n * lightDir);
      if (nDotL > 0.0) {
        shaded += albedo * light->radiance() * nDotL;
      }
    }
    return shaded;
  }

  inline bool usesBuiltInDepthStencilAndFragment(const Rasterizer& rasterizer) {
    return rasterizer.depthFunc() == Rasterizer::DepthFunc::Less
      && rasterizer.depthWriteEnabled()
      && !rasterizer.stencilTestEnabled()
      && !rasterizer.fragmentShader();
  }

  inline void rasterizePreparedTriangle(const RasterTriangle& triangle,
                                        const Recti& clipRect,
                                        const render::Scene* scene,
                                        const Rasterizer& rasterizer,
                                        Buffer<Colord>& buffer,
                                        Buffer<double>& zBuffer,
                                        Buffer<std::uint8_t>* stencilBuffer) {
    const RasterVertex& v0 = triangle.vertices[0];
    const RasterVertex& v1 = triangle.vertices[1];
    const RasterVertex& v2 = triangle.vertices[2];

    if (usesBuiltInDepthStencilAndFragment(rasterizer)) {
      core::rasterizeTriangle(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y,
        clipRect.left(), clipRect.top(), clipRect.right(), clipRect.bottom(),
        [&](int x, int y, double w0b, double w1b, double w2b) {
        const InterpolatedFragment fragment = interpolateFragment(v0, v1, v2, w0b, w1b, w2b);
        if (fragment.depth >= zBuffer[y][x]) return;

        zBuffer[y][x] = fragment.depth;
        buffer[y][x] = shadeBuiltInFragment(
          triangle, scene, fragment.worldPos, fragment.normal, fragment.uv);
      });
      return;
    }

    core::rasterizeTriangle(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y,
      clipRect.left(), clipRect.top(), clipRect.right(), clipRect.bottom(),
      [&](int x, int y, double w0b, double w1b, double w2b) {
      if (stencilBuffer) {
        const std::uint8_t stored = (*stencilBuffer)[y][x];
        if (!stencilTestPass(
              rasterizer.stencilFunc(),
              rasterizer.stencilReference(),
              stored,
              rasterizer.stencilMask())) {
          updateStencil(stencilBuffer, rasterizer, x, y, rasterizer.stencilFailOp());
          return;
        }
      }

      const InterpolatedFragment fragment = interpolateFragment(v0, v1, v2, w0b, w1b, w2b);
      if (!depthTestPass(rasterizer.depthFunc(), fragment.depth, zBuffer[y][x])) {
        updateStencil(stencilBuffer, rasterizer, x, y, rasterizer.stencilDepthFailOp());
        return;
      }

      updateStencil(stencilBuffer, rasterizer, x, y, rasterizer.stencilPassOp());

      Colord shaded;
      if (const auto& shader = rasterizer.fragmentShader()) {
        const Vector3d n = fragment.normal.normalized();
        const Rasterizer::FragmentInput input{
          x,
          y,
          fragment.depth,
          Vector3d(w0b, w1b, w2b),
          fragment.worldPos,
          n,
          fragment.uv,
          triangle.primitive,
          triangle.material.get(),
          triangle.faceIdx
        };
        shaded = shader(input);
      } else {
        shaded = shadeBuiltInFragment(
          triangle, scene, fragment.worldPos, fragment.normal, fragment.uv);
      }

      if (rasterizer.depthWriteEnabled()) {
        zBuffer[y][x] = fragment.depth;
      }
      buffer[y][x] = shaded;
    });
  }

  template<class EmitFn>
  void emitRasterTriangles(const render::Scene* scene,
                           const std::shared_ptr<render::Camera>& camera,
                           int lod,
                           const Rasterizer& rasterizer,
                           const std::atomic<bool>& cancelled,
                           EmitFn&& callback) {
    std::uint64_t globalFaceIdx = 0;
    walkLeaves(scene, nullptr,
      [&](const render::Primitive* primitive, std::shared_ptr<render::Material> material) {
        if (cancelled.load()) return;

        auto mesh = primitive->tessellate(lod);
        if (!mesh) return;

        const auto& vertices = mesh->vertices();
        const auto& faces = mesh->faces();
        const auto& viewPlane = *camera->viewPlane();

        std::vector<ProjectedVertex> projected(vertices.size());
        for (std::size_t vi = 0; vi < vertices.size(); ++vi) {
          const auto& vertex = vertices[vi];
          const Vector4d clip = camera->projectPointToClipSpace(vertex.point);
          const std::uint8_t outCode = clipOutCode(clip);
          projected[vi] = {
            clip,
            outCode == 0 ? screenFromClipUnchecked(clip, viewPlane) : Vector3d::undefined(),
            outCode
          };
        }

        for (std::size_t fi = 0; fi < faces.size(); ++fi, ++globalFaceIdx) {
          if (cancelled.load()) return;

          const auto& face = faces[fi];
          if (face.size() < 3) continue;

          for (std::size_t i = 1; i + 1 < face.size(); ++i) {
            const ProjectedVertex& p0 = projected[face[0]];
            const ProjectedVertex& p1 = projected[face[i]];
            const ProjectedVertex& p2 = projected[face[i + 1]];
            if ((p0.outCode & p1.outCode & p2.outCode) != 0) {
              continue;
            }

            const std::uint8_t outCodeOr = p0.outCode | p1.outCode | p2.outCode;
            if (outCodeOr == 0) {
              ClipVert v0{ vertices[face[0]].point, vertices[face[0]].normal,
                vertices[face[0]].uv, Vector4d::undefined(), p0.screen };
              ClipVert v1{ vertices[face[i]].point, vertices[face[i]].normal,
                vertices[face[i]].uv, Vector4d::undefined(), p1.screen };
              ClipVert v2{ vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                vertices[face[i + 1]].uv, Vector4d::undefined(), p2.screen };

              if (shouldCullTriangle(rasterizer.cullMode(), v0, v1, v2)) {
                continue;
              }

              RasterTriangle triangle;
              if (makeRasterTriangle(v0, v1, v2,
                    rasterizer, primitive, material, globalFaceIdx, triangle)) {
                callback(triangle);
              }
              continue;
            }

            // Sutherland-Hodgman homogeneous clipping. The camera
            // gives us un-divided clip coordinates, so the same
            // polygon clipper handles the near plane and the four
            // viewport edges before any perspective divide can blow
            // up screen coordinates.
            const std::array<ClipVert, 3> input = {{
              { vertices[face[0]].point, vertices[face[0]].normal,
                vertices[face[0]].uv, p0.clip, p0.screen },
              { vertices[face[i]].point, vertices[face[i]].normal,
                vertices[face[i]].uv, p1.clip, p1.screen },
              { vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                vertices[face[i + 1]].uv, p2.clip, p2.screen },
            }};

            ClipPolygon clipped;
            const std::size_t clippedCount = clipTriangleToView(input, clipped);
            if (clippedCount < 3) continue;

            for (std::size_t t = 1; t + 1 < clippedCount; ++t) {
              ClipVert v0 = clipped[0];
              ClipVert v1 = clipped[t];
              ClipVert v2 = clipped[t + 1];

              if (!ensureScreen(v0, viewPlane)
                  || !ensureScreen(v1, viewPlane)
                  || !ensureScreen(v2, viewPlane)) {
                continue;
              }
              if (shouldCullTriangle(rasterizer.cullMode(), v0, v1, v2)) {
                continue;
              }

              RasterTriangle triangle;
              if (makeRasterTriangle(v0, v1, v2,
                    rasterizer, primitive, material, globalFaceIdx, triangle)) {
                callback(triangle);
              }
            }
          }
        }
      });
  }
}

void Rasterizer::render(Buffer<Colord>& buffer) {
  // Caller is expected to call uncancel() between renders. Matches
  // the Wireframe / Raytracer convention.

  // Clear to background. Buffer<T>::clear() default-constructs every
  // cell; write the configured colour explicitly instead.
  for (int y = 0; y < buffer.height(); ++y)
    for (int x = 0; x < buffer.width(); ++x)
      buffer[y][x] = m_backgroundColor;

  if (!m_scene || !m_camera) return;

  // Same view-plane setup the other engines perform — the camera
  // projection math depends on the cached basis vectors.
  m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());

  const int width = buffer.width();
  const int height = buffer.height();
  p->tasks.clear();

  if (width <= 0 || height <= 0 || m_cancelled.load()) return;

  const int tileCount = std::max(1, std::min(p->queueSize, width * height));
  IntegerDecomposition decomposition(tileCount);
  const int rows = std::max(1, std::min(decomposition.first(), height));
  const int cols = std::max(1, std::min(decomposition.second(), width));

  // Z-buffer: per-pixel eye-relative depth. Smaller depth = closer
  // to the eye for the default `DepthFunc::Less` path.
  Buffer<double> zBuffer(width, height);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      zBuffer[y][x] = m_depthClearValue;

  std::unique_ptr<Buffer<std::uint8_t>> stencilBuffer;
  if (m_stencilTestEnabled) {
    stencilBuffer = std::make_unique<Buffer<std::uint8_t>>(width, height);
    for (int y = 0; y < height; ++y)
      for (int x = 0; x < width; ++x)
        (*stencilBuffer)[y][x] = m_stencilClearValue;
  }

  auto scene = m_scene;

  if (tileCount == 1 && usesBuiltInDepthStencilAndFragment(*this) && !m_vertexShader) {
    std::uint64_t globalFaceIdx = 0;
    walkLeaves(m_scene.get(), nullptr,
      [&](const render::Primitive* primitive, std::shared_ptr<render::Material> material) {
        if (m_cancelled.load()) return;

        auto mesh = primitive->tessellate(m_lod);
        if (!mesh) return;

        const auto& vertices = mesh->vertices();
        const auto& faces = mesh->faces();
        const auto& viewPlane = *m_camera->viewPlane();

        std::vector<ProjectedVertex> projected(vertices.size());
        for (std::size_t vi = 0; vi < vertices.size(); ++vi) {
          const auto& vertex = vertices[vi];
          const Vector4d clip = m_camera->projectPointToClipSpace(vertex.point);
          const std::uint8_t outCode = clipOutCode(clip);
          projected[vi] = {
            clip,
            outCode == 0 ? screenFromClipUnchecked(clip, viewPlane) : Vector3d::undefined(),
            outCode
          };
        }

        for (std::size_t fi = 0; fi < faces.size(); ++fi, ++globalFaceIdx) {
          if (m_cancelled.load()) return;

          const auto& face = faces[fi];
          if (face.size() < 3) continue;

          for (std::size_t i = 1; i + 1 < face.size(); ++i) {
            const ProjectedVertex& p0 = projected[face[0]];
            const ProjectedVertex& p1 = projected[face[i]];
            const ProjectedVertex& p2 = projected[face[i + 1]];
            if ((p0.outCode & p1.outCode & p2.outCode) != 0) {
              continue;
            }

            const std::uint8_t outCodeOr = p0.outCode | p1.outCode | p2.outCode;
            if (outCodeOr == 0) {
              ClipVert v0{ vertices[face[0]].point, vertices[face[0]].normal,
                vertices[face[0]].uv, Vector4d::undefined(), p0.screen };
              ClipVert v1{ vertices[face[i]].point, vertices[face[i]].normal,
                vertices[face[i]].uv, Vector4d::undefined(), p1.screen };
              ClipVert v2{ vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                vertices[face[i + 1]].uv, Vector4d::undefined(), p2.screen };

              if (shouldCullTriangle(m_cullMode, v0, v1, v2)) continue;

              const Vector3d& s0 = v0.screen;
              const Vector3d& s1 = v1.screen;
              const Vector3d& s2 = v2.screen;

              const double z0 = s0.z(), z1 = s1.z(), z2 = s2.z();
              const double invZ0 = 1.0 / z0, invZ1 = 1.0 / z1, invZ2 = 1.0 / z2;

              const int x0 = static_cast<int>(std::lround(s0.x()));
              const int y0 = static_cast<int>(std::lround(s0.y()));
              const int x1 = static_cast<int>(std::lround(s1.x()));
              const int y1 = static_cast<int>(std::lround(s1.y()));
              const int x2 = static_cast<int>(std::lround(s2.x()));
              const int y2 = static_cast<int>(std::lround(s2.y()));

              const std::uint64_t capturedFaceIdx = globalFaceIdx;
              core::rasterizeTriangle(x0, y0, x1, y1, x2, y2,
                0, 0, width, height,
                [&, capturedFaceIdx](int x, int y, double w0b, double w1b, double w2b) {
                const double oneOverZ = w0b * invZ0 + w1b * invZ1 + w2b * invZ2;
                const double pixelDepth = 1.0 / oneOverZ;
                if (pixelDepth >= zBuffer[y][x]) return;

                const double wp0 = w0b * invZ0;
                const double wp1 = w1b * invZ1;
                const double wp2 = w2b * invZ2;
                const Vector3d normal = (
                  v0.normal * wp0 + v1.normal * wp1 + v2.normal * wp2
                ) * pixelDepth;
                const Vector3d worldPos = (
                  v0.point  * wp0 + v1.point  * wp1 + v2.point  * wp2
                ) * pixelDepth;
                const Vector2d uv = (
                  v0.uv     * wp0 + v1.uv     * wp1 + v2.uv     * wp2
                ) * pixelDepth;
                const Vector3d n = normal.normalized();

                const Colord albedo = materialAlbedo(
                  material, primitive, worldPos, n, uv, capturedFaceIdx);

                Colord shaded = m_scene->ambient() * kAmbientCoefficient * albedo;
                for (const auto& light : m_scene->lights()) {
                  const Vector3d lightDir = light->direction(worldPos);
                  const double nDotL = std::max(0.0, n * lightDir);
                  if (nDotL > 0.0) {
                    shaded += albedo * light->radiance() * nDotL;
                  }
                }

                zBuffer[y][x] = pixelDepth;
                buffer[y][x] = shaded;
              });
              continue;
            }

            const std::array<ClipVert, 3> input = {{
              { vertices[face[0]].point, vertices[face[0]].normal,
                vertices[face[0]].uv, p0.clip, p0.screen },
              { vertices[face[i]].point, vertices[face[i]].normal,
                vertices[face[i]].uv, p1.clip, p1.screen },
              { vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                vertices[face[i + 1]].uv, p2.clip, p2.screen },
            }};

            ClipPolygon clipped;
            const std::size_t clippedCount = clipTriangleToView(input, clipped);
            if (clippedCount < 3) continue;

            for (std::size_t t = 1; t + 1 < clippedCount; ++t) {
              ClipVert v0 = clipped[0];
              ClipVert v1 = clipped[t];
              ClipVert v2 = clipped[t + 1];

              if (!ensureScreen(v0, viewPlane)
                  || !ensureScreen(v1, viewPlane)
                  || !ensureScreen(v2, viewPlane)) {
                continue;
              }
              if (shouldCullTriangle(m_cullMode, v0, v1, v2)) continue;

              const Vector3d& s0 = v0.screen;
              const Vector3d& s1 = v1.screen;
              const Vector3d& s2 = v2.screen;

              const double z0 = s0.z(), z1 = s1.z(), z2 = s2.z();
              const double invZ0 = 1.0 / z0, invZ1 = 1.0 / z1, invZ2 = 1.0 / z2;

              const int x0 = static_cast<int>(std::lround(s0.x()));
              const int y0 = static_cast<int>(std::lround(s0.y()));
              const int x1 = static_cast<int>(std::lround(s1.x()));
              const int y1 = static_cast<int>(std::lround(s1.y()));
              const int x2 = static_cast<int>(std::lround(s2.x()));
              const int y2 = static_cast<int>(std::lround(s2.y()));

              const std::uint64_t capturedFaceIdx = globalFaceIdx;
              core::rasterizeTriangle(x0, y0, x1, y1, x2, y2,
                0, 0, width, height,
                [&, capturedFaceIdx](int x, int y, double w0b, double w1b, double w2b) {
                const double oneOverZ = w0b * invZ0 + w1b * invZ1 + w2b * invZ2;
                const double pixelDepth = 1.0 / oneOverZ;
                if (pixelDepth >= zBuffer[y][x]) return;

                const double wp0 = w0b * invZ0;
                const double wp1 = w1b * invZ1;
                const double wp2 = w2b * invZ2;
                const Vector3d normal = (
                  v0.normal * wp0 + v1.normal * wp1 + v2.normal * wp2
                ) * pixelDepth;
                const Vector3d worldPos = (
                  v0.point  * wp0 + v1.point  * wp1 + v2.point  * wp2
                ) * pixelDepth;
                const Vector2d uv = (
                  v0.uv     * wp0 + v1.uv     * wp1 + v2.uv     * wp2
                ) * pixelDepth;
                const Vector3d n = normal.normalized();

                const Colord albedo = materialAlbedo(
                  material, primitive, worldPos, n, uv, capturedFaceIdx);

                Colord shaded = m_scene->ambient() * kAmbientCoefficient * albedo;
                for (const auto& light : m_scene->lights()) {
                  const Vector3d lightDir = light->direction(worldPos);
                  const double nDotL = std::max(0.0, n * lightDir);
                  if (nDotL > 0.0) {
                    shaded += albedo * light->radiance() * nDotL;
                  }
                }

                zBuffer[y][x] = pixelDepth;
                buffer[y][x] = shaded;
              });
            }
          }
        }
      });
    return;
  }

  if (tileCount == 1) {
    const Recti fullRect(0, 0, width, height);
    emitRasterTriangles(scene.get(), m_camera, m_lod, *this, m_cancelled,
      [&](const RasterTriangle& triangle) {
        rasterizePreparedTriangle(
          triangle, fullRect, scene.get(), *this, buffer, zBuffer, stencilBuffer.get());
      });
    return;
  }

  std::vector<RasterTriangle> triangles;
  std::vector<std::vector<std::size_t>> tileTriangles(rows * cols);
  std::size_t binnedTriangleCount = 0;
  emitRasterTriangles(scene.get(), m_camera, m_lod, *this, m_cancelled,
    [&](const RasterTriangle& triangle) {
      const std::size_t triangleIndex = triangles.size();
      const std::size_t added = addTriangleToTiles(
        triangle, triangleIndex, width, height, rows, cols, tileTriangles);
      if (added != 0) {
        triangles.push_back(triangle);
        binnedTriangleCount += added;
      }
    });

  if (m_cancelled.load() || binnedTriangleCount == 0) return;

  for (int row = 0; row != rows; ++row) {
    for (int col = 0; col != cols; ++col) {
      const Recti rect = tileRect(width, height, rows, cols, row, col);
      if (rect.width() <= 0 || rect.height() <= 0) continue;
      const std::size_t tileIndex = row * cols + col;

      auto task = std::make_shared<RasterTileTask>(rect, [&, rect, scene, tileIndex] {
        const auto& triangleIndices = tileTriangles[tileIndex];
        for (const std::size_t triangleIndex : triangleIndices) {
          if (m_cancelled.load()) return;
          rasterizePreparedTriangle(
            triangles[triangleIndex], rect, scene.get(), *this, buffer, zBuffer, stencilBuffer.get());
        }
      });

      p->tasks.push_back(task);
      p->threadPool->start(task.get());
    }
  }

  p->threadPool->waitForDone();
}
