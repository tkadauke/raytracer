#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"
#include "core/geometry/Mesh.h"
#include "core/geometry/Rasterize.h"
#include "core/math/Vector.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"
#include "render/viewplanes/ViewPlane.h"

#include <cmath>
#include <cstdint>
#include <limits>

using namespace engine::raster;

Rasterizer::Rasterizer(std::shared_ptr<render::Scene> scene)
  : RenderEngine(std::move(scene))
{
}

Rasterizer::Rasterizer(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene)
  : RenderEngine(std::move(camera), std::move(scene))
{
}

Rasterizer::~Rasterizer() = default;

void Rasterizer::cancel() {
  m_cancelled.store(true);
}

void Rasterizer::uncancel() {
  m_cancelled.store(false);
}

namespace {
  // A reasonably colour-spread hash from a uint64 face index → RGB
  // in [0, 1]³. Used so adjacent faces are visually distinguishable
  // in the V1 flat-shaded output. Replaced by real shading in a
  // later phase.
  Colord faceColor(std::uint64_t index) {
    // Three independent hashes — the multipliers are large primes
    // that produce well-separated bit patterns under modulo.
    const std::uint64_t r = (index * 2654435761ULL)        & 0xFFu;
    const std::uint64_t g = (index * 40503ULL + 12345)     & 0xFFu;
    const std::uint64_t b = (index * 15485863ULL + 999983) & 0xFFu;
    // Bias toward the 0.3-1.0 range so triangles don't wash out
    // against a black background.
    return Colord(0.3 + (r / 255.0) * 0.7,
                  0.3 + (g / 255.0) * 0.7,
                  0.3 + (b / 255.0) * 0.7);
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

  auto mesh = m_scene->tessellate(m_lod);
  if (!mesh) return;

  const auto& vertices = mesh->vertices();
  const auto& faces = mesh->faces();

  const int width = buffer.width();
  const int height = buffer.height();

  // Z-buffer: per-pixel eye-relative depth, initialised to +infinity
  // so the first triangle to write any pixel always wins. Smaller
  // depth = closer to the eye; the test "new < old" replaces the cell.
  Buffer<double> zBuffer(width, height);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      zBuffer[y][x] = std::numeric_limits<double>::infinity();

  for (std::uint64_t faceIdx = 0; faceIdx < faces.size(); ++faceIdx) {
    if (m_cancelled.load()) return;

    const auto& face = faces[faceIdx];
    if (face.size() < 3) continue;

    const Colord color = faceColor(faceIdx);

    // Triangulate the face as a fan from vertex 0. The per-primitive
    // tessellate impls produce convex faces (quads or triangles), so
    // the simple fan is correct for everything that ships in this
    // codebase.
    for (std::size_t i = 1; i + 1 < face.size(); ++i) {
      const Vector3d& w0 = vertices[face[0]].point;
      const Vector3d& w1 = vertices[face[i]].point;
      const Vector3d& w2 = vertices[face[i + 1]].point;

      const Vector3d s0 = m_camera->projectPointWithDepth(w0);
      const Vector3d s1 = m_camera->projectPointWithDepth(w1);
      const Vector3d s2 = m_camera->projectPointWithDepth(w2);

      // Skip if any vertex projection is undefined — vertex behind
      // the eye, camera with no closed-form inverse, or otherwise
      // unprojectable. The clipping pass that handles the
      // partially-behind case lands in a later phase.
      if (s0.isUndefined() || s1.isUndefined() || s2.isUndefined()) continue;

      const double z0 = s0.z(), z1 = s1.z(), z2 = s2.z();
      const double invZ0 = 1.0 / z0, invZ1 = 1.0 / z1, invZ2 = 1.0 / z2;

      const int x0 = static_cast<int>(std::lround(s0.x()));
      const int y0 = static_cast<int>(std::lround(s0.y()));
      const int x1 = static_cast<int>(std::lround(s1.x()));
      const int y1 = static_cast<int>(std::lround(s1.y()));
      const int x2 = static_cast<int>(std::lround(s2.x()));
      const int y2 = static_cast<int>(std::lround(s2.y()));

      core::rasterizeTriangle(x0, y0, x1, y1, x2, y2,
        [&](int x, int y, double w0b, double w1b, double w2b) {
          if (x < 0 || x >= width || y < 0 || y >= height) return;
          // Perspective-correct depth interpolation. The screen-space
          // barycentric weights from `rasterizeTriangle` are linear
          // in screen space — but vertex *depth* is not. The standard
          // trick: 1/z IS linear in screen space, so interpolate 1/z
          // and invert. (Heckbert & Moreton 1991, "Interpolation for
          // polygon texture mapping and shading".)
          const double oneOverZ = w0b * invZ0 + w1b * invZ1 + w2b * invZ2;
          const double pixelDepth = 1.0 / oneOverZ;
          if (pixelDepth < zBuffer[y][x]) {
            zBuffer[y][x] = pixelDepth;
            buffer[y][x] = color;
          }
        });
    }
  }
}
