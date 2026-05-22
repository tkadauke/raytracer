#pragma once

#include "render/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace render {
  /**
    * Torus primitive, rendered through the engines that support it:
    *
    * <table><tr>
    * <td>@image html torus__raytracer.png "Raytracer"</td>
    * <td>@image html torus__raster.png "Rasterizer"</td>
    * <td>@image html torus__wireframe.png "Wireframe"</td>
    * </tr></table>
    */
  class Torus : public Primitive {
  public:
    inline explicit Torus(double sweptRadius, double tubeRadius)
      : m_sweptRadius(sweptRadius),
        m_tubeRadius(tubeRadius)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, render::State& state) const override;

    /**
      * Mesh approximation. The torus is aligned with its hole along
      * the +Y axis (the ring lies in the XZ plane), matching the
      * orientation of `Torus::intersect`.
      *
      * Major × minor parametrisation:
      * - Major angle u ∈ [0, 2π] (around the ring), minor angle
      *   v ∈ [0, 2π] (around the tube).
      * - point = ((R + r·cos v)·cos u, r·sin v, (R + r·cos v)·sin u).
      * - normal = (cos v·cos u, sin v, cos v·sin u) — unit-length by
      *   construction.
      * - uv = (u/2π, v/2π).
      *
      * LOD schedule: `majorSegs = minorSegs = 16 << lod`. lod=0
      * → 16 × 16 (289 verts / 256 quads), lod=1 → 32 × 32 (1089 /
      * 1024). Vertex count grows ~4× per level — quads are the
      * product of the two dimensions, so torus meshes get expensive
      * fast.
      *
      * Both seams (u=0/2π and v=0/2π) are closed by duplicating the
      * final column and row with u=1 or v=1. Unlike the sphere,
      * neither parametric direction degenerates at a pole — every
      * grid vertex is a regular interior vertex with a well-defined
      * normal, which is what makes torus topology cleaner than
      * sphere topology.
      *
      * The interactive widget below shows the major and minor
      * segmentation as two side-by-side cross-sections. Use the LOD slider to
      * change both segment counts.
      *
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="torus_tessellate.js"></script>
      * @endhtmlonly
      *
      * @image html torus__wireframe.png "Torus rendered through Wireframe"
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const override;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;

  private:
    Vector3d computeNormal(const Vector3d& p) const;

    double m_sweptRadius;
    double m_tubeRadius;
  };
}
