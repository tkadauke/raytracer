#include "engine/wireframe/Wireframe.h"

#include "core/Buffer.h"
#include "core/geometry/Bresenham.h"
#include "core/geometry/Mesh.h"
#include "core/math/Vector.h"
#include "render/cameras/Camera.h"
#include "render/primitives/Scene.h"
#include "render/viewplanes/ViewPlane.h"

#include <cmath>
#include <optional>

using namespace engine::wireframe;

Wireframe::Wireframe(std::shared_ptr<render::Scene> scene)
    : RenderEngine(std::move(scene)) {
}

Wireframe::Wireframe(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene)
    : RenderEngine(std::move(camera), std::move(scene)) {
}

Wireframe::~Wireframe() = default;

std::shared_ptr<render::RenderEngine> Wireframe::cloneForRender() const {
  auto result = std::make_shared<Wireframe>(m_camera ? m_camera->clone() : nullptr, m_scene);
  result->setTonemap(tonemap());
  result->setLod(m_lod);
  result->setEdgeColor(m_edgeColor);
  result->setNearClipDepth(m_nearClipDepth);
  result->setGeometryMode(m_geometryMode);
  if (hasBackgroundColorOverride()) {
    result->setBackgroundColor(backgroundColor());
  }
  return result;
}

Colord Wireframe::backgroundColor() const {
  // Override the RenderEngine default: when there's no explicit
  // override, fall back to black instead of the scene's background.
  // Keeps the canonical lines-on-black look even when the scene
  // carries a colourful background.
  if (hasBackgroundColorOverride()) {
    return *m_backgroundColorOverride;
  }
  return Colord::black();
}

void Wireframe::cancel() {
  m_cancelled.store(true);
}

void Wireframe::uncancel() {
  m_cancelled.store(false);
}

void Wireframe::setNearClipDepth(double depth) {
  m_nearClipDepth = (std::isfinite(depth) && depth > 0.0) ? depth : 0.1;
}

namespace {
  bool clipEdgeToNearPlane(const render::Camera& camera, Vector3d& worldA, Vector3d& worldB,
                           double nearClipDepth) {
    const double depthA = camera.eyeRelativeDepth(worldA);
    const double depthB = camera.eyeRelativeDepth(worldB);
    if (!std::isfinite(depthA) || !std::isfinite(depthB)) {
      return false;
    }

    const bool aInside = depthA >= nearClipDepth;
    const bool bInside = depthB >= nearClipDepth;
    if (!aInside && !bInside) {
      return false;
    }

    if (aInside != bInside) {
      const double t = (nearClipDepth - depthA) / (depthB - depthA);
      const Vector3d clipped = worldA + (worldB - worldA) * t;
      if (aInside) {
        worldB = clipped;
      } else {
        worldA = clipped;
      }
    }

    return true;
  }

  // Project a single edge from world space to screen space and rasterize it
  // into `buffer`. Edges that cross the near plane are shortened before
  // projection so close camera moves do not drop otherwise-visible lines.
  void rasterizeEdge(Buffer<Colord>& buffer, const render::Camera& camera, const Vector3d& worldA,
                     const Vector3d& worldB, const Colord& color, double nearClipDepth) {
    Vector3d clippedA = worldA;
    Vector3d clippedB = worldB;
    if (!clipEdgeToNearPlane(camera, clippedA, clippedB, nearClipDepth)) {
      return;
    }

    Vector2d a = camera.projectPoint(clippedA);
    Vector2d b = camera.projectPoint(clippedB);
    if (a.isUndefined() || b.isUndefined())
      return;

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
} // namespace

void Wireframe::render(Buffer<Colord>& buffer) {
  // Note: this function does NOT reset the cancellation flag — the
  // caller is expected to call `uncancel()` between renders. That
  // matches the `Raytracer` convention and lets pre-cancellation
  // before the first render work as expected.

  // Clear to background colour. Buffer<T>::clear() default-constructs
  // every cell, which would give us black regardless of the
  // configured backgroundColor — write explicitly instead.
  const Colord bg = backgroundColor();
  for (int y = 0; y < buffer.height(); ++y)
    for (int x = 0; x < buffer.width(); ++x)
      buffer[y][x] = bg;

  if (!m_scene || !m_camera)
    return;

  // The same view-plane setup the raytracer engine performs — the
  // camera projection math depends on `topLeft` / `right` / `down`
  // basis vectors that `setupVectors` populates.
  m_camera->viewPlane()->setup(m_camera->matrix(), buffer.rect());

  if (m_geometryMode == GeometryMode::CurveOverlay) {
    m_scene->forEachCurveOverlaySegment(
      [&](const Vector3d& start, const Vector3d& end, const std::optional<Colord>& color) {
        if (m_cancelled.load())
          return;
        rasterizeEdge(buffer, *m_camera, start, end, color ? *color : m_edgeColor,
                      m_nearClipDepth);
      });
    return;
  }

  auto mesh = m_scene->tessellate(m_lod);
  if (!mesh)
    return;

  // Walk every face, rasterize every edge. Adjacent faces share
  // edges and we'll redraw shared edges twice — fine for V1, since
  // the visible result is identical. Edge deduplication would help
  // raster performance for very dense meshes (Sphere at lod=3 has
  // 8192 quads = 32k edges, of which ~half are shared).
  const auto& vertices = mesh->vertices();
  for (const auto& face : mesh->faces()) {
    if (m_cancelled.load())
      return;

    const std::size_t n = face.size();
    for (std::size_t i = 0; i < n; ++i) {
      const Vector3d& a = vertices[face[i]].point;
      const Vector3d& b = vertices[face[(i + 1) % n]].point;
      rasterizeEdge(buffer, *m_camera, a, b, m_edgeColor, m_nearClipDepth);
    }
  }
}
