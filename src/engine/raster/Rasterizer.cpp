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

  // Near-plane depth used by the rasterizer's eye-space clipper. It
  // sits just in front of the eye to keep perspective divides bounded
  // for clipped vertices.
  constexpr double kNearClipDepth = 0.1;

  struct ProjectedVertex {
    double depth;
    Vector3d screen;
  };

  struct ClipVert {
    Vector3d point;
    Vector3d normal;
    double depth;
    Vector3d screen;
  };

  struct RasterVertex {
    Vector3d point;
    Vector3d normal;
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

  inline RasterVertex rasterVertex(const ClipVert& vertex) {
    const double z = vertex.screen.z();
    return {
      vertex.point,
      vertex.normal,
      1.0 / z,
      static_cast<int>(std::lround(vertex.screen.x())),
      static_cast<int>(std::lround(vertex.screen.y()))
    };
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
                        std::uint64_t faceIdx) {
    if (auto matte = std::dynamic_pointer_cast<render::MatteMaterial>(material)) {
      auto texture = matte->diffuseTexture();
      if (texture) {
        // Synthesize a HitPoint at the interpolated surface position
        // and normal so position-dependent textures (CheckerBoard)
        // sample at the right place. The ray and distance fields
        // aren't used by the texture eval path; pass placeholders.
        const HitPoint hp(primitive, 0.0, Vector4d(worldPos), normal);
        const Rayd ray(worldPos, -normal);
        return texture->evaluate(ray, hp);
      }
    }
    // Fall back to a per-face hash colour when no material is set
    // — a primitive with `material() == nullptr` would otherwise
    // render as an indistinct black silhouette.
    return faceColor(faceIdx);
  }

  inline void rasterizePreparedTriangle(const RasterTriangle& triangle,
                                        const Recti& clipRect,
                                        const render::Scene* scene,
                                        Buffer<Colord>& buffer,
                                        Buffer<double>& zBuffer) {
    const RasterVertex& v0 = triangle.vertices[0];
    const RasterVertex& v1 = triangle.vertices[1];
    const RasterVertex& v2 = triangle.vertices[2];

    core::rasterizeTriangle(v0.x, v0.y, v1.x, v1.y, v2.x, v2.y,
      clipRect.left(), clipRect.top(), clipRect.right(), clipRect.bottom(),
      [&](int x, int y, double w0b, double w1b, double w2b) {
      // Perspective-correct depth interpolation. The screen-space
      // barycentric weights from `rasterizeTriangle` are linear in
      // screen space — but vertex *depth* is not. The standard trick:
      // 1/z IS linear in screen space, so interpolate 1/z and invert.
      // (Heckbert & Moreton 1991.)
      const double oneOverZ = w0b * v0.invZ + w1b * v1.invZ + w2b * v2.invZ;
      const double pixelDepth = 1.0 / oneOverZ;
      if (pixelDepth >= zBuffer[y][x]) return;

      // Perspective-correct attribute interpolation: same trick as
      // depth, applied to vertex normals and world positions:
      //   attr_pixel = (Σ_i w_i · attr_i / z_i) · pixelDepth
      const double wp0 = w0b * v0.invZ;
      const double wp1 = w1b * v1.invZ;
      const double wp2 = w2b * v2.invZ;
      const Vector3d normal = (
        v0.normal * wp0 + v1.normal * wp1 + v2.normal * wp2
      ) * pixelDepth;
      const Vector3d worldPos = (
        v0.point  * wp0 + v1.point  * wp1 + v2.point  * wp2
      ) * pixelDepth;
      const Vector3d n = normal.normalized();

      const Colord albedo = materialAlbedo(
        triangle.material, triangle.primitive, worldPos, n, triangle.faceIdx);

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

      zBuffer[y][x] = pixelDepth;
      buffer[y][x] = shaded;
    });
  }

  template<class EmitFn>
  void emitRasterTriangles(const render::Scene* scene,
                           const std::shared_ptr<render::Camera>& camera,
                           int lod,
                           Rasterizer::CullMode cullMode,
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

        std::vector<ProjectedVertex> projected(vertices.size());
        for (std::size_t vi = 0; vi < vertices.size(); ++vi) {
          const auto& vertex = vertices[vi];
          const double depth = camera->eyeRelativeDepth(vertex.point);
          projected[vi] = {
            depth,
            depth >= kNearClipDepth
              ? camera->projectPointWithDepth(vertex.point)
              : Vector3d::undefined()
          };
        }

        for (std::size_t fi = 0; fi < faces.size(); ++fi, ++globalFaceIdx) {
          if (cancelled.load()) return;

          const auto& face = faces[fi];
          if (face.size() < 3) continue;

          for (std::size_t i = 1; i + 1 < face.size(); ++i) {
            // Sutherland-Hodgman near-plane clip — the input
            // triangle's three vertices, classified by signed
            // eye-relative depth. Vertices on or behind the near
            // plane are unprojectable; clipping replaces each
            // crossing edge with its intersection with the plane,
            // producing an output polygon entirely in front of the
            // eye. The classic GPU pipeline does the same clip in
            // clip space; doing it in eye-space here is the simpler
            // textbook formulation.
            const std::array<ClipVert, 3> input = {{
              { vertices[face[0]].point, vertices[face[0]].normal,
                projected[face[0]].depth, projected[face[0]].screen },
              { vertices[face[i]].point, vertices[face[i]].normal,
                projected[face[i]].depth, projected[face[i]].screen },
              { vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                projected[face[i + 1]].depth, projected[face[i + 1]].screen },
            }};

            std::array<ClipVert, 4> clipped;
            std::size_t clippedCount = 0;
            for (std::size_t k = 0; k < 3; ++k) {
              const ClipVert& curr = input[k];
              const ClipVert& prev = input[(k + 2) % 3];
              const bool currIn = curr.depth >= kNearClipDepth;
              const bool prevIn = prev.depth >= kNearClipDepth;
              if (currIn != prevIn) {
                const double t = (kNearClipDepth - prev.depth) / (curr.depth - prev.depth);
                ClipVert mid;
                mid.point = prev.point + (curr.point - prev.point) * t;
                mid.normal = prev.normal + (curr.normal - prev.normal) * t;
                mid.depth = kNearClipDepth;
                mid.screen = camera->projectPointWithDepth(mid.point);
                clipped[clippedCount++] = mid;
              }
              if (currIn) clipped[clippedCount++] = curr;
            }
            if (clippedCount < 3) continue;

            for (std::size_t t = 1; t + 1 < clippedCount; ++t) {
              const ClipVert& v0 = clipped[0];
              const ClipVert& v1 = clipped[t];
              const ClipVert& v2 = clipped[t + 1];

              if (v0.screen.isUndefined() || v1.screen.isUndefined() || v2.screen.isUndefined()) {
                continue;
              }
              if (shouldCullTriangle(cullMode, v0, v1, v2)) {
                continue;
              }

              callback(RasterTriangle{
                {{ rasterVertex(v0), rasterVertex(v1), rasterVertex(v2) }},
                primitive,
                material,
                globalFaceIdx
              });
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

  // Z-buffer: per-pixel eye-relative depth, initialised to +infinity
  // so the first triangle to write any pixel always wins. Smaller
  // depth = closer to the eye; the test "new < old" replaces the cell.
  Buffer<double> zBuffer(width, height);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      zBuffer[y][x] = std::numeric_limits<double>::infinity();

  auto scene = m_scene;

  if (tileCount == 1) {
    std::uint64_t globalFaceIdx = 0;
    walkLeaves(m_scene.get(), nullptr,
      [&](const render::Primitive* primitive, std::shared_ptr<render::Material> material) {
        if (m_cancelled.load()) return;

        auto mesh = primitive->tessellate(m_lod);
        if (!mesh) return;

        const auto& vertices = mesh->vertices();
        const auto& faces = mesh->faces();

        std::vector<ProjectedVertex> projected(vertices.size());
        for (std::size_t vi = 0; vi < vertices.size(); ++vi) {
          const auto& vertex = vertices[vi];
          const double depth = m_camera->eyeRelativeDepth(vertex.point);
          projected[vi] = {
            depth,
            depth >= kNearClipDepth
              ? m_camera->projectPointWithDepth(vertex.point)
              : Vector3d::undefined()
          };
        }

        for (std::size_t fi = 0; fi < faces.size(); ++fi, ++globalFaceIdx) {
          if (m_cancelled.load()) return;

          const auto& face = faces[fi];
          if (face.size() < 3) continue;

          for (std::size_t i = 1; i + 1 < face.size(); ++i) {
            const std::array<ClipVert, 3> input = {{
              { vertices[face[0]].point, vertices[face[0]].normal,
                projected[face[0]].depth, projected[face[0]].screen },
              { vertices[face[i]].point, vertices[face[i]].normal,
                projected[face[i]].depth, projected[face[i]].screen },
              { vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
                projected[face[i + 1]].depth, projected[face[i + 1]].screen },
            }};

            std::array<ClipVert, 4> clipped;
            std::size_t clippedCount = 0;
            for (std::size_t k = 0; k < 3; ++k) {
              const ClipVert& curr = input[k];
              const ClipVert& prev = input[(k + 2) % 3];
              const bool currIn = curr.depth >= kNearClipDepth;
              const bool prevIn = prev.depth >= kNearClipDepth;
              if (currIn != prevIn) {
                const double t = (kNearClipDepth - prev.depth) / (curr.depth - prev.depth);
                ClipVert mid;
                mid.point = prev.point + (curr.point - prev.point) * t;
                mid.normal = prev.normal + (curr.normal - prev.normal) * t;
                mid.depth = kNearClipDepth;
                mid.screen = m_camera->projectPointWithDepth(mid.point);
                clipped[clippedCount++] = mid;
              }
              if (currIn) clipped[clippedCount++] = curr;
            }
            if (clippedCount < 3) continue;

            for (std::size_t t = 1; t + 1 < clippedCount; ++t) {
              const ClipVert& v0 = clipped[0];
              const ClipVert& v1 = clipped[t];
              const ClipVert& v2 = clipped[t + 1];

              const Vector3d& s0 = v0.screen;
              const Vector3d& s1 = v1.screen;
              const Vector3d& s2 = v2.screen;
              if (s0.isUndefined() || s1.isUndefined() || s2.isUndefined()) continue;
              if (shouldCullTriangle(m_cullMode, v0, v1, v2)) continue;

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
                const Vector3d n = normal.normalized();

                const Colord albedo = materialAlbedo(
                  material, primitive, worldPos, n, capturedFaceIdx);

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

  std::vector<RasterTriangle> triangles;
  std::vector<std::vector<std::size_t>> tileTriangles(rows * cols);
  std::size_t binnedTriangleCount = 0;
  emitRasterTriangles(scene.get(), m_camera, m_lod, m_cullMode, m_cancelled,
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
          rasterizePreparedTriangle(triangles[triangleIndex], rect, scene.get(), buffer, zBuffer);
        }
      });

      p->tasks.push_back(task);
      p->threadPool->start(task.get());
    }
  }

  p->threadPool->waitForDone();
}
