#pragma once

#include "render/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace render {
  /**
    * @brief A sphere defined by its centre `origin` and `radius`.
    *
    * The same sphere rendered through every engine, side by side —
    * shared scene + camera, only the integrator differs. Compare
    * what each pipeline preserves and drops:
    *
    * <table>
    *   <tr>
    *     <th>Raytracer</th>
    *     <th>Software rasterizer</th>
    *     <th>Wireframe</th>
    *   </tr>
    *   <tr>
    *     <td>@image html sphere__raytracer.png ""</td>
    *     <td>@image html sphere__raster.png ""</td>
    *     <td>@image html sphere__wireframe.png ""</td>
    *   </tr>
    * </table>
    *
    * The raytracer renders the full Whitted pipeline: textured
    * checkerboard floor, sky background, soft shadow under the
    * sphere, the floor faintly reflected in the sphere's lower
    * hemisphere. The rasterizer projects the same scene's tessellated
    * triangles directly to screen and Lambertian-shades each pixel
    * with the primitive's own material albedo — no recursion, so
    * reflections, refractions, and proper shadows are absent, and
    * the engine's default black fills the background where the
    * raytracer renders sky. The comparison makes those differences
    * visible directly.
    */
  class Sphere : public Primitive {
  public:
    inline explicit Sphere(const Vector3d& origin, double radius)
        : m_origin(origin),
          m_radius(radius) {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;
    RayPacketIntersection4 intersectPacket(const Ray4& rays, render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                            const PrimitivePacketState8& states) const override;
    virtual bool intersects(const Rayd& ray, render::State& state) const override;

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
      * side view. Use the LOD slider to change the segment count. Pole vertices
      * (drawn red)
      * collapse to a single point on this projection but live as
      * `lonSegs + 1` distinct mesh vertices.
      *
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="sphere_tessellate.js"></script>
      * @endhtmlonly
      *
      * @image html sphere__wireframe.png "Sphere rendered through Wireframe"
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const override;

    /**
      * Returns the farthest point on the sphere in the given direction. The
      * following interactive figure illustrates the geometry. Use the
      * direction-angle slider to change the direction vector. The resulting
      * point is highlighted in red.
      * 
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="sphere_farthest_point.js"></script>
      * @endhtmlonly
      */
    virtual Vector3d farthestPoint(const Vector3d& direction) const override;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;

  private:
    template<typename Packet, typename StateArray, typename Result>
    Result intersectPacketHitsFor(const Packet& rays, const StateArray& states) const;

    Vector3d m_origin;
    double m_radius;
  };
}
