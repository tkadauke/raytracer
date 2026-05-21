#include "engine/wireframe/Wireframe.h"

#include "core/Buffer.h"
#include "core/geometry/Bresenham.h"
#include "core/geometry/Mesh.h"
#include "core/math/Vector.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"
#include "render/viewplanes/ViewPlane.h"

#include <cmath>

using namespace engine::wireframe;

Wireframe::Wireframe(std::shared_ptr<render::Scene> scene)
  : RenderEngine(std::move(scene))
{
}

Wireframe::Wireframe(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene)
  : RenderEngine(std::move(camera), std::move(scene))
{
}

Wireframe::~Wireframe() = default;

std::shared_ptr<render::RenderEngine> Wireframe::cloneForRender() const {
  auto result = std::make_shared<Wireframe>(
    m_camera ? m_camera->clone() : nullptr,
    m_scene
  );
  result->setTonemap(tonemap());
  result->setLod(m_lod);
  result->setEdgeColor(m_edgeColor);
  result->setBackgroundColor(m_backgroundColor);
  return result;
}

void Wireframe::cancel() {
  m_cancelled.store(true);
}

void Wireframe::uncancel() {
  m_cancelled.store(false);
}

namespace {
  // Project a single edge from world space to screen space and
  // rasterize it into `buffer`. Returns silently if either endpoint
  // is behind the eye (projectPoint returns Vector2d::undefined)
  // — V1 doesn't clip mixed-case lines against the near plane, so
  // edges that straddle the camera are dropped entirely. That's a
  // visible glitch when the camera moves close to geometry, fixed in
  // V2 by Liang-Barsky or Cohen-Sutherland clipping.
  void rasterizeEdge(Buffer<Colord>& buffer,
                     const render::Camera& camera,
                     const Vector3d& worldA,
                     const Vector3d& worldB,
                     const Colord& color) {
    Vector2d a = camera.projectPoint(worldA);
    Vector2d b = camera.projectPoint(worldB);
    if (a.isUndefined() || b.isUndefined()) return;

    int x0 = static_cast<int>(std::lround(a.x()));
    int y0 = static_cast<int>(std::lround(a.y()));
    int x1 = static_cast<int>(std::lround(b.x()));
    int y1 = static_cast<int>(std::lround(b.y()));

    const int width = buffer.width();
    const int height = buffer.height();

    core::drawLine(x0, y0, x1, y1, [&](int x, int y) {
      if (x >= 0 && x < width && y >= 0 && y < height) {
        buffer[y][x] = color;
      }
    });
  }
}  // namespace

void Wireframe::render(Buffer<Colord>& buffer) {
  // Note: this function does NOT reset the cancellation flag — the
  // caller is expected to call `uncancel()` between renders. That
  // matches the `Raytracer` convention and lets pre-cancellation
  // before the first render work as expected.

  // Clear to background colour. Buffer<T>::clear() default-constructs
  // every cell, which would give us black regardless of the
  // configured backgroundColor — write explicitly instead.
  for (int y = 0; y < buffer.height(); ++y)
    for (int x = 0; x < buffer.width(); ++x)
      buffer[y][x] = m_backgroundColor;

  if (!m_scene || !m_camera) return;

  // The same view-plane setup the raytracer engine performs — the
  // camera projection math depends on `topLeft` / `right` / `down`
  // basis vectors that `setupVectors` populates.
  m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());

  auto mesh = m_scene->tessellate(m_lod);
  if (!mesh) return;

  // Walk every face, rasterize every edge. Adjacent faces share
  // edges and we'll redraw shared edges twice — fine for V1, since
  // the visible result is identical. Edge deduplication would help
  // raster performance for very dense meshes (Sphere at lod=3 has
  // 8192 quads = 32k edges, of which ~half are shared).
  const auto& vertices = mesh->vertices();
  for (const auto& face : mesh->faces()) {
    if (m_cancelled.load()) return;

    const std::size_t n = face.size();
    for (std::size_t i = 0; i < n; ++i) {
      const Vector3d& a = vertices[face[i]].point;
      const Vector3d& b = vertices[face[(i + 1) % n]].point;
      rasterizeEdge(buffer, *m_camera, a, b, m_edgeColor);
    }
  }
}
