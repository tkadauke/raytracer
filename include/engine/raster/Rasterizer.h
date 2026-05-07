#pragma once

#include "render/RenderEngine.h"

#include <atomic>

namespace engine::raster {

/**
  * @brief `RenderEngine` that fills every mesh face's projected
  *        triangle as a flat-shaded coloured region on the
  *        framebuffer — the textbook software-rasterizer pipeline
  *        in the simplest form that produces a recognisable image.
  *
  * Pipeline (V1, no Z-buffer / no shading / no clipping):
  *
  *  1. Tessellate the scene into a single `Mesh` via
  *     `Scene::tessellate(lod)`.
  *  2. For every face, project its vertices to screen space via
  *     `Camera::projectPoint` (same forward-projection used by
  *     `engine::wireframe::Wireframe`).
  *  3. Triangulate the face (fan from vertex 0 — assumes convex
  *     faces, which the per-primitive tessellate impls guarantee).
  *  4. Rasterize each triangle via `core::rasterizeTriangle`,
  *     writing a per-face hash colour for every pixel inside.
  *
  * V1 omits the depth pass — overlapping faces overdraw and the
  * last-rasterized one wins. That's the simplest possible pixel
  * pipeline that still demonstrates the projection + coverage
  * stages; later phases add the depth buffer, vertex normal
  * interpolation, Lambertian shading via `Material`, backface
  * culling, and near-plane clipping.
  *
  * Cameras supported: any subclass that overrides
  * `Camera::projectPoint` (currently `PinholeCamera` and
  * inheritors `ThinLensCamera` / `TiltShiftCamera`). Cameras
  * without a closed-form inverse (`FishEyeCamera`,
  * `SphericalCamera`, `EquirectangularCamera`) silently produce
  * empty / degenerate renders.
  *
  * Threading: V1 renders on the calling thread.
  *
  * @see Wireframe — the cheaper sibling that draws only edges; the
  *      same projection + tessellation pipeline drives both.
  */
class Rasterizer : public render::RenderEngine {
public:
  explicit Rasterizer(std::shared_ptr<render::Scene> scene);
  Rasterizer(std::shared_ptr<render::Camera> camera, std::shared_ptr<render::Scene> scene);

  ~Rasterizer() override;

  using RenderEngine::render;
  void render(Buffer<Colord>& buffer) override;
  void cancel() override;
  void uncancel() override;

  /// Level of detail forwarded to `Primitive::tessellate(lod)`.
  /// Higher values produce denser triangulation (a UV sphere at
  /// `lod=0` has 128 quads = 256 triangles; `lod=2` has 2048 quads).
  inline int lod() const { return m_lod; }
  inline void setLod(int lod) { m_lod = lod; }

  /// Colour the framebuffer is cleared to before triangles are
  /// rasterized. Defaults to pure black (`Colord::black()`).
  inline const Colord& backgroundColor() const { return m_backgroundColor; }
  inline void setBackgroundColor(const Colord& color) { m_backgroundColor = color; }

private:
  std::atomic<bool> m_cancelled{false};
  int m_lod{0};
  Colord m_backgroundColor{Colord::black()};
};

}  // namespace engine::raster
