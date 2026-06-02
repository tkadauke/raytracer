#pragma once

#include "render/primitives/Primitive.h"
#include "core/DivisionByZeroException.h"
#include "core/math/Vector.h"

namespace render {
  /**
    * Open cylinder side surface, rendered through the engines that
    * support it:
    *
    * <table><tr>
    * <td>@image html open_cylinder__raytracer.png "Raytracer"</td>
    * <td>@image html open_cylinder__raster.png "Rasterizer"</td>
    * <td>@image html open_cylinder__wireframe.png "Wireframe"</td>
    * </tr></table>
    */
  class OpenCylinder : public Primitive {
  public:
    /**
      * Constructs an open cylinder with the given @p radius and @p height.
      *
      * @throws DivisionByZeroException if @p radius is zero — IEEE 754
      *   division by zero would otherwise silently leave m_invRadius as
      *   +Infinity, which propagates into surface normals and corrupts
      *   shading for every ray hitting the cylinder.
      */
    inline explicit OpenCylinder(double radius, double height)
        : m_radius(radius),
          m_halfHeight(height / 2.0),
          m_invRadius(radius == 0.0 ? 0.0 : 1.0 / radius) {
      if (radius == 0.0)
        throw DivisionByZeroException(__FILE__, __LINE__);
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                            const PrimitivePacketState8& states) const override;
    virtual bool intersects(const Rayd& ray, render::State& state) const override;
    virtual Vector3d farthestPoint(const Vector3d& direction) const override;

    /**
      * Tessellates the side surface (no caps) as a quad strip
      * wrapping around the Y axis.
      *
      * LOD schedule: `segments = 16 << lod` (16 / 32 / 64 / 128 …).
      * Increasing LOD reduces silhouette faceting; smooth shading
      * already hides interior faceting because every vertex normal
      * points radially outward, not in the average-of-faces
      * direction.
      *
      * Vertex layout: `2 * (segments + 1)` vertices arranged as
      * bottom/top rings interleaved (`v[2*i] = bottom`,
      * `v[2*i + 1] = top`). The seam at `u = 0` / `u = 1` is
      * duplicated — the first and last column share a 3D position
      * but get distinct UVs so a wrapped texture doesn't smear
      * across the seam. UV `u` wraps `0 → 1` around the axis;
      * `v` is `0` at the bottom rim, `1` at the top.
      *
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="open_cylinder_tessellate.js"></script>
      * @endhtmlonly
      *
      * @image html open_cylinder__wireframe.png "OpenCylinder rendered through Wireframe"
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const override;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;

  private:
    template<typename Packet, typename StateArray, typename Result>
    Result intersectPacketHitsFor(const Packet& rays, const StateArray& states) const;

    double m_radius;
    double m_halfHeight;

    double m_invRadius;
  };
}
