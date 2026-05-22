#pragma once

#include "render/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace render {
  /**
    * Finite disk primitive, rendered through the engines that support
    * it:
    *
    * <table><tr>
    * <td>@image html disk__raytracer.png "Raytracer"</td>
    * <td>@image html disk__raster.png "Rasterizer"</td>
    * <td>@image html disk__wireframe.png "Wireframe"</td>
    * </tr></table>
    */
  class Disk : public Primitive {
  public:
    inline explicit Disk(const Vector3d& center, const Vector3d& normal, double radius)
      : Primitive(),
        m_center(center),
        m_normal(normal),
        m_radius(radius),
        m_squaredRadius(radius * radius)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, render::State& state) const override;
    virtual Vector3d farthestPoint(const Vector3d& direction) const override;

    /**
      * Triangle-fan tessellation: one centre vertex plus N rim
      * vertices, producing N triangles that all share the centre.
      *
      * LOD schedule: `segments = 16 << lod` (16 / 32 / 64 / 128 …).
      * Bumping LOD by one halves the angular gap on the rim — the
      * polygon converges on the inscribed circle quadratically while
      * vertex/triangle count grows only linearly. Most renders need
      * `lod = 0` or `lod = 1`.
      *
      * UVs lay the disk into the unit square: centre at `(0.5, 0.5)`,
      * rim vertex `i` at `(0.5 + 0.5·cos θᵢ, 0.5 + 0.5·sin θᵢ)`.
      * All vertex normals equal the disk's plane normal — flat
      * shading, since a tessellated disk is mathematically flat.
      *
      * The interactive widget below shows the fan layout from above. Use the
      * LOD slider to change the segment count.
      *
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="disk_tessellate.js"></script>
      * @endhtmlonly
      *
      * @image html disk__wireframe.png "Disk rendered through Wireframe"
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const override;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;

  private:
    Vector4d m_center;
    Vector3d m_normal;
    double m_radius, m_squaredRadius;
  };
}
