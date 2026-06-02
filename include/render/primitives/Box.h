#pragma once

#include "render/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace render {
  /**
    * @brief An axis-aligned box defined by its `center` and half-edge
    *        vector.
    *
    * Same scene rendered through every supporting engine — only the
    * integrator differs:
    *
    * <table>
    *   <tr>
    *     <th>Raytracer</th>
    *     <th>Software rasterizer</th>
    *     <th>Wireframe</th>
    *   </tr>
    *   <tr>
    *     <td>@image html box__raytracer.png ""</td>
    *     <td>@image html box__raster.png ""</td>
    *     <td>@image html box__wireframe.png ""</td>
    *   </tr>
    * </table>
    */
  class Box : public Primitive {
  public:
    inline explicit Box(const Vector3d& center, const Vector3d& edge)
        : m_center(center),
          m_edge(edge) {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints,
                                       render::State& state) const override;
    RayPacketIntersection4 intersectPacket(const Ray4& rays, render::State& state) const override;
    PrimitivePacketHit4 intersectPacketHits(const Ray4& rays,
                                            const PrimitivePacketState4& states) const override;
    PrimitivePacketHit8 intersectPacketHits(const Ray8& rays,
                                            const PrimitivePacketState8& states) const override;
    PrimitivePacketInterval4
    intersectPacketIntervals(const Ray4& rays, const PrimitivePacketState4& states) const override;
    PrimitivePacketInterval8
    intersectPacketIntervals(const Ray8& rays, const PrimitivePacketState8& states) const override;
    virtual bool intersects(const Rayd& ray, render::State& state) const override;

    /**
      * @returns the farthest point on the box in the given direction. The
      *   following interactive figure illustrates the geometry. Use the
      *   direction-angle slider to change the direction vector. The
      *   resulting point is highlighted in red.
      * 
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="box_farthest_point.js"></script>
      * @endhtmlonly
      */
    virtual Vector3d farthestPoint(const Vector3d& direction) const override;

    /**
      * Produce the canonical 12-triangle box mesh. Box is polyhedral
      * already, so `lod` is ignored — every level of detail is the
      * same six-face tessellation. Each face contributes 4 vertices
      * with face-local normals (so the box renders as flat-shaded,
      * not smooth-shaded across edges) and per-face UVs spanning
      * `[0, 1]²`.
      *
      * Vertex layout: 24 vertices total — 4 per face × 6 faces.
      * Vertices are NOT shared across faces because the normals and
      * UVs differ per face; sharing would require splitting on
      * texture seams anyway.
      *
      * @image html box__wireframe.png "Box rendered through Wireframe"
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const override;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;

  private:
    template<typename Packet, typename StateArray, typename Result>
    Result intersectPacketHitsFor(const Packet& rays, const StateArray& states) const;
    template<typename Packet, typename StateArray, typename Result>
    Result intersectPacketIntervalsFor(const Packet& rays, const StateArray& states) const;

    Vector3d m_center, m_edge;
  };
}
