#include "world/objects/Transformable.h"
#include "world/objects/Group.h"
#include "world/objects/Light.h"
#include "world/objects/Material.h"
#include "world/objects/Surface.h"
#include "world/objects/TransformComposition.h"
#include "render/primitives/Instance.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Scene.h"

Transformable::Transformable(Element* parent)
    : Element(parent),
      m_scale(Vector3d::one) {
}

Matrix4d Transformable::localTransform() const {
  return composePositionRotationScale(position(), rotation(), scale());
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
  if (isGenerated())
    return;

  setMatrix(globalTransform());
}

void Transformable::joinParent() {
  if (isGenerated())
    return;

  Matrix4d matrix;
  if (Transformable* p = dynamic_cast<Transformable*>(parent())) {
    matrix = p->globalTransform().inverted();
  }

  setMatrix(matrix * localTransform());
}

void Transformable::moveBy(const Vector3d& vector, bool global) {
  Vector3d offset;
  if (global) {
    offset = Matrix3d(globalTransform().inverted() * localTransform()) * vector;
  } else {
    offset = Matrix3d(localTransform()) * vector;
  }
  setPosition(position() + offset);
}

void Transformable::addChildPrimitivesTo(render::Composite& composite, render::Scene* scene,
                                         const StepPlaybackStyle& style) const {
  for (const auto& child : childElements()) {
    if (Surface* surface = qobject_cast<Surface*>(child)) {
      auto primitive = surface->toRaytracer(scene, style);
      if (primitive)
        composite.add(primitive);
    } else if (Group* group = qobject_cast<Group*>(child)) {
      auto primitive = group->toRaytracer(scene, style);
      if (primitive)
        composite.add(primitive);
    } else if (Light* light = qobject_cast<Light*>(child)) {
      if (light->visible())
        scene->addLight(light->toRaytracer());
    }
  }
}
