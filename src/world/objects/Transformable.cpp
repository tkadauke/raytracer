#include "world/objects/Transformable.h"
#include "world/objects/Material.h"
#include "raytracer/primitives/Instance.h"
#include "raytracer/primitives/Composite.h"

Transformable::Transformable(Element* parent)
  : Element(parent),
    m_scale(Vector3d::one())
{
}

Matrix4d Transformable::localTransform() const {
  return Matrix4d::translate(position()) *
         Matrix3d::rotate(rotation()) *
         Matrix3d::scale(scale());
}

Matrix4d Transformable::globalTransform() const {
  Matrix4d parentTransform;
  if (Transformable* p = dynamic_cast<Transformable*>(parent())) {
    parentTransform = p->globalTransform();
  }
  return parentTransform * localTransform();
}

void Transformable::setMatrix(const Matrix4d& matrix) {
  setPosition(matrix.translationVector());
  setRotation(Matrix3d(matrix).rotationVector());
  setScale(Matrix3d(matrix).scaleVector());
}

bool Transformable::canHaveChild(Element* child) const {
  return dynamic_cast<Transformable*>(child) != nullptr;
}

void Transformable::leaveParent() {
  setMatrix(globalTransform());
}

void Transformable::joinParent() {
  Matrix4d matrix;
  if (Transformable* p = dynamic_cast<Transformable*>(parent())) {
    matrix = p->globalTransform().inverted();
  }
  
  setMatrix(matrix * localTransform());
}

#include "Transformable.moc"
