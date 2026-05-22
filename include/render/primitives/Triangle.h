#pragma once

#include "render/primitives/Primitive.h"
#include "core/math/Vector.h"

namespace render {
  /**
    * Single triangle primitive, rendered through the engines that
    * support it:
    *
    * <table><tr>
    * <td>@image html triangle__raytracer.png "Raytracer"</td>
    * <td>@image html triangle__raster.png "Rasterizer"</td>
    * <td>@image html triangle__wireframe.png "Wireframe"</td>
    * </tr></table>
    *
    * Triangle intersection computes barycentric weights for the hit point:
    * `alpha = 1 - beta - gamma`, `beta`, and `gamma`. The same weights
    * define the default tessellated UVs (`p0 -> (0,0)`, `p1 -> (1,0)`,
    * `p2 -> (0,1)`) and are the attribute-interpolation coordinates used
    * by mesh triangles and the rasterizer. `Triangle` itself returns one
    * flat geometric normal, but it follows the same hit-test convention as
    * `FlatMeshTriangle` and `SmoothMeshTriangle`.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="mesh_triangle_interpolation.js"></script>
    * @endhtmlonly
    */
  class Triangle : public Primitive {
  public:
    inline explicit Triangle(const Vector3d& a, const Vector3d& b, const Vector3d& c)
      : Primitive(),
        m_point0(a),
        m_point1(b),
        m_point2(c)
    {
      m_normal = computeNormal();
    }

    virtual const Primitive* intersect(const Rayd& ray, HitPointInterval& hitPoints, render::State& state) const override;
    RayPacketIntersection4 intersectPacket(const Ray4& rays, render::State& state) const override;

    /**
      * Returns a single-triangle Mesh identical to this Triangle. All three
      * vertices carry the triangle's flat normal. UV assignments follow the
      * standard barycentric convention: vertex 0 at (0,0), vertex 1 at (1,0),
      * vertex 2 at (0,1). The @p lod parameter is ignored.
      *
      * @image html triangle__wireframe.png "Triangle rendered through Wireframe"
      */
    virtual std::shared_ptr<Mesh> tessellate(int lod) const override;

  protected:
    virtual BoundingBoxd calculateBoundingBox() const override;

  private:
    Vector3d computeNormal() const;

    Vector3d m_point0, m_point1, m_point2;
    Vector3d m_normal;
  };
}
