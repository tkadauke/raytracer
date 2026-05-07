#pragma once

#include "render/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace render {
  class Sphere : public Primitive {
  public:
    inline explicit Sphere(const Vector3d& origin, double radius)
      : m_origin(origin),
        m_radius(radius)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, render::State& state) const;
    virtual bool intersects(const Rayd& ray, render::State& state) const;

    /**
      * UV-sphere mesh approximation. Latitude–longitude grid:
      * - Latitude θ ∈ [-π/2, π/2], longitude φ ∈ [0, 2π].
      * - point = origin + radius·(cosθ·cosφ, sinθ, cosθ·sinφ).
      * - normal = (cosθ·cosφ, sinθ, cosθ·sinφ) — unit-length by
      *   construction.
      * - uv = (φ/2π, (θ+π/2)/π).
      *
      * LOD schedule: `lonSegs = 16 << lod`, `latBands = 8 << lod`.
      * lod=0 → 16 × 8 (153 verts / 128 quads), lod=1 → 32 × 16
      * (561 / 512), and so on — vertex count grows ~4× per level
      * because both dimensions double.
      *
      * Pole handling: south-pole and north-pole rows each carry
      * `lonSegs + 1` vertices sharing the same 3D position but
      * carrying distinct u-coordinates. The pinch-free seam keeps
      * UV-mapped textures from collapsing at the poles and lets the
      * quad topology stay uniform across the surface (no fan).
      *
      * The interactive widget below shows the band layout from a
      * side view with a live LOD slider. Pole vertices (drawn red)
      * collapse to a single point on this projection but live as
      * `lonSegs + 1` distinct mesh vertices.
      *
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="sphere_tessellate.js"></script>
      * @endhtmlonly
      *
      * @image html sphere_wireframe.png "Sphere rendered through WireframeEngine"
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const;

    /**
      * Returns the farthest point on the sphere in the given diretion. The
      * following interactive figure illustrates the geometry. Click and drag
      * horizontally to change the angle of the direction vector. The resulting
      * point is highlighted in red.
      * 
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="sphere_farthest_point.js"></script>
      * @endhtmlonly
      */
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Vector3d m_origin;
    double m_radius;
  };
}
