#pragma once

#include "raytracer/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace raytracer {
  class Box : public Primitive {
  public:
    inline explicit Box(const Vector3d& center, const Vector3d& edge)
      : m_center(center),
        m_edge(edge)
    {
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, State& state) const;
    virtual bool intersects(const Rayd& ray, State& state) const;

    /**
      * @returns the farthest point on the box in the given diretion. The
      *   following interactive figure illustrates the geometry. Click and drag
      *   horizontally to change the angle of the direction vector. The
      *   resulting point is highlighted in red.
      * 
      * @htmlonly
      * <script type="text/javascript" src="figure.js"></script>
      * <script type="text/javascript" src="box_farthest_point.js"></script>
      * @endhtmlonly
      */
    virtual Vector3d farthestPoint(const Vector3d& direction) const;

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
      * @image html box_wireframe.png "Box rendered through WireframeEngine"
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod = 0) const;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const;

  private:
    Vector3d m_center, m_edge;
  };
}
