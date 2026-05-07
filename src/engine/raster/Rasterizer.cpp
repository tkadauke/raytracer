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

      const Vector2d s0 = m_camera->projectPoint(w0);
      const Vector2d s1 = m_camera->projectPoint(w1);
      const Vector2d s2 = m_camera->projectPoint(w2);

      // Skip if any vertex projection is undefined (camera has no
      // closed-form inverse, or vertex is behind the camera). V1
      // doesn't clip mixed-case triangles against the near plane —
      // they're dropped entirely. The clipping pass lands in a
      // later phase.
      if (s0.isUndefined() || s1.isUndefined() || s2.isUndefined()) continue;

      const int x0 = static_cast<int>(std::lround(s0.x()));
      const int y0 = static_cast<int>(std::lround(s0.y()));
      const int x1 = static_cast<int>(std::lround(s1.x()));
      const int y1 = static_cast<int>(std::lround(s1.y()));
      const int x2 = static_cast<int>(std::lround(s2.x()));
      const int y2 = static_cast<int>(std::lround(s2.y()));

      core::rasterizeTriangle(x0, y0, x1, y1, x2, y2,
        [&](int x, int y, double, double, double) {
          if (x >= 0 && x < width && y >= 0 && y < height) {
            buffer[y][x] = color;
          }
        });
    }
  }
}
