#include "engine/raster/Rasterizer.h"

#include "core/Buffer.h"
#include "core/geometry/Mesh.h"
#include "core/geometry/Rasterize.h"
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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

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
  // Ambient coefficient — same role as MatteMaterial's
  // `ambientCoefficient`. Multiplies the scene's ambient term so
  // the unlit side of an object is visible at its full ambient
  // contribution rather than darkened.
  constexpr double kAmbientCoefficient = 1.0;

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

  // Z-buffer: per-pixel eye-relative depth, initialised to +infinity
  // so the first triangle to write any pixel always wins. Smaller
  // depth = closer to the eye; the test "new < old" replaces the cell.
  Buffer<double> zBuffer(width, height);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
      zBuffer[y][x] = std::numeric_limits<double>::infinity();

  // Walk leaf primitives so each face carries the hit primitive's
  // material — the scene-level merged tessellation would lose that
  // association. Per-leaf tessellation is no more expensive than the
  // scene-level call (Scene::tessellate just concatenates child
  // meshes); the difference is the metadata we keep.
  std::uint64_t globalFaceIdx = 0;
  walkLeaves(m_scene.get(), nullptr,
    [&](const render::Primitive* primitive, std::shared_ptr<render::Material> material) {
      if (m_cancelled.load()) return;

      auto mesh = primitive->tessellate(m_lod);
      if (!mesh) return;

      const auto& vertices = mesh->vertices();
      const auto& faces = mesh->faces();

      for (std::size_t fi = 0; fi < faces.size(); ++fi, ++globalFaceIdx) {
        if (m_cancelled.load()) return;

        const auto& face = faces[fi];
        if (face.size() < 3) continue;

        for (std::size_t i = 1; i + 1 < face.size(); ++i) {
          // Sutherland-Hodgman near-plane clip — the input triangle's
          // three vertices, classified by signed eye-relative depth.
          // Vertices on or behind the near plane (depth < EPS) are
          // unprojectable; clipping replaces each crossing edge with
          // its intersection with the plane, producing an output
          // polygon (3 or 4 vertices) entirely in front of the eye.
          //
          // Without this, triangles straddling the near plane get
          // dropped wholesale — most visibly the floor box in scenes
          // where the camera is close to the floor, leaving only the
          // tiny far-edge triangle of the floor visible. The classic
          // GPU pipeline does the same clip in clip space; doing it
          // in eye-space here is the simpler textbook formulation.
          struct ClipVert {
            Vector3d point;
            Vector3d normal;
            double depth;
          };
          const std::array<ClipVert, 3> input = {{
            { vertices[face[0]].point, vertices[face[0]].normal,
              m_camera->eyeRelativeDepth(vertices[face[0]].point) },
            { vertices[face[i]].point, vertices[face[i]].normal,
              m_camera->eyeRelativeDepth(vertices[face[i]].point) },
            { vertices[face[i + 1]].point, vertices[face[i + 1]].normal,
              m_camera->eyeRelativeDepth(vertices[face[i + 1]].point) },
          }};

          // EPS pulled in from the eye by a small distance — far
          // enough to keep the perspective divide bounded at the
          // clipped vertex (otherwise the projected screen coords
          // explode and the rasterizer's bounding-box scan iterates
          // for hundreds of millions of pixels off-screen) without
          // introducing visible offset on unclipped geometry. 0.1
          // is empirically safe for unit-scale scenes on a Pinhole
          // with `m_distance ≈ 5`: clipped vertices project at most
          // ~50× the eye's view-plane angle, which keeps screen
          // coords within sensible bounds for typical viewports.
          constexpr double kClipEps = 0.1;

          std::vector<ClipVert> clipped;
          clipped.reserve(4);
          for (std::size_t k = 0; k < 3; ++k) {
            const ClipVert& curr = input[k];
            const ClipVert& prev = input[(k + 2) % 3];
            const bool currIn = curr.depth >= kClipEps;
            const bool prevIn = prev.depth >= kClipEps;
            if (currIn != prevIn) {
              // Edge crosses — interpolate to find the intersection.
              const double t = (kClipEps - prev.depth) / (curr.depth - prev.depth);
              ClipVert mid;
              mid.point = prev.point + (curr.point - prev.point) * t;
              mid.normal = prev.normal + (curr.normal - prev.normal) * t;
              mid.depth = kClipEps;
              clipped.push_back(mid);
            }
            if (currIn) clipped.push_back(curr);
          }
          if (clipped.size() < 3) continue;

          // Triangulate the clipped polygon as a fan from clipped[0].
          // For a 3-vertex result this is a single triangle; for a
          // 4-vertex result, two triangles.
          for (std::size_t t = 1; t + 1 < clipped.size(); ++t) {
            const ClipVert& v0 = clipped[0];
            const ClipVert& v1 = clipped[t];
            const ClipVert& v2 = clipped[t + 1];

            const Vector3d s0 = m_camera->projectPointWithDepth(v0.point);
            const Vector3d s1 = m_camera->projectPointWithDepth(v1.point);
            const Vector3d s2 = m_camera->projectPointWithDepth(v2.point);
            if (s0.isUndefined() || s1.isUndefined() || s2.isUndefined()) continue;

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
              [&, capturedFaceIdx](int x, int y, double w0b, double w1b, double w2b) {
              if (x < 0 || x >= width || y < 0 || y >= height) return;
              // Perspective-correct depth interpolation. The
              // screen-space barycentric weights from
              // `rasterizeTriangle` are linear in screen space — but
              // vertex *depth* is not. The standard trick: 1/z IS
              // linear in screen space, so interpolate 1/z and
              // invert. (Heckbert & Moreton 1991.)
              const double oneOverZ = w0b * invZ0 + w1b * invZ1 + w2b * invZ2;
              const double pixelDepth = 1.0 / oneOverZ;
              if (pixelDepth >= zBuffer[y][x]) return;

              // Perspective-correct attribute interpolation: same
              // trick as depth, applied to vertex normals and world
              // positions:
              //   attr_pixel = (Σ_i w_i · attr_i / z_i) · pixelDepth
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

              // Lambertian shading. No shadow rays (no recursive ray
              // tracing in this engine); each light contributes
              // diffuse-cosine-weighted radiance directly.
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
}
