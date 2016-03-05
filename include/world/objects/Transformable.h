#pragma once

#include "world/objects/Element.h"
#include "core/math/Vector.h"
#include "core/math/Matrix.h"

class Transformable : public Element {
  Q_OBJECT;
  Q_PROPERTY(Vector3d position READ position WRITE setPosition);
  Q_PROPERTY(Vector3d rotation READ rotation WRITE setRotation);
  Q_PROPERTY(Vector3d scale READ scale WRITE setScale);
  
public:
  Transformable(Element* parent = nullptr);
  
  inline const Vector3d& position() const {
    return m_position;
  }
  
  inline void setPosition(const Vector3d& position) {
    m_position = position;
  }
  
  inline const Vector3d& rotation() const {
    return m_rotation;
  }
  
  inline void setRotation(const Vector3d& rotation) {
    m_rotation = rotation;
  }
  
  inline const Vector3d& scale() const {
    return m_scale;
  }
  
  inline void setScale(const Vector3d& scale) {
    m_scale = Vector3d(
      std::max(std::abs(scale.x()), std::numeric_limits<double>::epsilon()),
      std::max(std::abs(scale.y()), std::numeric_limits<double>::epsilon()),
      std::max(std::abs(scale.z()), std::numeric_limits<double>::epsilon())
    );
  }
  
  void setMatrix(const Matrix4d& matrix);

  Matrix4d localTransform() const;
  Matrix4d globalTransform() const;
  
  virtual bool canHaveChild(Element* child) const;
  
protected:
  virtual void leaveParent();
  virtual void joinParent();
  
private:
  Vector3d m_position;
  Vector3d m_rotation;
  Vector3d m_scale;
};
