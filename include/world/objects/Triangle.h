#pragma once
#include <memory>

#include "world/objects/Surface.h"

/**
  * Represents a single triangle primitive defined by three vertex
  * positions in the local frame. Position and orientation of the
  * triangle as a whole are inherited from `Transformable`.
  *
  * <table><tr>
  * <td>@image html triangle__raytracer.png "Raytracer"</td>
  * <td>@image html triangle__raster.png "Rasterizer"</td>
  * <td>@image html triangle__wireframe.png "Wireframe"</td>
  * </tr></table>
  */
class Triangle : public Surface {
  Q_OBJECT
  Q_PROPERTY(Vector3d vertexA READ vertexA WRITE setVertexA)
  Q_PROPERTY(Vector3d vertexB READ vertexB WRITE setVertexB)
  Q_PROPERTY(Vector3d vertexC READ vertexC WRITE setVertexC)

public:
  explicit Triangle(Element* parent = nullptr);

  inline const Vector3d& vertexA() const {
    return m_vertexA;
  }
  inline const Vector3d& vertexB() const {
    return m_vertexB;
  }
  inline const Vector3d& vertexC() const {
    return m_vertexC;
  }

  inline void setVertexA(const Vector3d& v) {
    m_vertexA = v;
  }
  inline void setVertexB(const Vector3d& v) {
    m_vertexB = v;
  }
  inline void setVertexC(const Vector3d& v) {
    m_vertexC = v;
  }

  virtual std::shared_ptr<render::Primitive> toRaytracerPrimitive() const;

private:
  Vector3d m_vertexA;
  Vector3d m_vertexB;
  Vector3d m_vertexC;
};
