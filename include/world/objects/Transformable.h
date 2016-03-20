#pragma once

#include "world/objects/Element.h"
#include "core/math/Vector.h"
#include "core/math/Matrix.h"

/**
  * Represents an object with a position, size and/or orientation. This includes
  * all visible objects, and lights.
  */
class Transformable : public Element {
  Q_OBJECT;
  Q_PROPERTY(Vector3d position READ position WRITE setPosition);
  Q_PROPERTY(Vector3d rotation READ rotation WRITE setRotation);
  Q_PROPERTY(Vector3d scale READ scale WRITE setScale);
  
public:
  /**
    * Default constructor.
    */
  Transformable(Element* parent = nullptr);
  
  /**
    * @returns the object's position vector.
    */
  inline const Vector3d& position() const {
    return m_position;
  }
  
  /**
    * Sets the object's position.
    * 
    * <table><tr>
    * <td>@image html transformable_position_1.png</td>
    * <td>@image html transformable_position_2.png</td>
    * <td>@image html transformable_position_3.png</td>
    * <td>@image html transformable_position_4.png</td>
    * <td>@image html transformable_position_5.png</td>
    * </tr></table>
    */
  inline void setPosition(const Vector3d& position) {
    m_position = position;
  }
  
  /**
    * @returns the object's rotation vector, containing the three Euler angles,
    * which describe this object's orientation. The Euler angles are in radians.
    */
  inline const Vector3d& rotation() const {
    return m_rotation;
  }
  
  /**
    * Sets the object's rotation vector, which is a vector of Euler angles in
    * radians.
    * 
    * <table><tr>
    * <td>@image html transformable_rotation_1.png</td>
    * <td>@image html transformable_rotation_2.png</td>
    * <td>@image html transformable_rotation_3.png</td>
    * <td>@image html transformable_rotation_4.png</td>
    * <td>@image html transformable_rotation_5.png</td>
    * </tr></table>
    */
  inline void setRotation(const Vector3d& rotation) {
    m_rotation = rotation;
  }
  
  /**
    * @returns the object's scale vector, containing the scale factors along the
    * x, y, and z axes.
    */
  inline const Vector3d& scale() const {
    return m_scale;
  }
  
  /**
    * Sets the scale vector for the x, y, and z axes.
    * 
    * <table><tr>
    * <td>@image html transformable_scale_1.png</td>
    * <td>@image html transformable_scale_2.png</td>
    * <td>@image html transformable_scale_3.png</td>
    * <td>@image html transformable_scale_4.png</td>
    * <td>@image html transformable_scale_5.png</td>
    * </tr></table>
    */
  inline void setScale(const Vector3d& scale) {
    m_scale = Vector3d(
      std::max(std::abs(scale.x()), 0.000001),
      std::max(std::abs(scale.y()), 0.000001),
      std::max(std::abs(scale.z()), 0.000001)
    );
  }
  
  /**
    * Extracts the position, rotation and scale vectors from matrix.
    */
  void setMatrix(const Matrix4d& matrix);

  /**
    * @returns the local transformation matrix from the position, rotation, and
    * scale vectors.
    */
  Matrix4d localTransform() const;
  
  /**
    * @returns the global transformation matrix by recursively multiplying the
    * local transformation matrix with this object's parent's global
    * transformation matrix. Effectively, the result is the transformation in
    * world coordinate space.
    */
  Matrix4d globalTransform() const;
  
  virtual bool canHaveChild(Element* child) const;
  
  /**
    * Moves the object by vector. If global is true, the object is moved
    * relative to world coordinates. If it is false, it is moved relative to
    * local coordinates.
    */
  void moveBy(const Vector3d& vector, bool global = false);
  
protected:
  virtual void leaveParent();
  virtual void joinParent();
  
private:
  Vector3d m_position;
  Vector3d m_rotation;
  Vector3d m_scale;
};
