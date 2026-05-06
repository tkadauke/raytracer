#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
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

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

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
      * The interactive widget below shows the fan layout from above
      * with a live LOD slider.
      *
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="disk_tessellate.js"></script>
      * @endhtmlonly
      *
      * @image html disk_wireframe.png "Disk rendered through WireframeEngine"
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Vector4d m_center;
    Vector3d m_normal;
    double m_radius, m_squaredRadius;
  };
}
